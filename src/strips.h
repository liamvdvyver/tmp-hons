#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>
#include <z3++.h>

namespace strips {

using json = nlohmann::json;
using PredicateId = std::size_t;
using ActionId = std::size_t;
using ObjectId = std::size_t;

struct Atom {
  std::string name;
  std::vector<std::string> arguments;
};

struct GroundAtom {
  PredicateId predicate_id;
  std::vector<ObjectId> object_ids;
};

struct GroundLiteral {
  bool positive;
  GroundAtom atom;
};

struct Formula;

struct Negation {
  std::shared_ptr<Formula> operand;
};

struct Conjunction {
  std::vector<Formula> operands;
};

struct Disjunction {
  std::vector<Formula> operands;
};

struct Formula {
  using Node = std::variant<std::monostate, Atom, Negation, Conjunction, Disjunction>;

  Node node;

  static Formula parse(const json &data);
  Formula substitute(const std::unordered_map<std::string, std::string> &bindings) const;
  z3::expr to_z3(z3::context &ctx, int t) const;
  void collect_literals(std::vector<std::pair<bool, Atom>> &literals) const;
};

struct PredicateSchema {
  std::string name;
  std::size_t arity;
};

struct ActionSchema {
  std::string name;
  std::vector<std::string> parameters;
  Formula precondition;
  Formula effect;
};

struct Domain {
  std::vector<PredicateSchema> predicates;
  std::vector<ActionSchema> actions;
  std::vector<std::string> constants;
  std::unordered_map<std::string, PredicateId> predicate_ids;
  std::unordered_map<std::string, ActionId> action_ids;

  static Domain parse(const json &data);
};

struct Problem {
  std::vector<std::string> objects;
  std::vector<Atom> init;
  Formula goal;
  std::unordered_map<std::string, ObjectId> object_ids;

  static Problem parse(const json &data);
};

struct GroundAction {
  ActionId action_id;
  std::vector<ObjectId> object_ids;
  Formula precondition;
  Formula effect;

  std::string timed_name(const Domain &domain,
                         const std::vector<std::string> &objects, int t) const;
  std::string pretty(const Domain &domain,
                     const std::vector<std::string> &objects) const;
};

std::string atom_name(const std::string &predicate,
                      const std::vector<std::string> &arguments, int t);
std::string action_name(const std::string &action,
                        const std::vector<std::string> &arguments, int t);
std::string atom_name(const Domain &domain, const std::vector<std::string> &objects,
                      PredicateId predicate_id,
                      const std::vector<ObjectId> &object_ids, int t);
std::string action_name(const Domain &domain, const std::vector<std::string> &objects,
                        ActionId action_id, const std::vector<ObjectId> &object_ids,
                        int t);
std::vector<std::vector<std::string>> all_params(const std::vector<std::string> &objects,
                                                 int arity);
std::vector<std::vector<ObjectId>> all_object_assignments(std::size_t object_count,
                                                          int arity);
GroundAtom ground_atom(const Domain &domain, const std::unordered_map<std::string, ObjectId> &object_ids,
                       const Atom &atom);
GroundLiteral ground_literal(const Domain &domain,
                             const std::unordered_map<std::string, ObjectId> &object_ids,
                             bool positive, const Atom &atom);
GroundAction ground_action(const Domain &domain, ActionId action_id,
                           const std::vector<std::string> &objects,
                           const std::vector<ObjectId> &object_ids);

} // namespace strips
