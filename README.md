# Dahlia

[![CI](https://github.com/SilasTeague/Dahlia/actions/workflows/ci.yml/badge.svg)](https://github.com/SilasTeague/Dahlia/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

A UCI chess engine written from scratch in modern C++ — bitboard move
generation, Zobrist-hashed make/unmake, an iterative-deepening
principal-variation search with a transposition table, tapered evaluation,
null-move pruning, aspiration windows and late move reductions, and an
asynchronous UCI loop that stays responsive mid-search.

## Play it

| | |
|---|---|
| ▶ **[silasteague.com/chess](https://silasteague.com/chess)** | Pick a side and a time control and play in the browser. Runs the same statically-linked binary this repository publishes on every `v*` tag. |
| ♞ **[lichess.org/@/DahliaBot](https://lichess.org/@/DahliaBot)** | **2113 blitz** over 149 games (88 W / 16 D / 45 L) as of 2026-08-20. A rating still in motion over a small sample — always quoted with its date and game count. |

## Build

Requires CMake ≥ 3.20 and a C++20 compiler. Catch2 and Google Benchmark are
fetched automatically; there is nothing to install first.

```bash
git clone https://github.com/SilasTeague/Dahlia.git
cd Dahlia

cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

./build/debug/dahlia
```

| Preset | Purpose |
|---|---|
| `debug` | day-to-day iteration; runs the full test suite |
| `debug-asan` | ASan + UBSan |
| `debug-tsan` | TSan, for the search/reader thread boundary |
| `release` | portable `RelWithDebInfo`; what CI benchmarks and what ships |
| `release-native` | `-march=native`, local benchmarking only, never shipped |

The `Makefile` carries no build logic and just delegates to the presets:
`make`, `make test`, `make run`, `make PRESET=release build`.

Prebuilt statically-linked Linux binaries (`x64` and `arm64`, with a
`SHA256SUMS` manifest) are attached to every
[release](https://github.com/SilasTeague/Dahlia/releases).

## Run

Dahlia speaks UCI, so it loads into Arena, Cute Chess, BanksiaGUI, or
`cutechess-cli`. By hand:

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
...
bestmove g1f3
```

Supported commands, options and time-management behaviour: [docs/uci.md](docs/uci.md).

## How it's built

`src/` is one directory per module, with dependencies pointing in exactly one
direction:

```
core  ←  movegen  ←  position  ←  search  ←  uci
                         ↑
                       eval
```

Two goals drive every decision: a correct and progressively stronger engine, and
a demonstration of professional systems practice — perft-verified correctness, a
benchmark harness with committed history, a multi-compiler and sanitizer CI
matrix, and decision records explaining why each contested choice went the way it
did.

The operating discipline is **Correctness → Measurement → Optimization, never
the reverse**. Dahlia deliberately ships the slow, obviously-correct
implementation first and only replaces it once a benchmark baseline exists to
measure the replacement against. Sliding attacks stayed a loop/bit-shift ray
walk from Milestone 1 to Milestone 7 for exactly that reason, so the magic
bitboards that eventually replaced them could be quoted as a 16× lookup speedup
against the baseline recorded when the ray walker shipped rather than asserted; the
transposition table was added after plain alpha-beta worked, for the same
reason.

Every performance claim is reproducible from committed benchmark JSON and pinned
positions. The full record, milestone by milestone, is in
[docs/results.md](docs/results.md).

## Documentation

| | |
|---|---|
| [Architecture](docs/architecture.md) | module graph, data flow, structural properties |
| [Move generation](docs/movegen.md) | leaper tables, magic bitboards, where the magic numbers come from |
| [Search](docs/search.md) | ordering, quiescence, TT, PVS, null-move, aspiration, LMR |
| [Evaluation](docs/evaluation.md) | tapered piece-square tables and game phase |
| [Results](docs/results.md) | every measured number, with the commits behind it |
| [Testing](docs/testing.md) · [Benchmarking](docs/benchmarking.md) | how correctness and performance are checked |
| [UCI](docs/uci.md) · [Releases](docs/releases.md) | protocol behaviour, and how binaries get built and shipped |
| [Roadmap](docs/roadmap.md) · [Limitations](docs/limitations.md) | what's done, what's next, what it doesn't do |
| [REFERENCE.md](docs/REFERENCE.md) · [ADRs](docs/adr) | the specification, and the contested decisions |

Full index: [docs/](docs/README.md).

## License

MIT — see [LICENSE](LICENSE).

---

<p align="center">
  <a href="https://silasteague.com/chess"><b>Play Dahlia →</b></a>
</p>
