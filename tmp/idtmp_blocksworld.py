#!/usr/bin/env python3
import argparse
import itertools
import json
import random
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import pddl
import pddl.core

ROOT = Path(__file__).resolve().parent.parent
DOMAIN_FILE = ROOT / "tmp" / "domain.pddl"
PARSE_SCRIPT = ROOT / "scripts" / "parse.py"
PLANNER = ROOT / "planner"
WORKDIR = ROOT / "tmp"


dom = pddl.parse_domain(str(DOMAIN_FILE))
preds = {p.name: p for p in dom.predicates}


@dataclass(frozen=True)
class Action:
    name: str
    arguments: tuple[str, ...]


@dataclass(frozen=True)
class Failure:
    reason: str
    constraint: dict[str, Any] | None = None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Proof-of-concept TAMP loop with implicit clear checking."
    )
    parser.add_argument("n_blocks", type=int)
    parser.add_argument("n_stacks", type=int)
    parser.add_argument("n_arms", type=int, nargs="?", default=2)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument(
        "--mode",
        choices=["totally_ordered", "partially_ordered", "graphplan"],
        default="totally_ordered",
    )
    parser.add_argument("--planner", default=str(PLANNER))
    parser.add_argument(
        "--generalize-supported-grab-collisions",
        action="store_true",
        help=(
            "When a grab fails because some block a is on target block b, "
            "send the general constraint 'on(a,b) -> not grab(b)' as JSON."
        ),
    )
    return parser.parse_args()


def mode_args(mode: str) -> list[str]:
    if mode == "partially_ordered":
        return ["--partially-ordered"]
    if mode == "graphplan":
        return ["--graph-plan"]
    return []


def instantiate_all(pred, args):
    return pred.instantiate({x: y for (x, y) in zip(pred.terms, args)})


def random_stacks(rng: random.Random, block_names: list[str], n_stacks: int) -> list[list[str]]:
    stacks = [[] for _ in range(n_stacks)]
    shuffled = block_names[:]
    rng.shuffle(shuffled)
    for b in shuffled:
        stacks[rng.randrange(n_stacks)].append(b)
    for i, stack in enumerate(stacks):
        stacks[i] = rng.sample(stack, len(stack))
    return stacks


def encode_stacks(
    stacks: list[list[str]],
    blocks: dict[str, pddl.core.Constant],
    arms: dict[str, pddl.core.Constant],
) -> set:
    ret = set()
    for arm in arms.values():
        ret.add(instantiate_all(preds["arm"], [arm]))
        ret.add(instantiate_all(preds["arm-clear"], [arm]))
    for block in blocks.values():
        ret.add(instantiate_all(preds["block"], [block]))
    for stack in stacks:
        if not stack:
            continue
        ret.add(instantiate_all(preds["on-table"], [blocks[stack[0]]]))
        for i in range(1, len(stack)):
            ret.add(instantiate_all(preds["on"], [blocks[stack[i]], blocks[stack[i - 1]]]))
    return ret


def make_problem(
    n_blocks: int,
    n_stacks: int,
    n_arms: int,
    seed: int,
) -> tuple[pddl.core.Problem, list[list[str]], list[list[str]]]:
    rng = random.Random(seed)
    block_names = [f"b{i}" for i in range(n_blocks)]
    arm_names = [f"r{i}" for i in range(n_arms)]

    init_stacks = random_stacks(rng, block_names, n_stacks)
    goal_stacks = random_stacks(rng, block_names, n_stacks)

    blocks = {b: pddl.core.Constant(b, "block") for b in block_names}
    arms = {a: pddl.core.Constant(a, "arm") for a in arm_names}

    prob = pddl.core.Problem(
        name=f"implicit-clear-{n_blocks}x{n_stacks}x{n_arms}-{seed}",
        domain=dom,
        objects=[*blocks.values(), *arms.values()],
        init=encode_stacks(init_stacks, blocks, arms),
        goal=pddl.core.And(*encode_stacks(goal_stacks, blocks, arms)),
    )
    return prob, init_stacks, goal_stacks


def extract_plan(text: str) -> list[list[Action]]:
    data = json.loads(text)
    return [
        [Action(a["name"], tuple(a["arguments"])) for a in step]
        for step in data["plan"]
    ]


def find_stack_containing(stacks: list[list[str]], block: str) -> int:
    for i, stack in enumerate(stacks):
        if block in stack:
            return i
    raise ValueError(f"block {block} not found")


def supported_grab_constraint(action: Action, above: str, target: str) -> dict[str, Any]:
    return {
        "not": {
            "and": [
                {"pred": ["on", [above, target]]},
                {"action": [action.name, list(action.arguments)]},
            ]
        }
    }


def apply_action(
    action: Action,
    stacks: list[list[str]],
    holding: dict[str, str],
    t: int,
) -> Failure | None:
    if action.name == "pick":
        arm, block = action.arguments
        try:
            stack_idx = find_stack_containing(stacks, block)
        except ValueError:
            return Failure(f"step {t}: pick({arm}, {block}) block not found")
        stack = stacks[stack_idx]
        if len(stack) != 1 or stack[-1] != block:
            if block in stack:
                block_pos = stack.index(block)
                if block_pos + 1 < len(stack):
                    above = stack[block_pos + 1]
                    return Failure(
                        f"step {t}: pick({arm}, {block}) violates implicit clear",
                        supported_grab_constraint(action, above, block),
                    )
            return Failure(f"step {t}: pick({arm}, {block}) violates implicit clear")
        if arm in holding:
            return Failure(f"step {t}: arm {arm} already holding")
        stack.pop()
        holding[arm] = block
    elif action.name == "pick-from":
        arm, block, support = action.arguments
        try:
            stack_idx = find_stack_containing(stacks, block)
        except ValueError:
            return Failure(f"step {t}: pick-from({arm}, {block}, {support}) block not found")
        stack = stacks[stack_idx]
        if stack[-1] != block:
            if block in stack:
                block_pos = stack.index(block)
                if block_pos + 1 < len(stack):
                    above = stack[block_pos + 1]
                    return Failure(
                        f"step {t}: pick-from({arm}, {block}, {support}) violates implicit clear",
                        supported_grab_constraint(action, above, block),
                    )
            return Failure(f"step {t}: pick-from({arm}, {block}, {support}) violates implicit clear")
        if len(stack) < 2 or stack[-2] != support:
            return Failure(f"step {t}: pick-from({arm}, {block}, {support}) has wrong support")
        if arm in holding:
            return Failure(f"step {t}: arm {arm} already holding")
        stack.pop()
        holding[arm] = block
    elif action.name == "place":
        arm, block = action.arguments
        if holding.get(arm) != block:
            return Failure(f"step {t}: place({arm}, {block}) without holding")
        stacks.append([block])
        del holding[arm]
    elif action.name == "place-on":
        arm, block, target = action.arguments
        if holding.get(arm) != block:
            return Failure(f"step {t}: place-on({arm}, {block}, {target}) without holding")
        try:
            target_idx = find_stack_containing(stacks, target)
        except ValueError:
            return Failure(f"step {t}: place-on({arm}, {block}, {target}) target disappeared")
        target_stack = stacks[target_idx]
        if target_stack[-1] != target:
            return Failure(f"step {t}: place-on({arm}, {block}, {target}) violates implicit clear")
        target_stack.append(block)
        del holding[arm]
    else:
        return Failure(f"step {t}: unknown action {action.name}")

    stacks[:] = [stack for stack in stacks if stack]
    return None


def apply_step_with_permutations(
    step: list[Action],
    stacks: list[list[str]],
    holding: dict[str, str],
    t: int,
) -> tuple[Failure | None, list[list[str]], dict[str, str]]:
    if len(step) <= 1:
        stacks_copy = [stack[:] for stack in stacks]
        holding_copy = dict(holding)
        for action in step:
            failure = apply_action(action, stacks_copy, holding_copy, t)
            if failure is not None:
                return failure, stacks, holding
        return None, stacks_copy, holding_copy

    last_failure = Failure(f"step {t}: no feasible permutation")
    for ordering in itertools.permutations(step):
        stacks_copy = [stack[:] for stack in stacks]
        holding_copy = dict(holding)
        failure = None
        for action in ordering:
            failure = apply_action(action, stacks_copy, holding_copy, t)
            if failure is not None:
                break
        if failure is None:
            return None, stacks_copy, holding_copy
        last_failure = failure
    return last_failure, stacks, holding


def check_and_apply(plan: list[list[Action]], init_stacks: list[list[str]]) -> Failure | None:
    stacks = [stack[:] for stack in init_stacks]
    holding: dict[str, str] = {}

    for t, step in enumerate(plan):
        failure, stacks, holding = apply_step_with_permutations(step, stacks, holding, t)
        if failure is not None:
            return failure

    return None


def read_next_plan(proc: subprocess.Popen[str]) -> str:
    if proc.stdout is None:
        raise RuntimeError("planner stdout unavailable")

    chunks: list[str] = []
    depth = 0
    started = False

    while True:
        ch = proc.stdout.read(1)
        if ch == "":
            raise RuntimeError("planner terminated before producing a full plan")
        if not started:
            if ch.isspace():
                continue
            if ch != "{":
                continue
            started = True
            depth = 1
            chunks.append(ch)
            continue

        chunks.append(ch)
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return "".join(chunks)


def run_loop(
    problem_file: Path,
    mode: str,
    planner: str,
    init_stacks: list[list[str]],
    generalize_supported_grab_collisions: bool,
) -> dict:
    ir_file = WORKDIR / f"ir_{mode}.json"
    subprocess.run(
        [sys.executable, str(PARSE_SCRIPT), str(DOMAIN_FILE), str(problem_file), str(ir_file)],
        check=True,
        capture_output=True,
        text=True,
    )

    proc = subprocess.Popen(
        [planner, *mode_args(mode), "--json-out", str(ir_file)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=0,
    )

    if proc.stdin is None:
        raise RuntimeError("planner stdin unavailable")

    rejected = 0
    last_reason = ""

    while True:
        plan_text = read_next_plan(proc)
        plan = extract_plan(plan_text)
        failure = check_and_apply(plan, init_stacks)
        last_reason = "accepted" if failure is None else failure.reason
        if failure is None:
            proc.stdin.write("y\n")
            proc.stdin.flush()
            _, stderr_text = proc.communicate()
            return {
                "mode": mode,
                "rejected": rejected,
                "reason": last_reason,
                "accepted_plan": json.loads(plan_text),
                "planner_stderr": stderr_text,
            }

        rejected += 1
        if generalize_supported_grab_collisions and failure.constraint is not None:
            proc.stdin.write(json.dumps(failure.constraint) + "\n")
        else:
            proc.stdin.write("n\n")
        proc.stdin.flush()


def main() -> int:
    args = parse_args()
    WORKDIR.mkdir(exist_ok=True)

    problem, init_stacks, goal_stacks = make_problem(
        args.n_blocks, args.n_stacks, args.n_arms, args.seed
    )
    problem_file = WORKDIR / f"problem_{args.n_blocks}_{args.n_stacks}_{args.n_arms}_{args.seed}.pddl"
    problem_file.write_text(str(problem))

    result = run_loop(
        problem_file,
        args.mode,
        args.planner,
        init_stacks,
        args.generalize_supported_grab_collisions,
    )
    print(
        json.dumps(
            {
                "domain": str(DOMAIN_FILE),
                "problem": str(problem_file),
                "mode": args.mode,
                "seed": args.seed,
                "init_stacks": init_stacks,
                "goal_stacks": goal_stacks,
                **result,
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
