#!/usr/bin/env python3
"""Report critical-path-style build health from ninja's log.

Parses out/.ninja_log and prints the three slowest dependency chains
with the slowest three individual steps on each.  ninja's log records
only start/end timestamps, not the dependency graph, so chains are
inferred: each step's predecessor is taken to be the largest-cumulative
prior step whose end precedes this step's start.  That underestimates
chains on over-provisioned builds (unrelated work overlaps) and
overestimates them on under-provisioned ones, but tracks well enough
to spot compile-time regressions.
"""
import sys
from collections import namedtuple
from pathlib import Path

Step = namedtuple("Step", "start end output")


def load(path):
    latest = {}
    for line in Path(path).read_text().splitlines():
        if line and not line.startswith("#"):
            parts = line.split("\t")
            if len(parts) >= 5:
                start, end, _mtime, output, _hash = parts[:5]
                latest[output] = Step(int(start), int(end), output)
    return sorted(latest.values(), key=lambda s: s.end)


def build_chains(steps):
    """Return cum: output -> (cumulative_ms, predecessor_output_or_None)."""
    cum = {}
    for i, s in enumerate(steps):
        dur = s.end - s.start
        best_cum, best_pred = 0, None
        for j in range(i):
            p = steps[j]
            if p.end <= s.start and cum[p.output][0] > best_cum:
                best_cum, best_pred = cum[p.output][0], p.output
        cum[s.output] = (best_cum + dur, best_pred)
    return cum


def walk_chain(end_output, cum, by_output):
    out = []
    cur = end_output
    while cur is not None:
        out.append(by_output[cur])
        cur = cum[cur][1]
    return list(reversed(out))


def main(argv):
    path = argv[1] if len(argv) > 1 else "out/.ninja_log"
    steps = load(path)
    if not steps:
        print(f"{path}: no build log entries", file=sys.stderr)
        return 1
    by_output = {s.output: s for s in steps}
    cum = build_chains(steps)

    # Rank terminals by cumulative time; skip chains whose terminal is
    # already on a previously-emitted chain so the report shows three
    # distinct critical paths instead of nested prefixes of one.
    terminals = sorted(cum.keys(), key=lambda o: cum[o][0], reverse=True)
    seen = set()
    chains = []
    for t in terminals:
        if t not in seen:
            c = walk_chain(t, cum, by_output)
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
