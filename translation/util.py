import pddl
import pddl.core
import itertools
import sys


def is_subtype(
    type: pddl.core.name_type | None,
    parent: pddl.core.name_type,
    types: dict[pddl.core.name_type, pddl.core.name_type | None],
) -> bool:
    """
    Walks the type tree, will be slow if the class hierarchy is deep.
    """
    while type is not None:
        if type == parent:
            return True
        type = types.get(type)
    return False


def ground_actions(
    dom: pddl.core.Domain, problem: pddl.core.Problem
) -> dict[
    pddl.core.Action,
    tuple[pddl.core.Action, list[pddl.core.name_type]],
]:
    """
    Returns a dict which maps a ground action to the "free" action, and a list of ground terms.
    """

    # Get typing info
    # typed_objects: dict[pddl.core.Types, Set[pddl.core.Constant]] = {}
    # for obj in problem.objects:

    type_memo: dict[
        pddl.core.name_type, frozenset[pddl.core.name_type]
    ] = {}

    ret: dict[
        pddl.core.Action,
        tuple[pddl.core.Action, list[pddl.core.name_type]],
    ] = {}

    objects: set[pddl.core.name_type] = {
        o.name for o in problem.objects
    }

    for act in dom.actions:
        print(f"{act.name}")

        # Get all relevant tuples of products
        arg_lists: list[frozenset[pddl.core.name_type]] = []

        for var in act.parameters:
            print(f"  {var.name}")

            # TODO: why is this a set?
            # Is multiple types for an argument even legal pddl?
            type = list(var.type_tags)[0]
            if len(var.type_tags) > 1:
                sys.stderr.write(
                    f"Warning: multiple types for arg {var.name} in action {act.name}"
                )

            print(f"   {type}")

            # Get set of objects with this type
            if type_memo.get(type) is None:
                type_memo[type] = frozenset(
                    {
                        o
                        for o in objects
                        if is_subtype(o, type, dom.types)
                    }
                )

            arg_lists.append(type_memo[type])

        arg_product = itertools.product(*arg_lists)
        for args in arg_product:
            for var, val in zip(act.parameters, args):
                new_arg = pddl.core.Action(
                    name=f"",
                    parameters=[],
                )

    return ret
