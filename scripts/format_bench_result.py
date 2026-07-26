#!/usr/bin/env python3
"""Wrap a raw Google Benchmark JSON dump into Dahlia's bench history schema
(see REFERENCE.md 3.11). Extend the "benchmarks" section as search_bench /
perft timing / TT metrics come online — this only knows about microbench today.
"""
import argparse
import json
import platform
import subprocess
import sys
from datetime import datetime, timezone


def git_short_hash() -> str:
	return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], text=True).strip()


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("--gbench-json", required=True, help="raw --benchmark_format=json output")
	parser.add_argument("--compiler", required=True)
	parser.add_argument("--build-type", required=True)
	parser.add_argument("--out", required=True)
	args = parser.parse_args()

	with open(args.gbench_json) as f:
		gbench = json.load(f)

	microbench = [
		{
			"name": b["name"],
			"real_time": b["real_time"],
			"cpu_time": b["cpu_time"],
			"time_unit": b["time_unit"],
		}
		for b in gbench.get("benchmarks", [])
	]

	result = {
		"commit": git_short_hash(),
		"timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
		"compiler": args.compiler,
		"build": args.build_type,
		"cpu": platform.processor() or platform.machine(),
		"benchmarks": {
			"microbench": microbench,
		},
	}

	with open(args.out, "w") as f:
		json.dump(result, f, indent=2)
		f.write("\n")

	return 0


if __name__ == "__main__":
	sys.exit(main())
