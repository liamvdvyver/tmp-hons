#include "transition.h"

#include <unordered_set>

namespace {

std::string untimed_action_key(strips::ActionId action_id,
                               const std::vector<strips::ObjectId> &object_ids) {
  return std::to_string(action_id) + ":" + [&]() {
    std::string key;
    for (std::size_t i = 0; i < object_ids.size(); ++i) {
      if (i) {
        key += ",";
      }
      key += std::to_string(object_ids[i]);
    }
    return key;
  }();
}

std::string untimed_atom_key(const strips::GroundAtom &atom) {
  return std::to_string(atom.predicate_id) + ":" + [&]() {
    std::string key;
    for (std::size_t i = 0; i < atom.object_ids.size(); ++i) {
      if (i) {
        key += ",";
      }
      key += std::to_string(atom.object_ids[i]);
    }
    return key;
  }();
}

bool same_atom(const strips::GroundAtom &a, const strips::GroundAtom &b) {
  return a.predicate_id == b.predicate_id && a.object_ids == b.object_ids;
}

} // namespace

TransitionCache::TransitionCache(const strips::Domain &domain,
                                 const std::vector<std::string> &objects,
                                 bool partially_ordered)
    : domain_(domain), objects_(objects), partially_ordered_(partially_ordered) {
  std::unordered_map<std::string, std::size_t> atom_indices;
  std::unordered_map<std::string, strips::ObjectId> object_ids;
  for (strips::ObjectId object_id = 0; object_id < objects.size(); ++object_id) {
    object_ids.emplace(objects[object_id], object_id);
  }

  for (strips::PredicateId predicate_id = 0; predicate_id < domain.predicates.size();
       ++predicate_id) {
    const auto &predicate = domain.predicates[predicate_id];
    for (const auto &assignment : strips::all_object_assignments(objects.size(), predicate.arity)) {
      const std::string key = std::to_string(predicate_id) + ":" + [&]() {
        std::string s;
        for (std::size_t i = 0; i < assignment.size(); ++i) {
          if (i) {
            s += ",";
          }
          s += std::to_string(assignment[i]);
        }
        return s;
      }();
      atom_indices.emplace(key, atoms_.size());
      atoms_.push_back(GroundAtomTemplate{predicate_id, assignment});
      atom_explainers_.emplace_back();
    }
  }

  for (strips::ActionId action_id = 0; action_id < domain.actions.size(); ++action_id) {
    const auto &action = domain.actions[action_id];
    for (const auto &assignment : strips::all_object_assignments(objects.size(), action.parameters.size())) {
      GroundActionTemplate entry;
      entry.action = strips::ground_action(domain, action_id, objects, assignment);

      std::vector<std::pair<bool, strips::Atom>> preconditions;
      entry.action.precondition.collect_literals(preconditions);
      for (const auto &[positive, atom] : preconditions) {
        entry.preconditions.push_back(
            strips::ground_literal(domain, object_ids, positive, atom));
      }

      std::vector<std::pair<bool, strips::Atom>> effects;
      entry.action.effect.collect_literals(effects);
      for (const auto &[positive, atom] : effects) {
        entry.effects.push_back(strips::ground_literal(domain, object_ids, positive, atom));
        const auto it = atom_indices.find(untimed_atom_key(entry.effects.back().atom));
        if (it != atom_indices.end()) {
          atom_explainers_[it->second].push_back(actions_.size());
        }
      }

      actions_.push_back(std::move(entry));
    }
  }

  action_conflicts_.resize(actions_.size());
  if (partially_ordered_) {
    for (std::size_t i = 0; i < actions_.size(); ++i) {
      std::unordered_set<std::size_t> conflicts;
      for (const auto &required : actions_[i].preconditions) {
        if (!required.positive) {
          continue;
        }
        for (std::size_t j = 0; j < actions_.size(); ++j) {
          if (i == j) {
            continue;
          }
          for (const auto &effect : actions_[j].effects) {
            if (!effect.positive && same_atom(effect.atom, required.atom)) {
              conflicts.insert(j);
              break;
            }
          }
        }
      }
      action_conflicts_[i] = std::vector<std::size_t>(conflicts.begin(), conflicts.end());
    }
  }
}

ActionMap TransitionCache::add_transition(
    z3::context &ctx, z3::solver &solver, int t,
    const graph::ActionLayer *graph_action_layer) const {
  ActionMap action_map;
  std::unordered_map<std::string, std::size_t> graph_action_indices;

  if (graph_action_layer != nullptr) {
    for (std::size_t i = 0; i < graph_action_layer->actions.size(); ++i) {
      const auto &action = graph_action_layer->actions[i];
      if (!action.is_noop) {
        graph_action_indices.emplace(
            untimed_action_key(action.action_id, action.object_ids), i);
      }
    }
  }

  for (const auto &entry : actions_) {
    const std::string action_var_name = entry.action.timed_name(domain_, objects_, t);
    z3::expr action_var = ctx.bool_const(action_var_name.c_str());

    solver.add(z3::implies(action_var,
                           entry.action.precondition.to_z3(ctx, t) &&
                               entry.action.effect.to_z3(ctx, t + 1)));

    if (graph_action_layer != nullptr &&
        !graph_action_indices.contains(
            untimed_action_key(entry.action.action_id, entry.action.object_ids))) {
      solver.add(!action_var);
    }

    action_map.emplace(action_var_name, entry.action);
  }

  for (std::size_t atom_index = 0; atom_index < atoms_.size(); ++atom_index) {
    const auto &atom = atoms_[atom_index];
    z3::expr p_t = ctx.bool_const(
        strips::atom_name(domain_, objects_, atom.predicate_id, atom.object_ids, t).c_str());
    z3::expr p_tp1 = ctx.bool_const(
        strips::atom_name(domain_, objects_, atom.predicate_id, atom.object_ids, t + 1).c_str());

    z3::expr_vector explainers(ctx);
    for (std::size_t action_index : atom_explainers_[atom_index]) {
      explainers.push_back(
          ctx.bool_const(actions_[action_index].action.timed_name(domain_, objects_, t).c_str()));
    }

    explainers.push_back(p_tp1 == p_t);
    solver.add(z3::mk_or(explainers));
  }

  z3::expr_vector all(ctx);
  for (const auto &entry : actions_) {
    all.push_back(ctx.bool_const(entry.action.timed_name(domain_, objects_, t).c_str()));
  }

  if (graph_action_layer != nullptr) {
    for (std::size_t i = 0; i < actions_.size(); ++i) {
      const auto it_i = graph_action_indices.find(
          untimed_action_key(actions_[i].action.action_id, actions_[i].action.object_ids));
      if (it_i == graph_action_indices.end()) {
        continue;
      }

      const z3::expr current =
          ctx.bool_const(actions_[i].action.timed_name(domain_, objects_, t).c_str());
      for (std::size_t j = i + 1; j < actions_.size(); ++j) {
        const auto it_j = graph_action_indices.find(
            untimed_action_key(actions_[j].action.action_id, actions_[j].action.object_ids));
        if (it_j == graph_action_indices.end()) {
          continue;
        }
        if (graph_action_layer->mutexes.contains(
                graph::mutex_key(std::to_string(it_i->second), std::to_string(it_j->second)))) {
          solver.add(z3::implies(
              current, !ctx.bool_const(
                           actions_[j].action.timed_name(domain_, objects_, t).c_str())));
        }
      }
    }
  } else if (partially_ordered_) {
    for (std::size_t i = 0; i < actions_.size(); ++i) {
      const z3::expr current =
          ctx.bool_const(actions_[i].action.timed_name(domain_, objects_, t).c_str());
      for (std::size_t other_index : action_conflicts_[i]) {
        solver.add(z3::implies(
            current, !ctx.bool_const(
                         actions_[other_index].action.timed_name(domain_, objects_, t).c_str())));
      }
    }
  } else {
    for (std::size_t i = 0; i < actions_.size(); ++i) {
      z3::expr current =
          ctx.bool_const(actions_[i].action.timed_name(domain_, objects_, t).c_str());
      z3::expr_vector others(ctx);
      for (std::size_t j = 0; j < actions_.size(); ++j) {
        if (i != j) {
          others.push_back(
              ctx.bool_const(actions_[j].action.timed_name(domain_, objects_, t).c_str()));
        }
      }
      if (!others.empty()) {
        solver.add(z3::implies(current, !z3::mk_or(others)));
      }
    }
  }

  solver.add(z3::mk_or(all));
  return action_map;
}
