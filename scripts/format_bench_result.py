#!/usr/bin/env python3
"""Wrap raw Google Benchmark JSON dumps into Dahlia's bench history schema
(see REFERENCE.md 3.11). Extend the "benchmarks" section further as perft
timing / TT metrics come online.
"""
import argparse
import json
import platform
import subprocess
import sys
from datetime import datetime, timezone


def git_short_hash() -> str:
	return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], text=True).strip()


def microbench_entries(gbench: dict) -> list:
	return [
		{
			"name": b["name"],
			"real_time": b["real_time"],
			"cpu_time": b["cpu_time"],
			"time_unit": b["time_unit"],
		}
		for b in gbench.get("benchmarks", [])
	]


def search_bench_entries(gbench: dict) -> list:
	# search_bench's custom counters (nodes, nps, depth_reached -- see
	# bench/search_bench/bench_search.cpp) are flattened directly onto each
	# benchmark object by Google Benchmark's JSON reporter.
	return [
		{
			"name": b["name"],
			"real_time": b["real_time"],
			"cpu_time": b["cpu_time"],
			"time_unit": b["time_unit"],
			"nodes": b.get("nodes"),
			"nodes_per_second": b.get("nps"),
			"depth_reached": b.get("depth_reached"),
		}
		for b in gbench.get("benchmarks", [])
	]


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("--gbench-json", required=True, help="raw microbench --benchmark_format=json output")
	parser.add_argument("--search-gbench-json", help="raw search_bench --benchmark_format=json output")
	parser.add_argument("--compiler", required=True)
	parser.add_argument("--build-type", required=True)
	parser.add_argument("--out", required=True)
	args = parser.parse_args()

	with open(args.gbench_json) as f:
		microbench_gbench = json.load(f)

	benchmarks = {
		"microbench": microbench_entries(microbench_gbench),
	}

	if args.search_gbench_json:
		with open(args.search_gbench_json) as f:
			search_gbench = json.load(f)
		benchmarks["search_bench"] = search_bench_entries(search_gbench)

	result = {
		"commit": git_short_hash(),
		"timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
		"compiler": args.compiler,
		"build": args.build_type,
		"cpu": platform.processor() or platform.machine(),
		"benchmarks": benchmarks,
	}

	with open(args.out, "w") as f:
		json.dump(result, f, indent=2)
		f.write("\n")

	return 0


if __name__ == "__main__":
	sys.exit(main())
