#include "graph.h"

#include <unordered_map>
#include <utility>
#include <variant>

namespace graph {
namespace {

using ProducerMap = std::unordered_map<std::string, std::vector<int>>;

Literal make_literal(bool positive, const strips::GroundAtom &atom) {
  return Literal{positive, atom};
}

bool complementary(const Literal &a, const Literal &b) {
  return a.positive != b.positive && a.atom.predicate_id == b.atom.predicate_id &&
         a.atom.object_ids == b.atom.object_ids;
}

std::vector<Literal> formula_literals(const strips::Domain &domain,
                                      const std::unordered_map<std::string, strips::ObjectId> &object_ids,
                                      const strips::Formula &formula) {
  std::vector<std::pair<bool, strips::Atom>> raw;
  formula.collect_literals(raw);

  std::vector<Literal> out;
  out.reserve(raw.size());
  for (const auto &[positive, atom] : raw) {
    out.push_back(make_literal(positive, strips::ground_atom(domain, object_ids, atom)));
  }
  return out;
}

bool formula_holds(const strips::Domain &domain,
                   const std::unordered_map<std::string, strips::ObjectId> &object_ids,
                   const strips::Formula &formula,
                   const std::unordered_set<std::string> &literals) {
  if (std::holds_alternative<std::monostate>(formula.node)) {
    return true;
  }

  if (const auto *atom = std::get_if<strips::Atom>(&formula.node)) {
    return literals.contains(
        literal_key(make_literal(true, strips::ground_atom(domain, object_ids, *atom))));
  }

  if (const auto *negation = std::get_if<strips::Negation>(&formula.node)) {
    if (const auto *inner = std::get_if<strips::Atom>(&negation->operand->node)) {
      return literals.contains(
          literal_key(make_literal(false, strips::ground_atom(domain, object_ids, *inner))));
    }
    return false;
  }

  if (const auto *conjunction = std::get_if<strips::Conjunction>(&formula.node)) {
    for (const auto &operand : conjunction->operands) {
      if (!formula_holds(domain, object_ids, operand, literals)) {
        return false;
      }
    }
    return true;
  }

  const auto &disjunction = std::get<strips::Disjunction>(formula.node);
  for (const auto &operand : disjunction.operands) {
    if (formula_holds(domain, object_ids, operand, literals)) {
      return true;
    }
  }
  return disjunction.operands.empty();
}

bool fact_mutex(const FactLayer &layer, const Literal &a, const Literal &b) {
  return layer.mutexes.contains(mutex_key(literal_key(a), literal_key(b)));
}

bool preconditions_non_mutex(const strips::Domain &domain,
                             const std::unordered_map<std::string, strips::ObjectId> &object_ids,
                             const strips::Formula &formula, const FactLayer &layer) {
  if (!formula_holds(domain, object_ids, formula, layer.literal_set)) {
    return false;
  }

  const std::vector<Literal> literals = formula_literals(domain, object_ids, formula);
  for (std::size_t i = 0; i < literals.size(); ++i) {
    for (std::size_t j = i + 1; j < literals.size(); ++j) {
      if (fact_mutex(layer, literals[i], literals[j])) {
        return false;
      }
    }
  }
  return true;
}

bool inconsistent_effects(const ActionInstance &a, const ActionInstance &b) {
  for (const auto &effect_a : a.effects) {
    for (const auto &effect_b : b.effects) {
      if (complementary(effect_a, effect_b)) {
        return true;
      }
    }
  }
  return false;
}

bool interference(const ActionInstance &a, const ActionInstance &b) {
  for (const auto &effect_a : a.effects) {
    for (const auto &pre_b : b.preconditions) {
      if (complementary(effect_a, pre_b)) {
        return true;
      }
    }
  }
  for (const auto &effect_b : b.effects) {
    for (const auto &pre_a : a.preconditions) {
      if (complementary(effect_b, pre_a)) {
        return true;
      }
    }
  }
  return false;
}

bool competing_needs(const ActionInstance &a, const ActionInstance &b,
                     const FactLayer &layer) {
  for (const auto &pre_a : a.preconditions) {
    for (const auto &pre_b : b.preconditions) {
      if (fact_mutex(layer, pre_a, pre_b)) {
        return true;
      }
    }
  }
  return false;
}

ActionInstance make_noop(const Literal &literal) {
  ActionInstance noop;
  noop.preconditions.push_back(literal);
  noop.effects.push_back(literal);
  noop.is_noop = true;
  return noop;
}

} // namespace

std::string literal_key(const Literal &literal) {
  const std::string key = std::to_string(literal.atom.predicate_id) + ":" + [&]() {
    std::string s;
    for (std::size_t i = 0; i < literal.atom.object_ids.size(); ++i) {
      if (i) {
        s += ",";
      }
      s += std::to_string(literal.atom.object_ids[i]);
    }
    return s;
  }();
  return literal.positive ? key : "!" + key;
}

std::string mutex_key(const std::string &a, const std::string &b) {
  return a < b ? a + "||" + b : b + "||" + a;
}

std::string ActionInstance::pretty(const strips::Domain &domain,
                                   const std::vector<std::string> &objects) const {
  if (is_noop) {
    return effects.front().positive
               ? "noop " + strips::atom_name(domain, objects, effects.front().atom.predicate_id,
                                               effects.front().atom.object_ids, -1)
               : "noop !" + strips::atom_name(domain, objects, effects.front().atom.predicate_id,
                                                effects.front().atom.object_ids, -1);
  }

  std::string result = domain.actions[action_id].name;
  for (strips::ObjectId object_id : object_ids) {
    result += " " + objects[object_id];
  }
  return result;
}

PlanningGraph::PlanningGraph(const strips::Domain &domain,
                             const strips::Problem &problem)
    : domain_(domain), problem_(problem) {
  objects_ = domain.constants;
  objects_.insert(objects_.end(), problem.objects.begin(), problem.objects.end());
  for (strips::ObjectId object_id = 0; object_id < objects_.size(); ++object_id) {
    object_ids_.emplace(objects_[object_id], object_id);
  }

  std::unordered_set<std::string> init_true;
  for (const auto &atom : problem.init) {
    init_true.insert(literal_key(make_literal(true, strips::ground_atom(domain_, object_ids_, atom))));
  }

  FactLayer init;
  for (strips::PredicateId predicate_id = 0; predicate_id < domain_.predicates.size();
       ++predicate_id) {
    const auto &predicate = domain_.predicates[predicate_id];
    for (const auto &assignment : strips::all_object_assignments(objects_.size(), predicate.arity)) {
      strips::GroundAtom atom{predicate_id, assignment};
      const bool is_true = init_true.contains(literal_key(make_literal(true, atom)));
      init.literals.push_back(make_literal(is_true, atom));
      init.literal_set.insert(literal_key(init.literals.back()));
    }
  }

  for (std::size_t i = 0; i < init.literals.size(); ++i) {
    for (std::size_t j = i + 1; j < init.literals.size(); ++j) {
      if (complementary(init.literals[i], init.literals[j])) {
        init.mutexes.insert(
            mutex_key(literal_key(init.literals[i]), literal_key(init.literals[j])));
      }
    }
  }

  fact_layers_.push_back(std::move(init));
}

void PlanningGraph::extend() {
  const FactLayer &facts = fact_layers_.back();

  ActionLayer action_layer;
  for (const auto &literal : facts.literals) {
    action_layer.actions.push_back(make_noop(literal));
  }

  for (strips::ActionId action_id = 0; action_id < domain_.actions.size(); ++action_id) {
    const auto &action = domain_.actions[action_id];
    for (const auto &assignment : strips::all_object_assignments(objects_.size(), action.parameters.size())) {
      strips::GroundAction grounded =
          strips::ground_action(domain_, action_id, objects_, assignment);
      if (!preconditions_non_mutex(domain_, object_ids_, grounded.precondition, facts)) {
        continue;
      }

      ActionInstance instance;
      instance.action_id = action_id;
      instance.object_ids = assignment;
      instance.preconditions = formula_literals(domain_, object_ids_, grounded.precondition);
      instance.effects = formula_literals(domain_, object_ids_, grounded.effect);
      action_layer.actions.push_back(std::move(instance));
    }
  }

  for (std::size_t i = 0; i < action_layer.actions.size(); ++i) {
    for (std::size_t j = i + 1; j < action_layer.actions.size(); ++j) {
      const auto &a = action_layer.actions[i];
      const auto &b = action_layer.actions[j];
      if (inconsistent_effects(a, b) || interference(a, b) ||
          competing_needs(a, b, facts)) {
        action_layer.mutexes.insert(mutex_key(std::to_string(i), std::to_string(j)));
      }
    }
  }

  FactLayer next;
  ProducerMap producers;
  for (int i = 0; i < static_cast<int>(action_layer.actions.size()); ++i) {
    for (const auto &effect : action_layer.actions[i].effects) {
      const std::string key = literal_key(effect);
      if (!next.literal_set.contains(key)) {
        next.literal_set.insert(key);
        next.literals.push_back(effect);
      }
      producers[key].push_back(i);
    }
  }

  for (std::size_t i = 0; i < next.literals.size(); ++i) {
    for (std::size_t j = i + 1; j < next.literals.size(); ++j) {
      const auto &a = next.literals[i];
      const auto &b = next.literals[j];
      const std::string key_a = literal_key(a);
      const std::string key_b = literal_key(b);

      if (complementary(a, b)) {
        next.mutexes.insert(mutex_key(key_a, key_b));
        continue;
      }

      bool supported_by_non_mutex = false;
      for (int producer_a : producers[key_a]) {
        for (int producer_b : producers[key_b]) {
          if (!action_layer.mutexes.contains(
                  mutex_key(std::to_string(producer_a), std::to_string(producer_b)))) {
            supported_by_non_mutex = true;
            break;
          }
        }
        if (supported_by_non_mutex) {
          break;
        }
      }

      if (!supported_by_non_mutex) {
        next.mutexes.insert(mutex_key(key_a, key_b));
      }
    }
  }

  leveled_off_ = next.literal_set == facts.literal_set && next.mutexes == facts.mutexes;
  action_layers_.push_back(std::move(action_layer));
  fact_layers_.push_back(std::move(next));
}

void PlanningGraph::build_to_length(int n) {
  while (horizon() < n) {
    extend();
  }
}

void PlanningGraph::build_until_fixpoint() {
  while (!leveled_off_) {
    extend();
  }
}

int PlanningGraph::horizon() const { return static_cast<int>(action_layers_.size()); }

bool PlanningGraph::leveled_off() const { return leveled_off_; }

bool PlanningGraph::goal_reachable(int t) const {
  if (t < 0 || t >= static_cast<int>(fact_layers_.size())) {
    return false;
  }

  const FactLayer &layer = fact_layers_[t];
  return preconditions_non_mutex(domain_, object_ids_, problem_.goal, layer);
}

bool PlanningGraph::literals_mutex(const Literal &a, const Literal &b, int t) const {
  if (t < 0 || t >= static_cast<int>(fact_layers_.size())) {
    return false;
  }
  return fact_layers_[t].mutexes.contains(mutex_key(literal_key(a), literal_key(b)));
}

bool PlanningGraph::actions_mutex(int t, int i, int j) const {
  if (t < 0 || t >= static_cast<int>(action_layers_.size())) {
    return false;
  }
  return action_layers_[t].mutexes.contains(mutex_key(std::to_string(i), std::to_string(j)));
}

const std::vector<FactLayer> &PlanningGraph::fact_layers() const { return fact_layers_; }

const std::vector<ActionLayer> &PlanningGraph::action_layers() const {
  return action_layers_;
}

} // namespace graph
