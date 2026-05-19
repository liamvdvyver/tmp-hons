#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>
#include <z3++.h>

#include "strips.h"

using json = nlohmann::json;

using ActionMap = std::unordered_map<std::string, strips::GroundAction>;

void add_init(z3::context &ctx, z3::solver &solver, const strips::Domain &domain,
              const strips::Problem &problem,
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

void add_goal(z3::context &ctx, z3::solver &solver, const strips::Formula &goal,
              int t) {
  solver.add(goal.to_z3(ctx, t));
}

ActionMap add_transition(z3::context &ctx, z3::solver &solver,
                         const strips::Domain &domain,
                         const std::vector<std::string> &objects, int t) {
  ActionMap action_map;
  std::vector<strips::GroundAction> grounded_actions;

  for (const auto &action : domain.actions) {
    for (const auto &arguments : strips::all_params(objects, action.parameters.size())) {
      strips::GroundAction grounded = strips::ground_action(action, arguments);
      const std::string action_var_name = grounded.timed_name(t);
      z3::expr action_var = ctx.bool_const(action_var_name.c_str());

      solver.add(z3::implies(action_var,
                             grounded.precondition.to_z3(ctx, t) &&
                                 grounded.effect.to_z3(ctx, t + 1)));

      action_map.emplace(action_var_name, grounded);
      grounded_actions.push_back(std::move(grounded));
    }
  }

  for (const auto &predicate : domain.predicates) {
    for (const auto &arguments : strips::all_params(objects, predicate.arity)) {
      z3::expr p_t = ctx.bool_const(strips::atom_name(predicate.name, arguments, t).c_str());
      z3::expr p_tp1 =
          ctx.bool_const(strips::atom_name(predicate.name, arguments, t + 1).c_str());

      z3::expr_vector explainers(ctx);

      for (const auto &grounded : grounded_actions) {
        std::vector<std::pair<bool, strips::Atom>> effects;
        grounded.effect.collect_literals(effects);

        bool affects_atom = false;
        for (const auto &[_, atom] : effects) {
          if (atom.name == predicate.name && atom.arguments == arguments) {
            affects_atom = true;
            break;
          }
        }

        if (affects_atom) {
          explainers.push_back(ctx.bool_const(grounded.timed_name(t).c_str()));
        }
      }

      explainers.push_back(p_tp1 == p_t);
      solver.add(z3::mk_or(explainers));
    }
  }

  std::vector<std::string> keys;
  keys.reserve(action_map.size());
  for (const auto &[name, _] : action_map) {
    keys.push_back(name);
  }

  for (std::size_t i = 0; i < keys.size(); ++i) {
    z3::expr current = ctx.bool_const(keys[i].c_str());
    z3::expr_vector others(ctx);

    for (std::size_t j = 0; j < keys.size(); ++j) {
      if (i != j) {
        others.push_back(ctx.bool_const(keys[j].c_str()));
      }
    }

    if (!others.empty()) {
      solver.add(z3::implies(current, !z3::mk_or(others)));
    }
  }

  return action_map;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: planner input.json\n";
    return 1;
  }

  std::ifstream file(argv[1]);
  json data;
  file >> data;

  const strips::Domain domain = strips::Domain::parse(data["domain"]);
  const strips::Problem problem = strips::Problem::parse(data["problem"]);

  std::vector<std::string> objects = domain.constants;
  objects.insert(objects.end(), problem.objects.begin(), problem.objects.end());

  z3::context ctx;
  z3::solver solver(ctx);

  add_init(ctx, solver, domain, problem, objects);

  int horizon = 0;

  solver.push();
  add_goal(ctx, solver, problem.goal, 0);

  if (solver.check() == z3::sat) {
    std::ofstream out("sas_plan");
    out << '\n';
    return 0;
  }

  std::vector<ActionMap> action_maps;

  while (true) {
    ++horizon;

    solver.pop();
    action_maps.push_back(add_transition(ctx, solver, domain, objects, horizon - 1));

    solver.push();
    add_goal(ctx, solver, problem.goal, horizon);

    if (solver.check() == z3::sat) {
      break;
    }
  }

  z3::model model = solver.get_model();
  std::vector<std::string> plan(horizon);

  for (int t = 0; t < horizon; ++t) {
    for (const auto &[name, action] : action_maps[t]) {
      z3::expr value = ctx.bool_const(name.c_str());
      if (model.eval(value).bool_value() == Z3_L_TRUE) {
        plan[t] = action.pretty();
      }
    }
  }

  std::ofstream out("sas_plan");
  for (const auto &step : plan) {
    out << '(' << step << ")\n";
  }

  return 0;
}
