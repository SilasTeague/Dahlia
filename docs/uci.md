# UCI

What Dahlia supports of the Universal Chess Interface, and how it behaves where
the protocol leaves room.

Dahlia loads into Arena, Cute Chess, BanksiaGUI, and `cutechess-cli`.
Specification: [REFERENCE.md §3.10](REFERENCE.md#310-uci-protocol-uci).

---

## Command support

| Command | Status |
|---|---|
| `uci`, `isready`, `ucinewgame`, `quit` | ✅ — `isready` answers immediately even mid-search |
| `position startpos \| fen <fen> [moves ...]` | ✅ |
| `go depth \| movetime \| wtime \| btime \| winc \| binc \| movestogo \| infinite` | ✅ |
| `stop` | ✅ — returns the best move from the last completed depth |
| `setoption name Hash value <MB>` | ✅ — 1–1024 MB, default 16 |
| `setoption name Move Overhead value <ms>` | ✅ — 0–1000 ms, default 10 |
| `info` lines | partial — `depth`, `score cp`/`score mate`, `nodes`, `nps`, `time`; no `seldepth`, `pv`, or `hashfull` yet |
| `go nodes`, `searchmoves`, `ponder`, `Threads` | not implemented |

Unrecognized tokens are ignored rather than treated as errors — they are simply
not matched by any branch of the `go` parser, so `go movetime 1000 ponder`
searches for a second and ignores the rest.

## Options

| Option | Type | Default | Range |
|---|---|---:|---|
| `Hash` | spin | 16 MB | 1–1024 |
| `Move Overhead` | spin | 10 ms | 0–1000 |

Out-of-range values are ignored, leaving the current setting in place. Option
names can be multi-word (`Move Overhead`), so the name tokens are rejoined with
the single spaces the protocol splits them on.

`Hash` resizes the transposition table, rounding down to a power-of-two entry
count and discarding existing entries. Note that node counts legitimately change
with hash size — see
[benchmarking.md](benchmarking.md#node-counts-are-a-test-not-a-benchmark).

## Time management

The `go` clock parameters become a single-move budget:

```
time_left / 20 + increment / 2,  capped at half the remaining clock
```

`movestogo`, when the GUI supplies it, only *tightens* the divisor — below 20
moves to the next time control the budget becomes
`time_left / movestogo + increment / 2`, so the clock is not left unspent at the
control. A bare `go` with no time control at all gets a 200 ms anytime budget
rather than searching forever.

Every GUI-imposed budget — `movetime` included — is then reduced by
`Move Overhead` so the reply lands *before* the deadline rather than exactly on
it. Everything after the search returns happens on the GUI's clock, and a GUI
configured with a zero time margin (cutechess-cli's default `timemargin`) scores
an on-the-deadline reply as a forfeit. Raise the option for high-latency
transports; the reserve never shrinks a budget below 1 ms.

Full detail in [search.md](search.md#time-management).

## Concurrency

`go` dispatches the search onto its own thread, so the stdin read loop is never
blocked for the duration of a search. Consequences worth knowing:

- `stop`, `isready` and `quit` are all answered mid-search.
- A second `go` arriving while a search is in flight is **rejected, not
  queued**. No compliant GUI produces one; queueing would mean answering "what
  does a *third* `go` do" ([ADR 0003](adr/0003-async-search-stop.md)).
- `quit` stops and joins the search thread rather than exiting out from under
  it. So does the destructor, so an input stream that simply runs out without a
  `quit` is also clean.
- All output — the search thread's `info` lines and the reader thread's
  `readyok`/`bestmove` — passes through one mutex, so lines can interleave but
  never tear. A GUI that receives a torn line hangs.
- `ucinewgame`, `setoption` and `position` are not guarded against a concurrent
  search, because UCI-compliant GUIs only send them between a `bestmove` and the
  next `go`.

`ucinewgame` resets the position, clears the transposition table, and clears the
move-ordering history table — carried into an unrelated game, that table is
stale bias rather than a head start.

## A session

```
$ ./build/release/dahlia
uci
id name Dahlia
id author Silas Teague
option name Hash type spin default 16 min 1 max 1024
option name Move Overhead type spin default 10 min 0 max 1000
uciok
position startpos moves e2e4 e7e5
go movetime 1000
info depth 1 score cp 0 nodes 30 nps 30000 time 0
info depth 2 score cp 0 nodes 145 nps 145000 time 0
...
bestmove g1f3
```

One `info` line is emitted per completed depth. Mate scores are reported in
*moves* to mate, not plies, as the protocol requires.

`bestmove 0000` is emitted only when the position has no legal move at all
(checkmate or stalemate); the release smoke test asserts against it specifically,
because a binary that starts and then finds no move would sail past a bare
`uciok` check.
