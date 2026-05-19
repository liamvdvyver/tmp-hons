#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <z3++.h>

#include "logic.h"

using json = nlohmann::json;
using namespace std;

/* =========================
   Helpers
   ========================= */

string atom_name(const string &pred, const vector<string> &args, int t) {
  string s = pred + "(";
  for (size_t i = 0; i < args.size(); i++) {
    s += args[i];
    if (i + 1 < args.size())
      s += ",";
  }
  s += ")@" + to_string(t);
  return s;
}

string action_name(const string &name, const vector<string> &args, int t) {
  string s = name + "(";
  for (size_t i = 0; i < args.size(); i++) {
    s += args[i];
    if (i + 1 < args.size())
      s += ",";
  }
  s += ")@" + to_string(t);
  return s;
}

/* =========================
   Formula -> Z3
   ========================= */

z3::expr formula_to_z3(z3::context &ctx, const json &f, int t) {
  if (f.contains("pred")) {
    string name = f["pred"][0];
    vector<string> args = f["pred"][1].get<vector<string>>();
    return ctx.bool_const(atom_name(name, args, t).c_str());
  }

  if (f.contains("not")) {
    return !formula_to_z3(ctx, f["not"], t);
  }

  if (f.contains("and")) {
    z3::expr_vector terms(ctx);
    for (auto &x : f["and"])
      terms.push_back(formula_to_z3(ctx, x, t));
    return z3::mk_and(terms);
  }

  if (f.contains("or")) {
    z3::expr_vector terms(ctx);
    for (auto &x : f["or"])
      terms.push_back(formula_to_z3(ctx, x, t));
    return z3::mk_or(terms);
  }

  // empty
  return ctx.bool_val(true);
}

/* =========================
   Grounding utilities
   ========================= */

vector<vector<string>> all_params(const vector<string> &objects, int arity) {
  vector<vector<string>> result;

  function<void(vector<string> &)> gen = [&](vector<string> &cur) {
    if ((int)cur.size() == arity) {
      result.push_back(cur);
      return;
    }
    for (auto &o : objects) {
      cur.push_back(o);
      gen(cur);
      cur.pop_back();
    }
  };

  vector<string> tmp;
  gen(tmp);
  return result;
}

/* =========================
   Instantiate formula
   ========================= */

json substitute(const json &f, const vector<string> &params,
                const vector<string> &args) {

  if (f.contains("pred")) {
    string name = f["pred"][0];
    vector<string> terms = f["pred"][1];

    vector<string> new_terms;
    for (auto &t : terms) {
      bool replaced = false;
      for (size_t i = 0; i < params.size(); i++) {
        if (t == params[i]) {
          new_terms.push_back(args[i]);
          replaced = true;
          break;
        }
      }
      if (!replaced)
        new_terms.push_back(t);
    }

    return json{{"pred", {name, new_terms}}};
  }

  if (f.contains("not")) {
    return json{{"not", substitute(f["not"], params, args)}};
  }

  if (f.contains("and")) {
    vector<json> xs;
    for (auto &x : f["and"])
      xs.push_back(substitute(x, params, args));
    return json{{"and", xs}};
  }

  if (f.contains("or")) {
    vector<json> xs;
    for (auto &x : f["or"])
      xs.push_back(substitute(x, params, args));
    return json{{"or", xs}};
  }

  return json{};
}

/* =========================
   Initial state
   ========================= */

void add_init(z3::context &ctx, z3::solver &s, const json &domain,
              const json &problem, const vector<string> &objects) {

  unordered_set<string> init_atoms;

  for (auto &f : problem["init"]) {
    string name = f["pred"][0];
    vector<string> args = f["pred"][1];
    init_atoms.insert(atom_name(name, args, 0));
  }

  for (auto &[pred, params] : domain["predicates"].items()) {
    int arity = params.size();

    for (auto &args : all_params(objects, arity)) {
      string a = atom_name(pred, args, 0);
      z3::expr var = ctx.bool_const(a.c_str());

      if (init_atoms.count(a))
        s.add(var);
      else
        s.add(!var);
    }
  }
}

/* =========================
   Goal
   ========================= */

void add_goal(z3::context &ctx, z3::solver &s, const json &goal, int t) {
  s.add(formula_to_z3(ctx, goal, t));
}

/* =========================
   Transition
   ========================= */

unordered_map<string, pair<string, vector<string>>>
add_transition(z3::context &ctx, z3::solver &s, const json &domain,
               const vector<string> &objects, int t) {

  unordered_map<string, pair<string, vector<string>>> action_map;

  for (auto &[name, act] : domain["actions"].items()) {
    vector<string> params = act["terms"];
    int arity = params.size();

    for (auto &args : all_params(objects, arity)) {

      string a_name = action_name(name, args, t);
      z3::expr a = ctx.bool_const(a_name.c_str());

      json pre = substitute(act["pre"], params, args);
      json eff = substitute(act["eff"], params, args);

      z3::expr pre_z3 = formula_to_z3(ctx, pre, t);
      z3::expr eff_z3 = formula_to_z3(ctx, eff, t + 1);

      s.add(z3::implies(a, pre_z3 && eff_z3));

      action_map[a_name] = {name, args};
    }
  }

  /* === Frame axioms (your compact version) === */

/* === Frame axioms with inverse effects === */

// Precompute: which actions add/remove which predicates (ungrounded)
unordered_map<string, vector<string>> adds;  // pred -> actions
unordered_map<string, vector<string>> dels;

for (auto &[act_name, act] : domain["actions"].items()) {
    if (!act.contains("eff")) continue;

    auto eff = act["eff"];

    // normalize: assume {"and": [...]} or single literal
    vector<json> effects;
    if (eff.contains("and")) {
        for (auto &e : eff["and"]) effects.push_back(e);
    } else {
        effects.push_back(eff);
    }

    for (auto &e : effects) {
        if (e.contains("not")) {
            auto p = e["not"]["pred"][0];
            dels[p].push_back(act_name);
        } else if (e.contains("pred")) {
            auto p = e["pred"][0];
            adds[p].push_back(act_name);
        }
    }
}

/* Now generate frame axioms */

for (auto &[pred, params] : domain["predicates"].items()) {
    int arity = params.size();

    for (auto &args : all_params(objects, arity)) {

        z3::expr p_t =
            ctx.bool_const(atom_name(pred, args, t).c_str());
        z3::expr p_tp1 =
            ctx.bool_const(atom_name(pred, args, t + 1).c_str());

        z3::expr_vector explainers(ctx);

        // --- ADD actions ---
        for (auto &act_name : adds[pred]) {
            auto &act = domain["actions"][act_name];
            vector<string> act_params = act["terms"];

            // match parameters by name (like Python partial map)
            for (auto &act_args : all_params(objects, act_params.size())) {

                bool consistent = true;

                // build mapping param -> object
                unordered_map<string, string> assign;
                for (size_t i = 0; i < act_params.size(); i++)
                    assign[act_params[i]] = act_args[i];

                // check if effect matches this predicate instance
                auto eff = act["eff"];
                vector<json> effects;
                if (eff.contains("and")) {
                    for (auto &e : eff["and"]) effects.push_back(e);
                } else {
                    effects.push_back(eff);
                }

                bool matches = false;

                for (auto &e : effects) {
                    if (!e.contains("pred")) continue;
                    if (e["pred"][0] != pred) continue;

                    vector<string> eff_args = e["pred"][1];

                    for (auto &x : eff_args) {
                        if (assign.count(x)) x = assign[x];
                    }

                    if (eff_args == args) {
                        matches = true;
                        break;
                    }
                }

                if (matches) {
                    string a_name = action_name(act_name, act_args, t);
                    explainers.push_back(
                        ctx.bool_const(a_name.c_str()));
                }
            }
        }

        // --- DELETE actions ---
        for (auto &act_name : dels[pred]) {
            auto &act = domain["actions"][act_name];
            vector<string> act_params = act["terms"];

            for (auto &act_args : all_params(objects, act_params.size())) {

                unordered_map<string, string> assign;
                for (size_t i = 0; i < act_params.size(); i++)
                    assign[act_params[i]] = act_args[i];

                auto eff = act["eff"];
                vector<json> effects;
                if (eff.contains("and")) {
                    for (auto &e : eff["and"]) effects.push_back(e);
                } else {
                    effects.push_back(eff);
                }

                bool matches = false;

                for (auto &e : effects) {
                    if (!e.contains("not")) continue;
                    auto inner = e["not"];
                    if (inner["pred"][0] != pred) continue;

                    vector<string> eff_args = inner["pred"][1];

                    for (auto &x : eff_args) {
                        if (assign.count(x)) x = assign[x];
                    }

                    if (eff_args == args) {
                        matches = true;
                        break;
                    }
                }

                if (matches) {
                    string a_name = action_name(act_name, act_args, t);
                    explainers.push_back(
                        ctx.bool_const(a_name.c_str()));
                }
            }
        }

        // final frame axiom

        explainers.push_back(p_tp1 == p_t);
        s.add(z3::mk_or(explainers));
    }
}

  /* === Mutual exclusion === */

  vector<string> keys;
  for (auto &[k, _] : action_map)
    keys.push_back(k);

  for (size_t i = 0; i < keys.size(); i++) {
    z3::expr ai = ctx.bool_const(keys[i].c_str());

    // vector<z3::expr> others;
    z3::expr_vector others(ctx);
    for (size_t j = 0; j < keys.size(); j++) {
      if (i == j)
        continue;
      others.push_back(ctx.bool_const(keys[j].c_str()));
    }

    if (!others.empty()) s.add(z3::implies(ai, !z3::mk_or(others)));
  }

  return action_map;
}

/* =========================
   Pretty print
   ========================= */

string pretty(const pair<string, vector<string>> &a) {
  string s = a.first;
  for (auto &x : a.second)
    s += " " + x;
  return s;
}

class Domain {
    public:
        PredicateDefinitions predicates;
        ActionDefinitions actions;
        InternedNames constants;
};

class Problem {
    public:
        InternedNames objects;
};

/* =========================
   Main
   ========================= */

int main(int argc, char **argv) {

  if (argc != 2) {
    cerr << "Usage: planner input.json\n";
    return 1;
  }

  ifstream f(argv[1]);
  json data;
  f >> data;

  auto domain = data["domain"];
  auto problem = data["problem"];

  auto predicates = domain["predicates"];
  auto actions = domain["actions"];
  auto constants = domain["constants"];
  auto objects = problem["objects"];

  Domain int_domain;
  Problem int_problem;

  // Constants and problem-specific objects
  InternedNames literals;
  for (auto &p : constants.items()) {
      literals.add(p.value());
  }
  for (auto &p : objects.items()) {
      literals.add(p.value());
  }

  // Add predicates
  for (auto &p : predicates.items()) {
      int_domain.predicates.add(p.key(), p.value().size());
  }

  // Add actions
  for (auto &a : actions.items()) {
      InternedNames action_args;
      for (auto &arg : a.value()["terms"]) {
          action_args.add(arg);
      }
      int_domain.actions.add(a.key(), a.value().size());
  }

  // Check
  for (auto i = 0uz; i < int_domain.predicates.get_names().size(); i++) {
      cout << int_domain.predicates[i] << " " << int_domain.predicates.get_arity(i) <<'\n';
  }
  for (auto i = 0uz; i < int_domain.actions.get_names().size(); i++) {
      cout << int_domain.actions[i] << " " << int_domain.actions.get_arity(i) <<'\n';
  }

  vector<string> objects = problem["objects"];

  z3::context ctx;
  z3::solver s(ctx);

  add_init(ctx, s, domain, problem, objects);

  int horizon = 0;

  s.push();
  add_goal(ctx, s, problem["goal"], 0);

  if (s.check() == z3::sat) {
    cerr << "Trivial solution\n";
    auto of = ofstream("sas_plan");
    of.clear();
    of << std::endl;
    of.flush();
    return 0;
  }

  vector<unordered_map<string, pair<string, vector<string>>>> maps;

  while (true) {
    horizon++;

    s.pop();

    maps.push_back(add_transition(ctx, s, domain, objects, horizon - 1));

    s.push();
    add_goal(ctx, s, problem["goal"], horizon);

    //cout << s;
    if (s.check() == z3::sat)
      break;
  }

  auto m = s.get_model();

  vector<string> plan(horizon, "");

  for (int t = 0; t < horizon; t++) {
    for (auto &[k, a] : maps[t]) {
      z3::expr v = ctx.bool_const(k.c_str());
      if (m.eval(v).bool_value() == Z3_L_TRUE) {
        plan[t] = pretty(a);
      }
    }
  }

  auto of = ofstream("sas_plan");
  of.clear();
  for (auto &p : plan) {
    of << "(" << p << ")\n";
  }
  of.flush();

  return 0;
}
