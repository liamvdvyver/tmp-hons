#!/usr/bin/env python3
import argparse
import csv
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "tmp" / "idtmp_blocksworld.py"
MODES = ["totally_ordered", "partially_ordered", "graphplan"]


def parse_range(spec: str) -> list[int]:
    spec = spec.strip()
    if not spec:
        raise ValueError("empty range specification")

    values: list[int] = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if ":" in part:
            pieces = part.split(":")
            if len(pieces) not in (2, 3):
                raise ValueError(f"invalid range component: {part}")
            start = int(pieces[0])
            stop = int(pieces[1])
            step = int(pieces[2]) if len(pieces) == 3 else 1
            if step <= 0:
                raise ValueError(f"step must be positive: {part}")
            if start > stop:
                raise ValueError(f"range start must be <= stop: {part}")
            values.extend(range(start, stop + 1, step))
        else:
            values.append(int(part))

    if not values:
        raise ValueError("range specification produced no values")
    return values


def plan_length(plan_json: dict) -> int:
    return sum(len(step) for step in plan_json["plan"])


def makespan(plan_json: dict) -> int:
    return len(plan_json["plan"])


def run_trial(
    planner: str,
    n_blocks: int,
    n_stacks: int,
    n_arms: int,
    mode: str,
    seed: int,
    generalize_supported_grab_collisions: bool,
) -> tuple[dict, float]:
    command = [
        sys.executable,
        str(SCRIPT),
        str(n_blocks),
        str(n_stacks),
        str(n_arms),
        "--seed",
        str(seed),
        "--mode",
        mode,
        "--planner",
        planner,
    ]
    if generalize_supported_grab_collisions:
        command.append("--generalize-supported-grab-collisions")
    t0 = time.time()
    proc = subprocess.run(command, check=True, capture_output=True, text=True)
    elapsed = time.time() - t0
    return json.loads(proc.stdout), elapsed


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the implicit-clear experiment on a grid of block/stacks/arms settings and write CSV results."
    )
    parser.add_argument("--blocks", required=True, help="Range/list, e.g. 3:8 or 3,5,7")
    parser.add_argument("--stacks", required=True, help="Range/list, e.g. 1:3")
    parser.add_argument("--arms", required=True, help="Range/list, e.g. 1:4")
    parser.add_argument("--trials", type=int, default=10)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--planner", default=str(ROOT / "planner"))
    parser.add_argument("--csv", default=str(ROOT / "tmp" / "grid_results.csv"))
    parser.add_argument(
        "--generalize-supported-grab-collisions",
        action="store_true",
        help="Pass through supported-grab collision generalization to idtmp_blocksworld.py",
    )
    args = parser.parse_args()

    if args.trials <= 0:
        sys.stderr.write("--trials must be positive\n")
        return 1

    block_values = parse_range(args.blocks)
    stack_values = parse_range(args.stacks)
    arm_values = parse_range(args.arms)

    out_path = Path(args.csv)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "n_blocks",
        "n_stacks",
        "n_arms",
        "trial",
        "seed",
        "mode",
        "generalize_supported_grab_collisions",
        "runtime_seconds",
        "rejected_plans",
        "plan_length",
        "makespan",
        "problem",
    ]

    total_points = len(block_values) * len(stack_values) * len(arm_values) * args.trials * len(MODES)
    completed = 0

    with out_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        for n_blocks in block_values:
            for n_stacks in stack_values:
                for n_arms in arm_values:
                    for trial in range(args.trials):
                        seed = args.seed + trial
                        print(
                            f"config blocks={n_blocks} stacks={n_stacks} arms={n_arms} "
                            f"trial={trial + 1}/{args.trials} seed={seed}"
                        )
                        for mode in MODES:
                            data, elapsed = run_trial(
                                args.planner,
                                n_blocks,
                                n_stacks,
                                n_arms,
                                mode,
                                seed,
                                args.generalize_supported_grab_collisions,
                            )
                            accepted_plan = data["accepted_plan"]
                            row = {
                                "n_blocks": n_blocks,
                                "n_stacks": n_stacks,
                                "n_arms": n_arms,
                                "trial": trial,
                                "seed": seed,
                                "mode": mode,
                                "generalize_supported_grab_collisions": int(
                                    args.generalize_supported_grab_collisions
                                ),
                                "runtime_seconds": elapsed,
                                "rejected_plans": int(data["rejected"]),
                                "plan_length": plan_length(accepted_plan),
                                "makespan": makespan(accepted_plan),
                                "problem": data["problem"],
                            }
                            writer.writerow(row)
                            f.flush()
                            completed += 1
                            print(
                                f"  {mode}: runtime={elapsed:.4f}s "
                                f"rejected={row['rejected_plans']} "
                                f"plan_length={row['plan_length']} "
                                f"makespan={row['makespan']} "
                                f"[{completed}/{total_points}]"
                            )
                        print()

    print(f"wrote CSV to {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
