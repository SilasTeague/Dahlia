#pragma once

// Runs a single fixed-depth search wrapped in a Sampler, then prints a
// self/total function-time report. Only built on macOS (see sampler.h) --
// dispatched from main() when invoked as `dahlia --profile [depth D] [fen ...]`.
int run_profile_mode(int argc, char** argv);
