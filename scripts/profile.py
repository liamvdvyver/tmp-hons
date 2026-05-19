import subprocess
import sys
import time

import matplotlib.pyplot as plt


def usage() -> int:
    sys.stderr.write(
        "Usage: python3 scripts/profile.py <min_n> <max_n> <trials> [domain.pddl] [planner] [fd_alias] [n_arms]\n"
        "  min_n/max_n: inclusive problem sizes to benchmark\n"
        "  trials: number of repetitions per size\n"
        "  domain.pddl: planning domain file (default: problems/multi_blocksworld/domain.pddl)\n"
        "  planner: C++ planner executable (default: ./planner)\n"
        "  fd_alias: Fast Downward alias, or 'none' to skip FD (default: seq-opt-lmcut)\n"
        "  n_arms: number of arms for generated problems (default: 2)\n"
    )
    return 1


def mean(xs: list[float]) -> float:
    return sum(xs) / len(xs) if xs else 0.0


def run_checked(command: list[str], capture_output: bool = False) -> None:
    subprocess.run(command, capture_output=capture_output).check_returncode()


def main() -> int:
    if len(sys.argv) not in (4, 5, 6, 7, 8):
        return usage()

    min_n = int(sys.argv[1])
    max_n = int(sys.argv[2])
    trials = int(sys.argv[3])
    domain = sys.argv[4] if len(sys.argv) >= 5 else "problems/multi_blocksworld/domain.pddl"
    planner = sys.argv[5] if len(sys.argv) >= 6 else "./planner"
    fd_alias = sys.argv[6] if len(sys.argv) >= 7 else "seq-opt-lmcut"
    n_arms = int(sys.argv[7]) if len(sys.argv) >= 8 else 2

    if min_n > max_n:
        sys.stderr.write("min_n must be <= max_n\n")
        return 1

    ns = list(range(min_n, max_n + 1))
    fname = "problems/multi_blocksworld/random.pddl"

    planner_modes = {
        "totally_ordered": [],
        "partially_ordered": [],
        "graphplan": [],
    }
    planner_mode_args = {
        "totally_ordered": [],
        "partially_ordered": ["--partially-ordered"],
        "graphplan": ["--graph-plan"],
    }
    planner_mode_avgs = {name: [] for name in planner_modes}

    fd_times: list[list[float]] = []
    fd_avg_times: list[float] = []
    run_fd = fd_alias.lower() != "none"

    for n in ns:
        subprocess.getoutput(
            f"python3 ./scripts/gen_blockworld.py {n} 1 {n_arms} > {fname}"
        )
        run_checked(["python3", "./scripts/parse.py", domain, fname], capture_output=True)

        for mode_name in planner_modes:
            planner_modes[mode_name].append([])
        if run_fd:
            fd_times.append([])

        for trial in range(trials):
            print(f"n={n}, trial={trial + 1}/{trials}")

            for mode_name, extra_args in planner_mode_args.items():
                t0 = time.time()
                run_checked([planner, *extra_args, "-y", "ir.json"], capture_output=True)
                elapsed = time.time() - t0
                planner_modes[mode_name][-1].append(elapsed)
                print(f"  {mode_name}: {elapsed}")

            if run_fd:
                t0 = time.time()
                run_checked(["fast-downward", "--alias", fd_alias, domain, fname])
                elapsed = time.time() - t0
                fd_times[-1].append(elapsed)
                print(f"  fd: {elapsed}")

        for mode_name in planner_modes:
            planner_mode_avgs[mode_name].append(mean(planner_modes[mode_name][-1]))
        if run_fd:
            fd_avg_times.append(mean(fd_times[-1]))

    print("planner raw times:")
    for mode_name, times in planner_modes.items():
        print(mode_name, times)

    print("planner averages:")
    for mode_name, avgs in planner_mode_avgs.items():
        print(mode_name, avgs)

    systems: dict[str, list[list[float]]] = dict(planner_modes)
    if run_fd:
        print("fd raw times:")
        print(fd_times)
        print("fd averages:")
        print(fd_avg_times)
        systems["fd"] = fd_times

    colors = {
        "totally_ordered": "red",
        "partially_ordered": "green",
        "graphplan": "purple",
        "fd": "blue",
    }

    plt.figure()
    for sys_name, system in systems.items():
        xs = []
        ys = []

        for n, samples in zip(ns, system):
            xs.extend([n] * len(samples))
            ys.extend(samples)

        plt.scatter(xs, ys, color=colors.get(sys_name, "black"), label=sys_name, alpha=0.6)

    for sys_name, avgs in planner_mode_avgs.items():
        plt.plot(ns, avgs, color=colors.get(sys_name, "black"), linewidth=2)
    if run_fd:
        plt.plot(ns, fd_avg_times, color=colors["fd"], linewidth=2)

    plt.xlabel("Input Size")
    plt.ylabel("Runtime (s)")
    plt.title("Runtime vs Input Size")
    plt.legend()
    plt.savefig("runtimes.png")
    plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
