# Releases and deployment

Pushing a `v*` tag runs [`release.yml`](../.github/workflows/release.yml), which:

1. Gates on the unit + perft suite.
2. Builds **statically linked** binaries for `linux-x64` and `linux-arm64` —
   natively on each architecture, no emulation — using
   [the same script a laptop build uses](../scripts/build-release.sh), so CI
   cannot drift from local output.
3. Smoke-tests the exact artifact that ships, inside the build container: a full
   UCI handshake plus a real search from a real position, asserting a
   well-formed `bestmove` rather than `bestmove 0000`. A binary that starts and
   then finds no move would sail past a bare `uciok` check.
4. Publishes the binaries with a `SHA256SUMS` manifest.

`release.yml` is the only tag-triggered workflow; performance is measured
locally before a tag rather than on a CI runner
([ADR 0004](adr/0004-node-counts-in-ci-timing-local.md)).

**Static linking is load-bearing, not incidental.** Deployment hosts span glibc
2.31 through 2.36, and a dynamically linked build simply refuses to start on
anything older than the build image.

## Tags

| Tag | Contents |
|---|---|
| `v1.0` | first playable release |
| `v2.0` | static Linux release binaries |
| `v2.1` | Milestone 5 — PVS, tapered PSTs, null-move pruning |
| `v2.2` | Milestone 6 — aspiration windows, late move reductions |

## Deployment

Two live instances run the published binaries:

- [silasteague.com/chess](https://silasteague.com/chess) — the web demo. Pick a
  side and a time control and play the engine in this repository.
- [lichess.org/@/DahliaBot](https://lichess.org/@/DahliaBot) — the rated BOT
  account, which is where the absolute strength figure in
  [results.md](results.md#strength) comes from.

The website's instance polls the checksum manifest and pulls new binaries
itself. Nothing in this repository holds credentials for, or pushes to, a
server.
