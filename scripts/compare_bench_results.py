#!/usr/bin/env python3
"""Read the benchmark history under bench/results/ and show how performance has
moved (docs/REFERENCE.md 3.11).

Two views:

  --latest    the most recent run against the one before it. This is the
              "what did the change I just made do" view, and is what
              run_benchmarks.sh prints automatically after a run.

  --history   every recorded run, oldest first, one table per benchmark, with
              run-over-run deltas and a total since the first record. This is
              the "how far has the engine come" view.

Timing numbers are only meaningful within one machine, so both views warn when
consecutive runs disagree on CPU, compiler, build type, or compiler flags.
Node counts are exempt: they are deterministic for a given engine and position,
so they stay comparable no matter what recorded them, and any change in them is
a real change in the search tree rather than measurement noise.
"""
import argparse
import glob
import json
import os
import sys

DEFAULT_HISTORY_DIR = os.path.join(
	os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "bench", "results", "history")

# The fields that have to match for a *time* comparison to mean anything.
ENVIRONMENT_FIELDS = ("cpu", "compiler", "build", "cxx_flags")

# Each metric: key into the result entry, column label, whether it carries the
# entry's time unit, whether it's deterministic (exempt from the noise band),
# and which direction counts as an improvement.
SEARCH_METRICS = (
	{"key": "nodes", "label": "Nodes", "is_time": False, "deterministic": True, "show_absolute": True},
	{"key": "real_time", "label": "Time", "is_time": True, "deterministic": False},
	# Derived in add_derived_nps(), not read from the file -- see that docstring.
	{"key": "nps", "label": "NPS", "is_time": False, "deterministic": False,
	 "higher_is_better": True},
)
MICRO_METRICS = ({"key": "real_time", "label": None, "is_time": True, "deterministic": False},)

SUITES = (
	("search_bench", "Search (whole engine)", SEARCH_METRICS),
	("microbench", "Microbenchmarks", MICRO_METRICS),
)


TIME_UNIT_SECONDS = {"ns": 1e-9, "us": 1e-6, "ms": 1e-3, "s": 1.0}


def add_derived_nps(run: dict) -> None:
	"""Compute nodes/second from the node count and time rather than reading the
	recorded `nodes_per_second`.

	Two reasons. It keeps the three columns internally consistent, since all
	three then come from the same pair of medians. And it is correct for runs
	recorded before 2026-08-14, when the benchmark's rate counter divided one
	iteration's node count by the time taken by *all* iterations -- those files
	have a wrong `nodes_per_second` but correct `nodes` and `real_time`, so
	deriving repairs the whole history without editing a single record.
	"""
	for entry in run.get("benchmarks", {}).get("search_bench", []):
		nodes = entry.get("nodes")
		seconds = (entry.get("real_time") or 0.0) * TIME_UNIT_SECONDS.get(entry.get("time_unit", ""), 0.0)
		entry["nps"] = nodes / seconds if nodes and seconds else None


def load_history(directory: str) -> list:
	runs = []
	for path in glob.glob(os.path.join(directory, "*.json")):
		with open(path) as f:
			run = json.load(f)
		add_derived_nps(run)
		runs.append(run)
	runs.sort(key=lambda r: r.get("timestamp", ""))
	return runs


def entries(run: dict, suite: str) -> dict:
	return {e["name"]: e for e in run.get("benchmarks", {}).get(suite, [])}


def benchmark_names(runs: list, suite: str) -> list:
	"""Every benchmark name ever recorded in this suite, in first-seen order."""
	names = []
	for run in runs:
		for name in entries(run, suite):
			if name not in names:
				names.append(name)
	return names


def pct(before, after):
	if before in (None, 0) or after is None:
		return None
	return (after - before) / before * 100.0


def fmt_value(value, entry: dict, is_time: bool) -> str:
	if value is None:
		return "—"
	if is_time:
		return f"{value:.4g} {entry.get('time_unit', '')}".strip()
	return f"{value:,.0f}"


def fmt_pct(delta) -> str:
	if delta is None:
		return "—"
	if delta == 0:
		return "0.0%"
	return f"{delta:+.1f}%"


def label(run: dict) -> str:
	date = run.get("timestamp", "")[:10] or "?"
	commit = run.get("commit", "?")
	tag = run.get("tag")
	return f"{date} `{commit}`" + (f" **{tag}**" if tag else "")


def environment_changes(previous: dict, current: dict) -> list:
	return [
		(field, previous.get(field, "?"), current.get(field, "?"))
		for field in ENVIRONMENT_FIELDS
		if previous.get(field, "") != current.get(field, "")
	]


def environment_warning(changes: list) -> list:
	if not changes:
		return []
	lines = ["> [!WARNING]",
	          "> The measurement environment changed between these runs, so the **time**",
	          "> columns are not like-for-like. Node counts are unaffected.",
	          ">"]
	lines += [f"> - `{field}`: `{was}` → `{now}`" for field, was, now in changes]
	return lines + [""]


def table(header: list, rows: list) -> list:
	return (["| " + " | ".join(header) + " |",
	          "|" + "|".join(["---"] * len(header)) + "|"]
	         + ["| " + " | ".join(row) + " |" for row in rows])


def render_latest(runs: list, tolerance: float) -> list:
	previous, current = runs[-2], runs[-1]
	lines = [f"### {label(previous)}  →  {label(current)}", ""]
	lines += environment_warning(environment_changes(previous, current))

	for suite, heading, metrics in SUITES:
		before_entries, after_entries = entries(previous, suite), entries(current, suite)
		if not after_entries:
			continue

		rows = []
		for name in benchmark_names([previous, current], suite):
			before, after = before_entries.get(name), after_entries.get(name)
			# Rows for benchmarks present in only one of the two runs are built to
			# the same width as the rest of the table -- a hardcoded cell count
			# silently produces a ragged row as soon as a suite gains a metric.
			if after is None:
				rows.append([f"`{name}`"] + ["—"] * (2 * len(metrics)) + ["removed"])
				continue
			if before is None:
				# A new benchmark has nothing to compare against, but the run
				# that introduces it is still the run that records it, so its
				# values are printed with the deltas left empty.
				row = [f"`{name}`"]
				for metric in metrics:
					row += [fmt_value(after.get(metric["key"]), after, metric["is_time"]), "—"]
				rows.append(row + ["new"])
				continue

			row = [f"`{name}`"]
			notes = []
			for metric in metrics:
				delta = pct(before.get(metric["key"]), after.get(metric["key"]))
				row += [fmt_value(after.get(metric["key"]), after, metric["is_time"]), fmt_pct(delta)]
				notes.append(verdict(delta, tolerance, metric["deterministic"],
				                      metric.get("higher_is_better", False)))
			# Deduplicated: several metrics all improving is still one "✅".
			rows.append(row + [" ".join(dict.fromkeys(n for n in notes if n))])

		header = ["Benchmark"]
		for metric in metrics:
			header += [metric["label"] or "Time", "Δ"]

		# Drop the flag column entirely when nothing in this suite was flagged,
		# rather than printing a column of blanks.
		if not any(row[-1].strip() for row in rows):
			rows = [row[:-1] for row in rows]
		else:
			header += [""]

		lines += [f"#### {heading}", ""] + table(header, rows) + [""]

	lines += [f"_Time deltas within ±{tolerance:g}% are treated as noise. "
	           "Node counts are deterministic, so any change in them is real._"]
	return lines


def verdict(delta, tolerance: float, deterministic: bool, higher_is_better: bool = False) -> str:
	if delta is None or delta == 0:
		return ""
	if not deterministic and abs(delta) < tolerance:
		return ""
	improved = delta > 0 if higher_is_better else delta < 0
	return "✅" if improved else "⚠️"


def render_history(runs: list, only: str) -> list:
	lines = [f"### Benchmark history — {len(runs)} run{'' if len(runs) == 1 else 's'}, "
	          f"{runs[0].get('timestamp', '?')[:10]} to {runs[-1].get('timestamp', '?')[:10]}", ""]

	environments = {tuple(run.get(f, "") for f in ENVIRONMENT_FIELDS) for run in runs}
	if len(environments) > 1:
		lines += ["> [!NOTE]",
		           "> Not every run in this history shares a measurement environment, so the",
		           "> time columns span more than one series. Node counts are unaffected.",
		           ""]

	for suite, heading, metrics in SUITES:
		names = [n for n in benchmark_names(runs, suite) if not only or only in n]
		if not names:
			continue
		lines += [f"## {heading}", ""]

		for name in names:
			recorded = [(run, entries(run, suite)[name])
			             for run in runs if name in entries(run, suite)]
			if not recorded:
				continue

			rows = []
			for index, (run, entry) in enumerate(recorded):
				row = [label(run)]
				for metric in metrics:
					key = metric["key"]
					previous_value = recorded[index - 1][1].get(key) if index else None
					row += [fmt_value(entry.get(key), entry, metric["is_time"]),
					         fmt_pct(pct(previous_value, entry.get(key)))]
				rows.append(row)

			header = ["Run"]
			for metric in metrics:
				header += [metric["label"] or "Time", "Δ"]

			lines += [f"#### `{name}`", ""] + table(header, rows)

			if len(recorded) > 1:
				first, last = recorded[0][1], recorded[-1][1]
				totals = [f"{(metric['label'] or 'time').lower()} "
				           f"{fmt_pct(pct(first.get(metric['key']), last.get(metric['key'])))}"
				           for metric in metrics]
				lines += ["", f"_Since first record: {', '.join(totals)}._"]
			lines += [""]

	return lines


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__,
	                                  formatter_class=argparse.RawDescriptionHelpFormatter)
	view = parser.add_mutually_exclusive_group()
	view.add_argument("--latest", action="store_true",
	                   help="most recent run vs. the one before it (default)")
	view.add_argument("--history", action="store_true", help="every run, oldest first")
	parser.add_argument("--dir", default=DEFAULT_HISTORY_DIR, help="history directory")
	parser.add_argument("--benchmark", default="",
	                     help="with --history, only benchmarks whose name contains this")
	parser.add_argument("--tolerance", type=float, default=5.0,
	                     help="percent band below which a time delta is treated as noise")
	parser.add_argument("--out", help="write Markdown here instead of stdout")
	args = parser.parse_args()

	runs = load_history(args.dir)
	if not runs:
		print(f"No benchmark history in {args.dir}.", file=sys.stderr)
		return 1

	if args.history:
		lines = render_history(runs, args.benchmark)
	elif len(runs) < 2:
		lines = [f"Only one run recorded ({label(runs[0])}); nothing to compare against yet."]
	else:
		lines = render_latest(runs, args.tolerance)

	report = "\n".join(lines) + "\n"
	if args.out:
		with open(args.out, "w") as f:
			f.write(report)
	else:
		sys.stdout.write(report)
	return 0


if __name__ == "__main__":
	sys.exit(main())
