#include <benchmark/benchmark.h>

#include "eval/evaluate.h"
#include "movegen/attacks.h"
#include "position/position.h"

// Evaluation cost over three positions, because it scales with pieces on the
// board and the slope is the interesting question, not one number.

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

void run_evaluate(benchmark::State& state, const char* fen) {
	const Position pos = parse_fen(fen);
	for (auto _ : state) {
		int16_t score = eval::evaluate(pos);
		benchmark::DoNotOptimize(score);
	}
}

}  // namespace

static void BM_Evaluate_StartPosition(benchmark::State& state) {
	run_evaluate(state, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}
BENCHMARK(BM_Evaluate_StartPosition);

static void BM_Evaluate_Middlegame(benchmark::State& state) {
	run_evaluate(state, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
}
BENCHMARK(BM_Evaluate_Middlegame);

static void BM_Evaluate_Endgame(benchmark::State& state) {
	// Five pieces against thirty-two: the low end of the same slope.
	run_evaluate(state, "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1");
}
BENCHMARK(BM_Evaluate_Endgame);
