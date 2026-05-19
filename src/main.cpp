#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>
#include <z3++.h>

#include "graph.h"
#include "strips.h"
#include "transition.h"

using json = nlohmann::json;

using Plan = std::vector<std::vector<strips::GroundAction>>;

// Encode the initial state as Boolean facts at time 0.
void add_init(z3::context &ctx, z3::solver &solver,
              const strips::Domain &domain, const strips::Problem &problem,
              const std::vector<std::string> &objects) {
  std::unordered_set<std::string> init_atoms;

  for (const auto &atom : problem.init) {
    init_atoms.insert(strips::atom_name(atom.name, atom.arguments, 0));
  }

  for (const auto &predicate : domain.predicates) {
    for (const auto &arguments : strips::all_params(objects, predicate.arity)) {
      const std::string atom = strips::atom_name(predicate.name, arguments, 0);
      z3::expr var = ctx.bool_const(atom.c_str());
      solver.add(init_atoms.count(atom) ? var : !var);
    }
  }
}

// Add the goal condition at a specific time step.
void add_goal(z3::context &ctx, z3::solver &solver, const strips::Formula &goal,
              int t) {
  solver.add(goal.to_z3(ctx, t));
}

int main(int argc, char **argv) {
  bool partially_ordered = false;
  bool graph_plan = false;
  bool auto_accept = false;
  bool json_out = false;
  std::string input_path;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--partially-ordered" || arg == "-p") {
      std::cerr << "searching for partially ordered\n";
      partially_ordered = true;
    } else if (arg == "--graph-plan" || arg == "-g") {
      std::cerr << "searching with graph-plan mutex propagation\n";
      graph_plan = true;
    } else if (arg == "-y") {
      auto_accept = true;
    } else if (arg == "--json-out" || arg == "-j") {
      json_out = true;
    } else {
      input_path = arg;
    }
  }

  if (input_path.empty()) {
    std::cerr << "Usage: planner [--partially-ordered|-p] [--graph-plan|-g] [-y] [--json-out|-j] input.json\n";
    return 1;
  }

  std::ifstream file(input_path);
  json data;
  file >> data;

  const strips::Domain domain = strips::Domain::parse(data["domain"]);
  const strips::Problem problem = strips::Problem::parse(data["problem"]);

  std::vector<std::string> objects = domain.constants;
  objects.insert(objects.end(), problem.objects.begin(), problem.objects.end());

  z3::context ctx;
  z3::solver solver(ctx);
  TransitionCache transition_cache(domain, objects, partially_ordered);
  std::optional<graph::PlanningGraph> planning_graph;
  if (graph_plan) {
    planning_graph.emplace(domain, problem);
  }

  add_init(ctx, solver, domain, problem, objects);

  std::vector<ActionMap> action_maps;

  auto action_to_json = [&](const strips::GroundAction &action) {
    json action_json;
    action_json["name"] = domain.actions[action.action_id].name;
    action_json["arguments"] = json::array();
    for (strips::ObjectId object_id : action.object_ids) {
      action_json["arguments"].push_back(objects[object_id]);
    }
    return action_json;
  };

  auto plan_to_json = [&](const Plan &plan) {
    json plan_json;
    plan_json["plan"] = json::array();
    for (const auto &parallel_step : plan) {
      json step_json = json::array();
      for (const auto &action : parallel_step) {
        step_json.push_back(action_to_json(action));
      }
      plan_json["plan"].push_back(step_json);
    }
    return plan_json;
  };

  // Read the chosen action variables back out of a satisfying model.
  auto build_plan = [&](const z3::model &model, int h) {
    Plan plan(h);
    for (int t = 0; t < h; ++t) {
      for (const auto &[name, action] : action_maps[t]) {
        z3::expr value = ctx.bool_const(name.c_str());
        if (model.eval(value).bool_value() == Z3_L_TRUE) {
          plan[t].push_back(action);
        }
      }
    }
    return plan;
  };

  // Print a candidate plan to stdout.
  auto print_plan = [&](const Plan &plan) {
    if (json_out) {
      std::cout << plan_to_json(plan).dump(2) << '\n';
      std::cout.flush();
      return;
    }

    for (const auto &parallel_step : plan) {
      for (const auto &action : parallel_step) {
        std::cout << '(' << action.pretty(domain, objects) << ")\n";
      }
      if (parallel_step.size() > 1) {
        std::cout << "---\n";
      }
    }
    std::cout.flush();
  };

  auto write_plan = [&](const Plan &plan) {
    std::ofstream out("sas_plan");
    if (json_out) {
      out << plan_to_json(plan).dump(2) << '\n';
      return;
    }

    for (const auto &parallel_step : plan) {
      for (const auto &action : parallel_step) {
        out << '(' << action.pretty(domain, objects) << ")\n";
      }
      if (parallel_step.size() > 1) {
        out << "---\n";
      }
    }
  };

  // Exclude exactly the current plan and keep searching at the same horizon.
  auto block_plan = [&](const Plan &plan, int h) {
    z3::expr_vector disj(ctx);
    for (int t = 0; t < h; ++t) {
      for (const auto &[name, action] : action_maps[t]) {
        bool chosen = false;
        for (const auto &chosen_action : plan[t]) {
          if (action.action_id == chosen_action.action_id &&
              action.object_ids == chosen_action.object_ids) {
            chosen = true;
            break;
          }
        }
        if (chosen) {
          disj.push_back(!ctx.bool_const(name.c_str()));
        }
      }
    }
    if (!disj.empty()) {
      solver.add(z3::mk_or(disj));
    }
  };

  // Debug helper: dump the current solver assertions.
  auto print_constraints = [&]() {
    z3::expr_vector assertions = solver.assertions();
    for (unsigned i = 0; i < assertions.size(); ++i) {
      std::cout << assertions[i] << "\n";
    }
    std::cout.flush();
  };

  // Current length bound. We grow it only when the current horizon is unsat.
  int horizon = 0;
  solver.push();
  add_goal(ctx, solver, problem.goal, horizon);

  while (true) {

    // If there is no plan at the current bound, increase the horizon.
    while (solver.check() != z3::sat) {
      solver.pop();
      ++horizon;
      std::cerr << "h:" << horizon;
      std::cerr << ", transition";
      const graph::ActionLayer *graph_action_layer = nullptr;
      if (planning_graph.has_value()) {
        planning_graph->build_to_length(horizon);
        graph_action_layer = &planning_graph->action_layers()[horizon - 1];
      }
      action_maps.push_back(
          transition_cache.add_transition(ctx, solver, horizon - 1, graph_action_layer));
      solver.push();
      std::cerr << ", goal";
      add_goal(ctx, solver, problem.goal, horizon);
      std::cerr << '\n';
    }

    z3::model model = solver.get_model();
    Plan plan = build_plan(model, horizon);
    print_plan(plan);

    if (auto_accept) {
      write_plan(plan);
      return 0;
    }

    std::string command;

    // Keep offering the same plan until the user accepts it or rejects it.
    while (command != "n") {
      std::cout << horizon << ":y/n/p> ";
      std::cout.flush();
      if (!std::getline(std::cin, command)) {
        return 1;
      }

      if (command == "y") {
        write_plan(plan);
        return 0;
      }

      if (command == "p") {
        print_constraints();
      }
    }

    block_plan(plan, horizon);
  }
}
