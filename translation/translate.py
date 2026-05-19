# Translates
import itertools
from typing import Sequence
import z3
import pddl
import pddl.core
import pddl.logic
import pddl.logic.base
import pddl.logic.predicates
import sys
import time

z3Var = z3.Any

def formula_to_z3(f: pddl.core.Formula, t: int) -> z3Var:
    if isinstance(f, pddl.logic.Predicate):
        # return z3.Bool(f"{f}@{t}")
        ret = z3.Bool(f"{f}@{t}")
        # print(type(f))
        return ret

    elif isinstance(f, pddl.logic.base.Not):
        # print(f)
        return z3.Not(formula_to_z3(f.argument, t))

    elif isinstance(f, pddl.logic.base.And):
        return z3.And(
            *[
                formula_to_z3(g, t)
                for g in pddl.logic.base.And(f).operands
            ]
        )

    elif isinstance(f, pddl.logic.base.Or):
        return z3.Or(
            *[
                formula_to_z3(g, t)
                for g in pddl.logic.base.Or(f).operands
            ]
        )

    else:
        raise NotImplementedError


def action_to_z3(a: pddl.core.Action, t: int) -> z3Var:
    ret = z3.Bool(f"{a.name}{a.terms}@{t}")
    return ret


def all_params(
    objects: set[pddl.logic.terms.Term],
    ungrounded: pddl.core.Predicate | pddl.core.Action,
):
    arity: int
    if isinstance(ungrounded, pddl.core.Predicate):
        arity = ungrounded.arity
    elif isinstance(ungrounded, pddl.core.Action):
        arity = len(ungrounded.terms)
    else:
        raise NotImplementedError

    return itertools.product(objects, repeat=arity)


def all_params_from_partial(
    objects: set[pddl.logic.terms.Term],
    action: pddl.core.Action,
    partial: dict[pddl.logic.Variable, pddl.logic.terms.Term],
) -> list[tuple[pddl.logic.terms.Term, ...]]:
    """Given a partial assignment of action arguments (e.g. from antecedent
    of frame axioms), generate all parameter values agreeing with this
    partial assignment"""
    params: list[list[pddl.logic.terms.Term]] = [[]]

    for t in action.terms:
        assert isinstance(t, pddl.logic.Variable)
        if t in partial:
            for ps in params:
                ps.append(partial[t])
        else:
            params = [ps + [obj] for ps in params for obj in objects]

    return [tuple(ps) for ps in params]


def instantiate_to_z3(
    formula: pddl.core.Formula,
    params: Sequence[pddl.logic.terms.Term],
    arguments: tuple[pddl.logic.terms.Term, ...],
    t: int,
) -> z3Var:
    arg_map: dict[pddl.logic.Variable, pddl.logic.terms.Term] = {
        param: arg
        for param, arg in zip(params, arguments)
        if isinstance(param, pddl.logic.Variable)
    }
    return formula_to_z3(formula.instantiate(arg_map), t)


def instantiate_action_to_z3(
    action: pddl.core.Action,
    params: Sequence[pddl.logic.Variable],
    arguments: tuple[pddl.core.name_type, ...],
    t: int,
) -> z3Var:
    raise NotImplementedError


def grounded_preconditions(
    action: pddl.core.Action,
    arguments: tuple[pddl.logic.terms.Term, ...],
    t: int,
) -> z3Var | None:
    return (
        instantiate_to_z3(
            action.precondition, action.parameters, arguments, t
        )
        if action.precondition
        else None
    )


def grounded_effects(
    action: pddl.core.Action,
    arguments: tuple[pddl.logic.terms.Term, ...],
    t: int,
) -> z3Var | None:
    return (
        instantiate_to_z3(
            action.effect, action.parameters, arguments, t
        )
        if action.effect
        else None
    )


def grounded_action(
    action: pddl.core.Action,
    arguments: tuple[pddl.logic.terms.Term, ...],
    t: int,
) -> z3Var | None:
    return action_to_z3(action.instantiate(arguments), t)


def add_init(
    domain: pddl.core.Domain,
    problem: pddl.core.Problem,
    objects: set[pddl.logic.terms.Term],
    solver: z3.Solver,
):
    # all_preds: list[pddl.logic.Predicate] = []
    #
    for pred in domain.predicates:
        for params in all_params(objects, pred):
            arg_map = {
                var: val
                for var, val in zip(pred.terms, params)
                if isinstance(var, pddl.logic.Variable)
            }
            pred_g = pred.instantiate(arg_map)

            if pred_g in problem.init:
                solver.add(formula_to_z3(pred_g, 0))
            else:
                solver.add(z3.Not(formula_to_z3(pred_g, 0)))


def add_goal(problem: pddl.core.Problem, solver: z3.Solver, t: int):
    """Adds goal at time t"""
    solver.add(formula_to_z3(problem.goal, t))


def get_effects(
    action: pddl.core.Action,
) -> tuple[set[pddl.core.Predicate], set[pddl.core.Predicate]]:
    """
    Returns (eff+, eff-), where both are sets of Formulae.
    Must be instantiated when adding frame axioms.
    """
    eff_pos: set[pddl.core.Predicate] = set()
    eff_neg: set[pddl.core.Predicate] = set()

    effects = action.effect

    # Empty effect list
    if effects is None:
        return eff_pos, eff_neg

    # Single effect
    if isinstance(effects, pddl.logic.base.Atomic):
        effects = pddl.logic.base.And(effects)

    assert isinstance(effects, pddl.logic.base.And)
    for eff in effects.operands:
        if isinstance(eff, pddl.logic.base.Not):
            assert isinstance(
                eff.argument, pddl.logic.predicates.Predicate
            )
            eff_neg.add(eff.argument)
        else:
            assert isinstance(eff, pddl.logic.predicates.Predicate)
            eff_pos.add(eff)

    assert len(eff_pos) + len(eff_neg) == len(effects.operands)
    return eff_pos, eff_neg


def inverse_effect_list(
    domain: pddl.core.Domain, problem: pddl.core.Problem
) -> dict[
    pddl.core.Predicate,
    tuple[set[pddl.core.Action], set[pddl.core.Action]],
]:
    """
    Given a problem, find the actions ungrounded which add/remove
    each action.
    """
    ret: dict[
        pddl.core.Predicate,
        tuple[set[pddl.core.Action], set[pddl.core.Action]],
    ] = {}

    for action in domain.actions:
        eff_pos, eff_neg = get_effects(action)

        # print(action)
        # print(eff_pos)
        # print(eff_neg)

        for pos_pred in eff_pos:
            if pos_pred not in ret:
                ret[pos_pred] = set(), set()
            ret[pos_pred][0].add(action)
        for neg_pred in eff_neg:
            if neg_pred not in ret:
                ret[neg_pred] = set(), set()
            ret[neg_pred][1].add(action)

    return ret


def add_transition_function(
    domain: pddl.core.Domain,
    problem: pddl.core.Problem,
    objects: set[pddl.logic.terms.Term],
    t: int,
    s: z3.Solver,
) -> dict[z3Var, pddl.core.Action]:
    """
    Adds the transition function at time t (to t + 1) to the solver.
    Returns a map from action variables to pddl actions added at this time
    step.
    """
    ret: dict[z3Var, pddl.core.Action] = {}

    # Precondition/effects
    log("Grouding, adding precondition/effect axioms")
    for action in domain.actions:
        # log("Getting params")
        for args in all_params(objects, action):
            # log("Grouding actions")
            g_action = grounded_action(action, args, t)
            # log("Grouding preconditions")
            g_prec = grounded_preconditions(action, args, t)
            # log("Grouding effects")
            g_eff = grounded_effects(action, args, t + 1)
            # log("Adding constraints")
            s.add(z3.Implies(g_action, z3.And(g_prec, g_eff)))
            # log("Populating map")
            ret[g_action] = action.instantiate(args)

    # Frame axioms
    # TODO:  use set?
    pos_explanations: dict[pddl.core.Formula, list[z3Var]] = {}
    neg_explanations: dict[pddl.core.Formula, list[z3Var]] = {}

    ground_preds: set[z3Var] = set()

    log("Finding inverse effects")
    inverse_effs = inverse_effect_list(domain, problem)

    log("Finding explanators")
    for pred, [pos_acts, neg_acts] in inverse_effs.items():

        for params in all_params(objects, pred):
            partial_map = {
                t: p
                for (t, p) in zip(pred.terms, params)
                if isinstance(t, pddl.logic.Variable)
            }

            pred_g = pred.instantiate(partial_map)
            ground_preds.add(pred_g)
            if pred_g not in pos_explanations:
                pos_explanations[pred_g] = []
            if pred_g not in neg_explanations:
                neg_explanations[pred_g] = []

            # print(f"params {params}")

            for pos_act in pos_acts:
                for args in all_params_from_partial(
                    objects, pos_act, partial_map
                ):
                    pos_explanations[pred_g].append(
                        grounded_action(pos_act, args, t)
                    )
            for neg_act in neg_acts:
                for args in all_params_from_partial(
                    objects, neg_act, partial_map
                ):
                    neg_explanations[pred_g].append(
                        grounded_action(neg_act, args, t)
                    )

    # log("Adding add list frame axioms")
    # for pred, actions in pos_explanations.items():
    #
    #     s.add(
    #         z3.Implies(
    #             z3.And(
    #                 z3.Not(formula_to_z3(pred, t)),
    #                 formula_to_z3(pred, t + 1),
    #             ),
    #             z3.Or(actions),
    #         )
    #     )
    #
    # log("Adding delete list frame axioms")
    # for pred, actions in neg_explanations.items():
    #     s.add(
    #         z3.Implies(
    #             z3.And(
    #                 formula_to_z3(pred, t),
    #                 z3.Not(
    #                     formula_to_z3(pred, t + 1),
    #                 ),
    #             ),
    #             z3.Or(actions),
    #         )
    #     )

    for pred in ground_preds:
    #     # g_pos_acts = {
    #     #     formula_to_z3(a, t) for a in pos_explanations.get(pred) or []
    #     # }
    #     # g_neg_acts = {
    #     #     formula_to_z3(a, t) for a in neg_explanations.get(pred) or []
    #     # }
        con = z3.Or(formula_to_z3(pred, t + 1) == formula_to_z3(pred, t),
                    z3.Or(*pos_explanations.get(pred) or [],
                          *neg_explanations.get(pred) or [])
                    )
        s.add(con)
            # == (
            #     z3.Or(
            #         z3.Or(pos_explanations.get(pred) or []),
            #         z3.And(
            #             formula_to_z3(pred, t),
            #             z3.Not(z3.Or(neg_explanations.get(pred) or [])),
            #         ),
            #     )
            # )
            #    )
    #     # print(con)
    #
    #     s.add(con)

    # Exclusion axioms
    # log("Adding exclusion axioms")
    # print(ret.keys())
    # all_actions: list[z3Var] = ret.keys()
    # for action in domain.actions:
    #     for params in all_params(objects, action):
    #         all_actions.append(grounded_action(action, params, t))

    for i, chosen_action in enumerate(ret.keys()):
        s.add(
            z3.Implies(
                chosen_action,
                z3.Not(
                    z3.Or(
                        [a for j, a in enumerate(ret.keys()) if i != j]
                    )
                ),
            )
        )

    return ret


def pretty_action(action: pddl.core.Action) -> str:
    ret = action.name
    for t in action.terms:
        ret += f" {t.name}"
    return ret


t0 = time.time()
tprev = t0


def log(msg) -> None:
    # return

    cur_time = time.time()
    since_start = f"{(cur_time - t0) * 1000:.4f}"
    since_prev = f"{(cur_time - tprev) * 1000:.4f}"
    globals()["tprev"] = cur_time
    sys.stderr.write(f"[t={since_start}ms (+{since_prev}ms)] {msg}\n")


def main():
    if len(sys.argv) != 3:
        sys.stderr.write(
            "Usage: python3 translate.py [domain.pddl] [problem.pddl]\n"
        )
        return 1

    log(f"Parsing domain: {sys.argv[1]}")
    domain: pddl.core.Domain = pddl.parse_domain(sys.argv[1])
    log(f"Parsing problem: {sys.argv[2]}")
    problem: pddl.core.Problem = pddl.parse_problem(sys.argv[2])

    # print(domain.actions)

    s = z3.Solver()

    # TODO: add types

    # Add objects (problem variables or domain constants)
    log("Initialising objects")
    objects: set[pddl.logic.terms.Term] = set.union(
        {o for o in domain.constants},
        {o for o in problem.objects},
    )

    action_maps: list[dict[z3Var, pddl.core.Action]] = []

    # Check for trivial solution
    log("Adding initial state")
    add_init(domain, problem, objects, s)
    log("Adding goal")
    s.push()
    add_goal(problem, s, 0)
    log("Solving for length 1")
    if s.check() == z3.sat:
        log("Found trivial solution")
        log(s.model())
        with (open("sas_plan", "w")  as f):
            f.write("")
        return

    t1 = time.time()

    horizon = 0
    while True:
        horizon += 1
        log("Pop previous goal")
        s.pop()  # Pop goal
        log(f"Adding transition for length {horizon}")
        action_maps.append(
            add_transition_function(
                domain, problem, objects, horizon - 1, s
            )
        )
        s.push()
        log(f"Adding goal for length {horizon}")
        add_goal(problem, s, horizon)
        log(f"Solving length {horizon}")
        # print(s)
        if s.check() == z3.sat:
            break
        log(f"No plan of length {horizon}")

    # Extract plan
    log(f"Found plan of length {horizon}")
    mod = s.model()
    plan: list[pddl.core.Action | None] = [
        None for _ in range(horizon)
    ]
    for t in range(horizon):
        for v, a in action_maps[t].items():
            if mod.get_interp(v):
                plan[t] = a
                continue
    with (open("sas_plan", "w")  as f):
        f.writelines([f"({pretty_action(a)})\n" if a is not None else "()" for a in plan])
    log(f"Planning time (excl init): {time.time() - t1} sec")

    # print(s)


if __name__ == "__main__":
    main()
