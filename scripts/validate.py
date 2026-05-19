"""Validate a plan against a PDDL domain/problem pair.

Usage:
    python3 scripts/validate.py <domain.pddl> <problem.pddl> <sas_plan>
"""

import sys

import pddl
import pddl.core


def main() -> int:
    if len(sys.argv) != 4:
        sys.stderr.write(
            "Usage: python3 scripts/validate.py <domain.pddl> <problem.pddl> <sas_plan>\n"
        )
        return 1

    domain: pddl.core.Domain = pddl.parse_domain(sys.argv[1])
    problem: pddl.core.Problem = pddl.parse_problem(sys.argv[2])
    plan: pddl.core.Plan = pddl.parse_plan(sys.argv[3])

    return 0 if plan.check(domain, problem) else 1


if __name__ == "__main__":
    raise SystemExit(main())
