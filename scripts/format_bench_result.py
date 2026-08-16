#!/usr/bin/env python3
"""Wrap raw Google Benchmark JSON dumps into Dahlia's bench history schema
(see docs/REFERENCE.md 3.11). Extend the "benchmarks" section further as perft
timing / TT metrics come online.

When the suite is run with --benchmark_repetitions, Google Benchmark emits
per-repetition rows *plus* mean/median/stddev aggregate rows. Only the median
is recorded here: docs/REFERENCE.md 3.11's noise discipline asks for a median over
repetitions, and keeping the raw rows would make history files grow with the
repetition count for no analytical gain.
"""
import argparse
import json
import platform
import subprocess
import sys
from datetime import datetime, timezone


def git_short_hash() -> str:
	return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], text=True).strip()


def cpu_model() -> str:
	"""A human-identifiable CPU string, not just the architecture.

	docs/REFERENCE.md 3.11 requires the CPU in every result file so cross-machine
	noise stays explainable after the fact -- which matters more now that runs
	come from both a dev laptop and a CI runner. platform.processor() returns
	only the bare machine string ("x86_64", "arm") on both Linux and macOS, so
	it is the last resort rather than the first choice.
	"""
	try:
		with open("/proc/cpuinfo") as f:
			for line in f:
				if line.startswith("model name"):
					return line.split(":", 1)[1].strip()
	except OSError:
		pass

	if sys.platform == "darwin":
		try:
			return subprocess.check_output(
				["sysctl", "-n", "machdep.cpu.brand_string"], text=True
			).strip()
		except (OSError, subprocess.CalledProcessError):
			pass

	return platform.processor() or platform.machine()


def median_runs(gbench: dict) -> list:
	"""The rows worth recording: median aggregates if repetitions were used,
	otherwise the plain single-run rows."""
	entries = gbench.get("benchmarks", [])
	medians = [b for b in entries if b.get("aggregate_name") == "median"]
	if medians:
		return medians
	return [b for b in entries if b.get("run_type", "iteration") == "iteration"]


def bench_name(b: dict) -> str:
	# An aggregate row's "name" carries a "/repeats:N_median" suffix; "run_name"
	# is the bare benchmark name, which is what keeps a repetition-averaged
	# history file comparable to the pre-repetition ones already committed.
	return b.get("run_name") or b["name"]


def microbench_entries(gbench: dict) -> list:
	return [
		{
			"name": bench_name(b),
			"real_time": b["real_time"],
			"cpu_time": b["cpu_time"],
			"time_unit": b["time_unit"],
		}
		for b in median_runs(gbench)
	]


def search_bench_entries(gbench: dict) -> list:
	# search_bench's custom counters (nodes, nps, depth_reached -- see
	# bench/search_bench/bench_search.cpp) are flattened directly onto each
	# benchmark object by Google Benchmark's JSON reporter.
	return [
		{
			"name": bench_name(b),
			"real_time": b["real_time"],
			"cpu_time": b["cpu_time"],
			"time_unit": b["time_unit"],
			"nodes": b.get("nodes"),
			"nodes_per_second": b.get("nps"),
			"depth_reached": b.get("depth_reached"),
		}
		for b in median_runs(gbench)
	]


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("--gbench-json", required=True, help="raw microbench --benchmark_format=json output")
	parser.add_argument("--search-gbench-json", help="raw search_bench --benchmark_format=json output")
	parser.add_argument("--compiler", required=True)
	parser.add_argument("--build-type", required=True)
	parser.add_argument(
		"--cxx-flags",
		default="",
		help="CMAKE_CXX_FLAGS the suite was built with; recorded so a later flag "
		     "change is visible in the history rather than silently shifting every number",
	)
	parser.add_argument(
		"--tag",
		default="",
		help="release tag this run corresponds to, if it marks one",
	)
	parser.add_argument("--repetitions", type=int, default=0, help="repetitions the suite was run with")
	parser.add_argument(
		"--dirty",
		default="false",
		help="true when the working tree had uncommitted changes, meaning `commit` "
		     "below is the measured change's parent rather than the change itself",
	)
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
		"dirty": args.dirty.lower() == "true",
		"tag": args.tag,
		"timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
		"compiler": args.compiler,
		"build": args.build_type,
		"cxx_flags": args.cxx_flags,
		"repetitions": args.repetitions,
		"cpu": cpu_model(),
		"benchmarks": benchmarks,
	}

	with open(args.out, "w") as f:
		json.dump(result, f, indent=2)
		f.write("\n")

	return 0


if __name__ == "__main__":
	sys.exit(main())
