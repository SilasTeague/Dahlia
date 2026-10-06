#include <benchmark/benchmark.h>

#include "core/types.h"
#include "movegen/attacks.h"
#include "movegen/movegen.h"

// Sliding-attack lookup latency and pseudo-legal generation speed.
//
// The three lookup benchmarks keep the names and bodies they had when they were
// measuring the ray walker, so the history in bench/results/ compares
// like-for-like across the magic-bitboard swap. The `_Ray` variants call the
// retained reference walker with the identical body, which puts both sides of
// that swap in one binary rather than in two builds
// (docs/results.md#magic-bitboards-milestone-7).

namespace {

struct AttackTableInit {
	AttackTableInit() { init_attack_tables(); }
} g_attack_table_init;

// A fairly dense middlegame-ish occupancy so the ray walker isn't measuring
// the trivial "always slides to the board edge" case.
constexpr Bitboard kMidgameOccupancy = 0xFFFF00000000FFFFULL | 0x0000240000240000ULL;

Position make_start_position() {
	return Position{
		.pieces = {
			{0x000000000000FF00, 0x0000000000000042, 0x0000000000000024, 0x0000000000000081, 0x0000000000000008, 0x0000000000000010},
			{0x00FF000000000000, 0x4200000000000000, 0x2400000000000000, 0x8100000000000000, 0x0800000000000000, 0x1000000000000000},
		},
		.aggregates = {0x000000000000FFFF, 0xFFFF000000000000, 0xFFFF00000000FFFF},
		.side_to_move = WHITE,
		.castling_rights = ANY_CASTLING,
		.en_passant_square = NULL_SQUARE,
		.halfmove_clock = 0,
		.fullmove_count = 1,
	};
}

}  // namespace

static void BM_RookAttacksLookup(benchmark::State& state) {
	for (auto _ : state) {
		Bitboard bb = rook_attacks(Square::d4, kMidgameOccupancy);
		benchmark::DoNotOptimize(bb);
	}
}
BENCHMARK(BM_RookAttacksLookup);

static void BM_BishopAttacksLookup(benchmark::State& state) {
	for (auto _ : state) {
		Bitboard bb = bishop_attacks(Square::d4, kMidgameOccupancy);
		benchmark::DoNotOptimize(bb);
	}
}
BENCHMARK(BM_BishopAttacksLookup);

static void BM_QueenAttacksLookup(benchmark::State& state) {
	for (auto _ : state) {
		Bitboard bb = queen_attacks(Square::d4, kMidgameOccupancy);
		benchmark::DoNotOptimize(bb);
	}
}
BENCHMARK(BM_QueenAttacksLookup);

static void BM_RookAttacksLookup_Ray(benchmark::State& state) {
	for (auto _ : state) {
		Bitboard bb = ray_rook_attacks(Square::d4, kMidgameOccupancy);
		benchmark::DoNotOptimize(bb);
	}
}
BENCHMARK(BM_RookAttacksLookup_Ray);

static void BM_BishopAttacksLookup_Ray(benchmark::State& state) {
	for (auto _ : state) {
		Bitboard bb = ray_bishop_attacks(Square::d4, kMidgameOccupancy);
		benchmark::DoNotOptimize(bb);
	}
}
BENCHMARK(BM_BishopAttacksLookup_Ray);

static void BM_QueenAttacksLookup_Ray(benchmark::State& state) {
	for (auto _ : state) {
		Bitboard bb = ray_rook_attacks(Square::d4, kMidgameOccupancy) |
		              ray_bishop_attacks(Square::d4, kMidgameOccupancy);
		benchmark::DoNotOptimize(bb);
	}
}
BENCHMARK(BM_QueenAttacksLookup_Ray);

// Magic bitboards move work from every lookup to a one-time table fill. This is
// that fill: 107,648 ray walks, paid once per process before the first search.
static void BM_InitAttackTables(benchmark::State& state) {
	for (auto _ : state) {
		init_attack_tables();
	}
}
BENCHMARK(BM_InitAttackTables);

static void BM_GeneratePseudoLegalMoves_StartPosition(benchmark::State& state) {
	Position pos = make_start_position();
	for (auto _ : state) {
		MoveList moves;
		generate_pseudo_legal_moves(moves, pos);
		benchmark::DoNotOptimize(moves);
	}
}
BENCHMARK(BM_GeneratePseudoLegalMoves_StartPosition);
