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

struct Atom {
  std::string name;
  std::vector<std::string> arguments;
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

struct GroundAction {
  std::string name;
  std::vector<std::string> arguments;
  Formula precondition;
  Formula effect;

  std::string timed_name(int t) const;
  std::string pretty() const;
};

struct Domain {
  std::vector<PredicateSchema> predicates;
  std::vector<ActionSchema> actions;
  std::vector<std::string> constants;

  static Domain parse(const json &data);
};

struct Problem {
  std::vector<std::string> objects;
  std::vector<Atom> init;
  Formula goal;

  static Problem parse(const json &data);
};

std::string atom_name(const std::string &predicate,
                      const std::vector<std::string> &arguments, int t);
std::string action_name(const std::string &action,
                        const std::vector<std::string> &arguments, int t);
std::vector<std::vector<std::string>> all_params(const std::vector<std::string> &objects,
                                                 int arity);
GroundAction ground_action(const ActionSchema &action, const std::vector<std::string> &arguments);

} // namespace strips
