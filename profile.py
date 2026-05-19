import subprocess
import time
import matplotlib.pyplot as plt

ns = range(1,7)
trials = 3

fname = "problems/blocksworld/random.pddl"
domain = "problems/blocksworld/domain.pddl"
fd_alias = "seq-opt-lmcut"

fd_times = []
smt_times = []
smt_cpp_times = []

fd_avg_times = []
smt_avg_times = []
smt_cpp_avg_times = []

for n in ns:
    proc = subprocess.getoutput(f"python3 ./problems/blocksworld/gen_blockworld.py {n} 1 > {fname}")

    fd_times.append([])
    smt_times.append([])
    smt_cpp_times.append([])

    for _ in range(trials):
        t0 = time.time()
        out_py = subprocess.run(["python3", "./translation/translate.py", domain, fname], capture_output=True)
        elapsed = time.time() - t0
        print(f"{n} blocks - smt: {elapsed}")
        smt_times[-1].append(elapsed)
        subprocess.run(["python3", "./translation/validate.py", domain, fname, "sas_plan"]).check_returncode()

        t0 = time.time()
        subprocess.run(["python3", "./translation/parse.py", domain, fname])
        out_cpp = subprocess.run(["./main", "ir.json"], capture_output=True)
        elapsed = time.time() - t0
        print(f"{n} blocks - sm (cpp): {elapsed}")
        smt_cpp_times[-1].append(elapsed)
        subprocess.run(["python3", "./translation/validate.py", domain, fname, "sas_plan"]).check_returncode()

        t0 = time.time()
        subprocess.run(["fast-downward", "--alias", fd_alias, domain, fname])
        elapsed = time.time() - t0
        print(f"{n} blocks - fd: {elapsed}")
        fd_times[-1].append(elapsed)
        subprocess.run(["python3", "./translation/validate.py", domain, fname, "sas_plan"]).check_returncode()

    fd_avg_times.append(sum(fd_times[-1]) / trials)
    smt_avg_times.append(sum(smt_times[-1]) / trials)
    smt_cpp_avg_times.append(sum(smt_cpp_times[-1]) / trials)

print(fd_times)
print(smt_times)
print(smt_cpp_times)

print(fd_avg_times)
print(smt_avg_times)
print(smt_cpp_avg_times)


systems = {"fd": fd_times, "smt": smt_times, "smt_cpp": smt_cpp_times}
#systems = {"fd": fd_times, "smt_cpp": smt_cpp_times}

colors = ['blue', 'red']

for sys_idx, (sys_name, system) in enumerate(systems.items()):
    xs = []
    ys = []

    for i, samples in enumerate(system):
        xs.extend([i] * len(samples))   # repeat input size for each sample
        ys.extend(samples)

    plt.scatter(xs, ys, color=colors[sys_idx % len(colors)], label=f"{sys_name}", alpha=0.6)

plt.xlabel("Input Size")
plt.ylabel("Runtime")
plt.title("Runtime vs Input Size")
plt.legend()
plt.savefig("runtimes.png")
plt.show()

