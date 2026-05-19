"""Parse a PDDL domain/problem pair and emit the JSON IR.

Usage:
    python3 scripts/parse.py <domain.pddl> <problem.pddl> [output.json]
"""

import json
import sys

import pddl.core
import pddl.logic
import pddl.logic.base


def pred_to_obj(p: pddl.core.Predicate, bound: set[str] | None = None) -> tuple[str, list[str]]:
    bound = bound or set()
    return p.name, [f"{'?' if a.name in bound else ''}{a.name}" for a in p.terms]


def formula_to_obj(f: pddl.core.Formula | None, bound: set[str] | None = None):
    bound = bound or set()
    if isinstance(f, pddl.core.Predicate):
        return {"pred": pred_to_obj(f, bound)}
    if isinstance(f, pddl.logic.base.And):
        return {"and": [formula_to_obj(o, bound) for o in f.operands]}
    if isinstance(f, pddl.logic.base.Or):
        return {"or": [formula_to_obj(o, bound) for o in f.operands]}
    if isinstance(f, pddl.logic.base.Not):
        return {"not": formula_to_obj(f.argument, bound)}
    return {}


def action_to_obj(a: pddl.core.Action) -> tuple[str, object]:
    terms = [t.name for t in a.terms]
    return a.name, {
        "terms": [f"?{t}" for t in terms],
        "pre": formula_to_obj(a.precondition, set(terms)),
        "eff": formula_to_obj(a.effect, set(terms)),
    }


def main() -> int:
    if len(sys.argv) not in (3, 4):
        sys.stderr.write(
            "Usage: python3 scripts/parse.py <domain.pddl> <problem.pddl> [output.json]\n"
        )
        return 1

    domain_file = sys.argv[1]
    problem_file = sys.argv[2]
    output_file = sys.argv[3] if len(sys.argv) == 4 else "ir.json"

    domain: pddl.core.Domain = pddl.parse_domain(domain_file)
    problem: pddl.core.Problem = pddl.parse_problem(problem_file)

    out = {"domain": {}, "problem": {}}
    out["domain"]["predicates"] = dict(pred_to_obj(p) for p in domain.predicates)
    out["domain"]["actions"] = dict(action_to_obj(a) for a in domain.actions)
    out["problem"]["constants"] = [o.name for o in domain.constants]
    out["problem"]["objects"] = [o.name for o in problem.objects]
    out["problem"]["init"] = [formula_to_obj(f) for f in problem.init]
    out["problem"]["goal"] = formula_to_obj(problem.goal)

    with open(output_file, "w") as f:
        f.write(json.dumps(out))
    print(json.dumps(out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
