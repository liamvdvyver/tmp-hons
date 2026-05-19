# Parse pddl domain/problem file,
# Outputs json IR

import pddl.core
import pddl.logic
import pddl.logic.base
import sys
import json

def pred_to_obj(p: pddl.core.Predicate, bound: set[str] = set()) -> tuple[str, list[str]]:
    print(f"bound: {bound}")
    return p.name, [f"{"?" if a.name in bound else ""}{a.name}" for a in p.terms]


def formula_to_obj(f: pddl.core.Formula | None, bound: set[str]= set()):
    if isinstance(f, pddl.core.Predicate):
        return {"pred": pred_to_obj(f, bound)}
    if isinstance(f, pddl.logic.base.And):
        return {"and": [formula_to_obj(o, bound) for o in f.operands]}
    if isinstance(f, pddl.logic.base.Or):
        return {"or": [formula_to_obj(o, bound) for o in f.operands]}
    if isinstance(f, pddl.logic.base.Not):
        return {"not": formula_to_obj(f.argument, bound)}
    else:
        return {}

def action_to_obj(a: pddl.core.Action) -> tuple[str, any]:
    terms = [t.name for t in a.terms]
    print(terms)
    print(set(terms))
    return a.name, {
      "terms": [f"?{t}" for t in terms],
      "pre": formula_to_obj(a.precondition, set(terms)),
      "eff": formula_to_obj(a.effect, set(terms))
    }


def main():
    if len(sys.argv) != 3:
        sys.stderr.write(
            "Usage: python3 translate.py [domain.pddl] [problem.pddl]\n"
        )
        return 1

    sys.stderr.write(f"Parsing domain: {sys.argv[1]}\n")
    domain: pddl.core.Domain = pddl.parse_domain(sys.argv[1])
    sys.stderr.write(f"Parsing problem: {sys.argv[2]}\n")
    problem: pddl.core.Problem = pddl.parse_problem(sys.argv[2])

    out = {"domain": {}, "problem": {}}

    out["domain"]["predicates"] = dict([pred_to_obj(p) for p in domain.predicates])
    out["domain"]["actions"] = dict([action_to_obj(a) for a in domain.actions])
    out["problem"]["constants"] = [o.name for o in domain.constants]

    out["problem"]["objects"] = [o.name for o in problem.objects]
    out["problem"]["init"] = [formula_to_obj(f) for f in problem.init]
    out["problem"]["goal"] = formula_to_obj(problem.goal)


    with (open("ir.json", "w")  as f):
        f.write(json.dumps(out))
    print(json.dumps(out))

if __name__ == "__main__":
    main()
