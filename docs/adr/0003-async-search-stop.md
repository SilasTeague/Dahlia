# 0003 — Asynchronous search; reject a concurrent `go` instead of queueing it

## Context

`run_uci_loop` called `search::think` synchronously on the reader thread:
the entire engine blocked for the duration of a `go`. This made `stop` a
documented no-op (by the time a `stop` line was read, any prior `go` had
already returned), made `isready` unanswerable mid-search, and made `go
infinite` unusable — REFERENCE.md 3.9 explicitly flags "not handling
`infinite`" as a pitfall, and a GUI's analysis mode depends on it working.

`search::think`'s inner loop (`negamax`) already checked a `stop_requested`
flag on a node-count modulus and returned early when it was set
(REFERENCE.md 3.8's "not respecting stop/time checks frequently enough"
pitfall was already handled) — what was missing was any way for something
*outside* the search to set that flag while the search was running.

## Decision

`go` now runs `search::think` on a dedicated `std::thread` owned by a new
`Engine` class (`src/uci/uci.cpp`); the reader loop only ever spawns/signals
it, never blocks on it except at `quit`/session end. Concretely:

- `SearchState::stop_requested` became `std::atomic<bool>&`, bound to an
  atomic owned by `Engine`. `stop` sets it and returns immediately (does not
  join); the search thread observes it within a few nodes (the existing
  modulus-checked, unconditional-per-node check already handled this) and
  returns the best move found so far.
- `search::think` no longer writes to an `ostream` itself. It takes an
  `InfoCallback` (`std::function<void(const std::string&)>`) instead, called
  once per completed depth with a fully-formatted line. `Engine` supplies a
  callback that locks a mutex shared with every other line the engine
  writes (`uci`/`readyok`/`bestmove`/...), so a search thread's `info` line
  can never interleave mid-line with the reader thread's output
  (REFERENCE.md 2.4's "no stray output" constraint extends to "no torn
  lines" once two threads can both write).
- `isready` always answers immediately — it never touches the search
  thread or waits on it, so it's a true mid-search liveness check rather
  than a response that happens to arrive quickly.
- `quit` (and `~Engine`, for the case where the input stream just ends
  without an explicit `quit`) sets `stop_requested` and *joins* before
  returning, so the process never exits with a search thread still running.
- **A second `go` that arrives while a search is already in flight is
  rejected (silently ignored), not queued.** Considered and rejected:
  queueing the new `go`'s position/limits and starting it automatically
  once the in-flight search finishes.

Rejection was chosen because:
1. A UCI-compliant GUI never sends a second `go` before receiving
   `bestmove` for the first — REFERENCE.md 3.10 already leans on this
   assumption for `position`/`setoption`/`ucinewgame` arriving mid-search.
   Handling the case at all is a defensive-programming choice, not a
   protocol requirement, so the simpler behavior wins.
2. Queueing means deciding what a *third* `go` does while one is queued
   (replace the queued one? reject? queue a list?), and gives the queued
   search a start time the caller never asked for and can't predict —
   scope creep for a scenario a compliant client won't trigger.
3. Rejecting keeps the invariant "at most one `bestmove` per `go` the
   engine actually acted on" trivially true. A misbehaving client that
   fires a second `go` early gets exactly the first search's result and
   can send another `go` once it sees `bestmove` — recoverable, and
   observably different from a silent hang.

## Consequences

- `search::think`'s signature changed: it now takes `std::atomic<bool>&
  stop_requested` (required) and `const InfoCallback&` (optional) instead
  of `std::ostream*`. Every caller (`uci.cpp`, `tests/unit/test_search.cpp`,
  `bench/search_bench/bench_search.cpp`) passes its own local
  `std::atomic<bool>` when it doesn't care about external stop.
- `dahlia_core` now links `Threads::Threads`; `CMakeLists.txt` gained
  `find_package(Threads REQUIRED)`.
- The engine gained its first real concurrency, so a data race is now a
  real failure mode instead of a hypothetical one. A ThreadSanitizer preset
  (`debug-tsan`, `cmake/Sanitizers.cmake`'s `dahlia_enable_tsan`) and CI leg
  (`.github/workflows/ci.yml`'s `tsan` job) were added alongside this change
  rather than deferred, since REFERENCE.md 2.2 already anticipated exactly
  this trigger ("TSan is lower priority until/unless the engine gains real
  multithreading").
- A client that violates the "one `go` in flight at a time" assumption
  (sends a second `go` before `bestmove`) loses that `go` silently rather
  than erroring or queueing — acceptable per REFERENCE.md 3.10's framing of
  UCI as "a solved, well-specified protocol" where handling client protocol
  violations gracefully is explicitly out of scope.
