import subprocess
import sys
import time

import matplotlib.pyplot as plt


def usage() -> int:
    sys.stderr.write(
        "Usage: python3 scripts/profile.py <min_n> <max_n> <trials> [domain.pddl] [planner] [fd_alias]\n"
        "  min_n/max_n: inclusive problem sizes to benchmark\n"
        "  trials: number of repetitions per size\n"
        "  domain.pddl: planning domain file (default: problems/blocksworld/domain.pddl)\n"
        "  planner: C++ planner executable (default: ./main)\n"
        "  fd_alias: Fast Downward alias (default: seq-opt-lmcut)\n"
    )
    return 1


def main() -> int:
    if len(sys.argv) not in (4, 5, 6, 7):
        return usage()

    min_n = int(sys.argv[1])
    max_n = int(sys.argv[2])
    trials = int(sys.argv[3])
    domain = sys.argv[4] if len(sys.argv) >= 5 else "problems/blocksworld/domain.pddl"
    planner = sys.argv[5] if len(sys.argv) >= 6 else "./build/planner"
    fd_alias = sys.argv[6] if len(sys.argv) >= 7 else "seq-opt-lmcut"

    if min_n > max_n:
        sys.stderr.write("min_n must be <= max_n\n")
        return 1

    ns = range(min_n, max_n + 1)
    fname = "problems/blocksworld/random.pddl"

    fd_times = []
    smt_cpp_times = []
    fd_avg_times = []
    smt_cpp_avg_times = []

    for n in ns:
        subprocess.getoutput(
            f"python3 ./problems/blocksworld/gen_blockworld.py {n} 1 > {fname}"
        )

        fd_times.append([])
        smt_cpp_times.append([])

        for _ in range(trials):
            t0 = time.time()
            subprocess.run(["python3", "./scripts/parse.py", domain, fname]).check_returncode()
            subprocess.run([planner, "ir.json"], capture_output=True).check_returncode()
            elapsed = time.time() - t0
            print(f"{n} blocks - smt_cpp: {elapsed}")
            smt_cpp_times[-1].append(elapsed)
            subprocess.run(
                ["python3", "./scripts/validate.py", domain, fname, "sas_plan"]
            ).check_returncode()

            t0 = time.time()
            subprocess.run(["fast-downward", "--alias", fd_alias, domain, fname]).check_returncode()
            elapsed = time.time() - t0
            print(f"{n} blocks - fd: {elapsed}")
            fd_times[-1].append(elapsed)
            subprocess.run(
                ["python3", "./scripts/validate.py", domain, fname, "sas_plan"]
            ).check_returncode()

        fd_avg_times.append(sum(fd_times[-1]) / trials)
        smt_cpp_avg_times.append(sum(smt_cpp_times[-1]) / trials)

    print(fd_times)
    print(smt_cpp_times)
    print(fd_avg_times)
    print(smt_cpp_avg_times)

    systems = {"fd": fd_times, "smt_cpp": smt_cpp_times}
    colors = ["blue", "red"]

    for sys_idx, (sys_name, system) in enumerate(systems.items()):
        xs = []
        ys = []

        for i, samples in enumerate(system):
            xs.extend([i] * len(samples))
            ys.extend(samples)

        plt.scatter(xs, ys, color=colors[sys_idx % len(colors)], label=sys_name, alpha=0.6)

    plt.xlabel("Input Size")
    plt.ylabel("Runtime")
    plt.title("Runtime vs Input Size")
    plt.legend()
    plt.savefig("runtimes.png")
    plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

