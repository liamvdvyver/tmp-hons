# Generates a multi-arm blocksworld problem
import sys
import random
import pddl
import pddl.core

random.seed = "deez"

DOMAIN_FILE = "problems/multi_blocksworld/domain.pddl"

dom = pddl.parse_domain(DOMAIN_FILE)
preds = {p.name: p for p in dom.predicates}

n_blocks = int(sys.argv[1])
n_stacks = int(sys.argv[2])
n_arms = int(sys.argv[3]) if len(sys.argv) >= 4 else 2

init_cfg = [[] for _ in range(n_stacks)]
final_cfg = [[] for _ in range(n_stacks)]

blocks = {
    b: pddl.core.Constant(b, "block")
    for b in [f"b{i}" for i in range(n_blocks)]
}
arms = {
    a: pddl.core.Constant(a, "arm")
    for a in [f"r{i}" for i in range(n_arms)]
}

for b_name, b in blocks.items():
    beg = int(random.random() * n_stacks)
    fin = int(random.random() * n_stacks)
    init_cfg[beg].append(b)
    final_cfg[fin].append(b)

for i, l in enumerate(init_cfg):
    init_cfg[i] = random.sample(l, len(l))

def instantiate_all(pred, args):
    return pred.instantiate(
                    {
                        x: y
                        for (x, y) in zip(
                            pred.terms, args
                        )
                    }
            )

def encode_stacks(stacks):
    ret = set()
    for arm in arms.values():
        ret.add(instantiate_all(preds["arm"], [arm]))
        ret.add(instantiate_all(preds["arm-clear"], [arm]))
    for block in blocks.values():
        ret.add(instantiate_all(preds["block"], [block]))
    for stack in stacks:
        if not len(stack):
            continue
        ret.add(instantiate_all(preds["on-table"], [stack[0]]))
        for i in range(1, len(stack)):
            ret.add(instantiate_all(preds["on"], [stack[i], stack[i - 1]]))
        ret.add(instantiate_all(preds["clear"], [stack[-1]]))
    return ret

prob = pddl.core.Problem(
    name=f"Random-{n_blocks}x{n_stacks}x{n_arms}",
    domain=dom,
    objects=[*blocks.values(), *arms.values()],
    init=encode_stacks(init_cfg),
    goal=pddl.core.And(*encode_stacks(final_cfg))
)
print(prob)
