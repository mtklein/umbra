#!/usr/bin/env python3
"""Report critical-path build health from ninja's log + real deps.

Parses out/.ninja_log for timings, `ninja -t query` for the declared
build-edge DAG (which includes the discovered header deps ninja
already accumulates into .ninja_deps), and prints the three slowest
distinct dependency chains with their three slowest individual steps.
"""
import subprocess
import sys
from collections import namedtuple
from pathlib import Path

Step = namedtuple("Step", "start end output")


def load_log(path):
    latest = {}
    for line in Path(path).read_text().splitlines():
        if line and not line.startswith("#"):
            parts = line.split("\t")
            if len(parts) >= 5:
                start, end, _mtime, output, _hash = parts[:5]
                latest[output] = Step(int(start), int(end), output)
    return latest


def query_deps(outputs, chdir):
    """Return output -> list of input outputs (filtered to built targets).

    Log entries whose target has since been dropped from build.ninja are
    silently skipped — `ninja -t query` would reject the whole batch on
    the first unknown name.
    """
    known = set(subprocess.run(
        ["ninja", "-C", chdir, "-t", "targets", "all"],
        capture_output=True, text=True, check=True,
    ).stdout.splitlines())
    known = {line.split(":", 1)[0] for line in known}
    outputs = [o for o in outputs if o in known]
    built = set(outputs)
    proc = subprocess.run(
        ["ninja", "-C", chdir, "-t", "query"] + outputs,
        capture_output=True, text=True, check=True,
    )
    deps = {o: [] for o in outputs}
    current = None
    in_inputs = False
    for line in proc.stdout.splitlines():
        if not line.startswith(" ") and line.endswith(":"):
            current = line[:-1]
            in_inputs = False
        elif line.startswith("  input:"):
            in_inputs = True
        elif line.startswith("  outputs:"):
            in_inputs = False
        elif in_inputs and line.startswith("    ") and current in deps:
            dep = line[4:]
            if dep in built:
                deps[current].append(dep)
    return deps


def critical_path(steps, deps):
    """output -> (cumulative_ms, predecessor_output_or_None)."""
    order = sorted(steps, key=lambda s: s.end)
    cum = {}
    for s in order:
        dur = s.end - s.start
        best_cum, best_pred = 0, None
        for d in deps.get(s.output, ()):
            if d in cum and cum[d][0] > best_cum:
                best_cum, best_pred = cum[d][0], d
        cum[s.output] = (best_cum + dur, best_pred)
    return cum


def walk(end_output, cum, by_output):
    out = []
    cur = end_output
    while cur is not None:
        out.append(by_output[cur])
        cur = cum[cur][1]
    return list(reversed(out))


def main(argv):
    log_path = Path(argv[1] if len(argv) > 1 else "out/.ninja_log")
    chdir = str(log_path.resolve().parent.parent)  # dir containing build.ninja

    steps_by_output = load_log(log_path)
    if not steps_by_output:
        print(f"{log_path}: no build log entries", file=sys.stderr)
        return 1

    deps = query_deps(list(steps_by_output.keys()), chdir)
    steps_by_output = {o: steps_by_output[o] for o in deps}
    cum = critical_path(steps_by_output.values(), deps)

    # Rank terminals by cumulative time; skip chains whose terminal is
    # already on a previously-emitted chain so the three reports are
    # distinct critical paths, not nested prefixes of one.
    terminals = sorted(cum.keys(), key=lambda o: cum[o][0], reverse=True)
    seen = set()
    chains = []
    for t in terminals:
        if t not in seen:
            c = walk(t, cum, steps_by_output)
            chains.append((cum[t][0], c))
            seen.update(s.output for s in c)
            if len(chains) == 3:
                break

    for i, (total, c) in enumerate(chains, 1):
        print(f"Chain {i}: {total/1000:.2f}s over {len(c)} steps")
        top = sorted(c, key=lambda s: s.end - s.start, reverse=True)[:3]
        for s in top:
            print(f"  {(s.end - s.start)/1000:6.2f}s  {s.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
