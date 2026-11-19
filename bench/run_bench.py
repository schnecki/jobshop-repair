#!/usr/bin/env python3
"""Run jsr over instances x rules x breakdown scenarios, collect CSV, plot results.

Usage: python3 bench/run_bench.py [--binary build/jsr] [--instances instances] [--out results]
"""

import argparse
import csv
import io
import random
import subprocess
from pathlib import Path


def read_header(path: Path) -> tuple[int, int]:
    for line in path.read_text().splitlines():
        if line.strip() and not line.startswith("#"):
            jobs, machines = map(int, line.split()[:2])
            return jobs, machines
    raise ValueError(f"no header in {path}")


def run(binary: Path, instance: Path, rule: str, breakdown: str | None,
        improve: bool) -> list[dict]:
    cmd = [str(binary), str(instance), "--rule", rule, "--header"]
    if breakdown:
        cmd += ["--breakdown", breakdown]
    if improve:
        cmd.append("--improve")
    out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    rows = list(csv.DictReader(io.StringIO(out)))
    for r in rows:
        r["breakdown"] = breakdown or ""
    return rows


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", type=Path, default=Path("build/jsr"))
    ap.add_argument("--instances", type=Path, default=Path("instances"))
    ap.add_argument("--out", type=Path, default=Path("results"))
    ap.add_argument("--rules", nargs="+", default=["SPT", "LPT", "MWKR", "FIFO"])
    ap.add_argument("--scenarios", type=int, default=5, help="random breakdowns per instance")
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    args.out.mkdir(parents=True, exist_ok=True)
    rows: list[dict] = []

    for inst in sorted(args.instances.glob("*.txt")):
        _, machines = read_header(inst)
        # Breakdown scenarios are shared across rules for a fair comparison.
        scenarios = []
        for _ in range(args.scenarios):
            m = rng.randrange(machines)
            start = rng.randint(5, 40)
            scenarios.append(f"{m},{start},{start + rng.randint(5, 30)}")
        for rule in args.rules:
            for improve in (False, True):
                for bd in scenarios:
                    rows += run(args.binary, inst, rule, bd, improve)

    csv_path = args.out / "results.csv"
    with csv_path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {len(rows)} rows to {csv_path}")

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed, skipping plot")
        return

    repairs = [r for r in rows if r["phase"] == "repair"]
    groups = [(rule, imp) for rule in args.rules for imp in ("0", "1")]
    labels = [f"{rule}{'+LS' if imp == '1' else ''}" for rule, imp in groups]
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))
    for ax, metric, label in [(axes[0], "makespan", "Makespan after repair"),
                              (axes[1], "total_shift", "Total start-time shift (stability)")]:
        data = [[int(r[metric]) for r in repairs if (r["rule"], r["improve"]) == g]
                for g in groups]
        ax.boxplot(data)
        ax.set_xticks(range(1, len(labels) + 1), labels, rotation=45)
        ax.set_title(label)
    fig.tight_layout()
    fig.savefig(args.out / "repair.png", dpi=150)
    print(f"wrote {args.out / 'repair.png'}")


if __name__ == "__main__":
    main()
