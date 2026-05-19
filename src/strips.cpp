#include "strips.h"

#include <functional>

namespace strips {

namespace {

std::vector<std::string> parse_string_array(const json &data) {
  if (data.is_null()) {
    return {};
  }
  return data.get<std::vector<std::string>>();
}

Atom parse_atom(const json &data) {
  return Atom{data["pred"][0].get<std::string>(),
              data["pred"][1].get<std::vector<std::string>>()};
}

} // namespace

std::string atom_name(const std::string &predicate,
                      const std::vector<std::string> &arguments, int t) {
  std::string result = predicate + "(";
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    result += arguments[i];
    if (i + 1 < arguments.size()) {
      result += ",";
    }
  }
  result += ")@" + std::to_string(t);
  return result;
}

std::string action_name(const std::string &action,
                        const std::vector<std::string> &arguments, int t) {
  std::string result = action + "(";
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    result += arguments[i];
    if (i + 1 < arguments.size()) {
      result += ",";
    }
  }
  result += ")@" + std::to_string(t);
  return result;
}

Formula Formula::parse(const json &data) {
  if (data.is_null() || data.empty()) {
    return Formula{};
  }

  if (data.contains("pred")) {
    return Formula{parse_atom(data)};
  }

  if (data.contains("not")) {
    return Formula{Negation{std::make_shared<Formula>(parse(data["not"]))}};
  }

  if (data.contains("and")) {
    Conjunction conjunction;
    for (const auto &operand : data["and"]) {
      conjunction.operands.push_back(parse(operand));
    }
    return Formula{std::move(conjunction)};
  }

  if (data.contains("or")) {
    Disjunction disjunction;
    for (const auto &operand : data["or"]) {
      disjunction.operands.push_back(parse(operand));
    }
    return Formula{std::move(disjunction)};
  }

  return Formula{};
}

Formula Formula::substitute(const std::unordered_map<std::string, std::string> &bindings) const {
  if (std::holds_alternative<std::monostate>(node)) {
    return Formula{};
  }

  if (const auto *atom = std::get_if<Atom>(&node)) {
    Atom grounded = *atom;
    for (auto &argument : grounded.arguments) {
      if (const auto it = bindings.find(argument); it != bindings.end()) {
        argument = it->second;
      }
    }
    return Formula{std::move(grounded)};
  }

  if (const auto *negation = std::get_if<Negation>(&node)) {
    return Formula{Negation{std::make_shared<Formula>(negation->operand->substitute(bindings))}};
  }

  if (const auto *conjunction = std::get_if<Conjunction>(&node)) {
    Conjunction grounded;
    for (const auto &operand : conjunction->operands) {
      grounded.operands.push_back(operand.substitute(bindings));
    }
    return Formula{std::move(grounded)};
  }

  Disjunction grounded;
  for (const auto &operand : std::get<Disjunction>(node).operands) {
    grounded.operands.push_back(operand.substitute(bindings));
  }
  return Formula{std::move(grounded)};
}

z3::expr Formula::to_z3(z3::context &ctx, int t) const {
  if (std::holds_alternative<std::monostate>(node)) {
    return ctx.bool_val(true);
  }

  if (const auto *atom = std::get_if<Atom>(&node)) {
    return ctx.bool_const(atom_name(atom->name, atom->arguments, t).c_str());
  }

  if (const auto *negation = std::get_if<Negation>(&node)) {
    return !negation->operand->to_z3(ctx, t);
  }

  if (const auto *conjunction = std::get_if<Conjunction>(&node)) {
    z3::expr_vector operands(ctx);
    for (const auto &operand : conjunction->operands) {
      operands.push_back(operand.to_z3(ctx, t));
    }
    return z3::mk_and(operands);
  }

  z3::expr_vector operands(ctx);
  for (const auto &operand : std::get<Disjunction>(node).operands) {
    operands.push_back(operand.to_z3(ctx, t));
  }
  return z3::mk_or(operands);
}

void Formula::collect_literals(std::vector<std::pair<bool, Atom>> &literals) const {
  if (const auto *atom = std::get_if<Atom>(&node)) {
    literals.emplace_back(true, *atom);
    return;
  }

  if (const auto *negation = std::get_if<Negation>(&node)) {
    if (const auto *inner = std::get_if<Atom>(&negation->operand->node)) {
      literals.emplace_back(false, *inner);
    }
    return;
  }

  if (const auto *conjunction = std::get_if<Conjunction>(&node)) {
    for (const auto &operand : conjunction->operands) {
      operand.collect_literals(literals);
    }
    return;
  }

  if (const auto *disjunction = std::get_if<Disjunction>(&node)) {
    for (const auto &operand : disjunction->operands) {
      operand.collect_literals(literals);
    }
  }
}

std::vector<std::vector<std::string>> all_params(const std::vector<std::string> &objects,
                                                 int arity) {
  std::vector<std::vector<std::string>> result;

  std::function<void(std::vector<std::string> &)> generate =
      [&](std::vector<std::string> &current) {
        if (static_cast<int>(current.size()) == arity) {
          result.push_back(current);
          return;
        }
        for (const auto &object : objects) {
          current.push_back(object);
          generate(current);
          current.pop_back();
        }
      };

  std::vector<std::string> current;
  generate(current);
  return result;
}

GroundAction ground_action(const ActionSchema &action, const std::vector<std::string> &arguments) {
  std::unordered_map<std::string, std::string> bindings;
  for (std::size_t i = 0; i < action.parameters.size(); ++i) {
    bindings[action.parameters[i]] = arguments[i];
  }

  return GroundAction{action.name,
                      arguments,
                      action.precondition.substitute(bindings),
                      action.effect.substitute(bindings)};
}

std::string GroundAction::timed_name(int t) const {
  return action_name(name, arguments, t);
}

std::string GroundAction::pretty() const {
  std::string result = name;
  for (const auto &argument : arguments) {
    result += " " + argument;
  }
  return result;
}

Domain Domain::parse(const json &data) {
  Domain domain;

  if (data.contains("constants")) {
    domain.constants = parse_string_array(data["constants"]);
  }

  for (const auto &[name, parameters] : data["predicates"].items()) {
    domain.predicates.push_back(PredicateSchema{name, parameters.size()});
  }

  for (const auto &[name, action_data] : data["actions"].items()) {
    domain.actions.push_back(ActionSchema{
        name,
        action_data["terms"].get<std::vector<std::string>>(),
        Formula::parse(action_data["pre"]),
        Formula::parse(action_data["eff"]),
    });
  }

  return domain;
}

Problem Problem::parse(const json &data) {
  Problem problem;
  problem.objects = parse_string_array(data["objects"]);
  problem.goal = Formula::parse(data["goal"]);

  for (const auto &atom_data : data["init"]) {
    problem.init.push_back(parse_atom(atom_data));
  }

  return problem;
}

} // namespace strips
