#include "profiling/profile_mode.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "aggregator.h"
#include "movegen/attacks.h"
#include "position/history.h"
#include "position/position.h"
#include "sampler.h"
#include "search/search.h"

namespace {

constexpr const char* kStartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr int kDefaultProfileDepth = 8;
constexpr int kDefaultHashMb = 16;
constexpr int kSampleHz = 500;
constexpr const char* kOutputPath = "dahlia_profile.txt";

struct ProfileArgs {
	int depth = kDefaultProfileDepth;
	std::string fen = kStartFen;
};

// "--profile [depth D] [fen <fen fields...>]"
ProfileArgs parse_profile_args(int argc, char** argv) {
	ProfileArgs args;
	for (int i = 1; i < argc; ++i) {
		std::string_view tok = argv[i];
		if (tok == "depth" && i + 1 < argc) {
			args.depth = std::atoi(argv[++i]);
		} else if (tok == "fen") {
			std::string fen;
			int j = i + 1;
			for (; j < argc && std::string_view(argv[j]) != "depth"; ++j) {
				if (!fen.empty()) fen += ' ';
				fen += argv[j];
			}
			if (!fen.empty()) args.fen = fen;
			i = j - 1;
		}
	}
	return args;
}

} // namespace

int run_profile_mode(int argc, char** argv) {
	ProfileArgs args = parse_profile_args(argc, argv);

	init_attack_tables();

	Position pos = parse_fen(args.fen);
	PositionHistory history;
	search::TranspositionTable tt{kDefaultHashMb};
	search::HistoryTable move_history;
	std::atomic<bool> stop_requested{false};

	search::SearchLimits limits;
	limits.depth = args.depth;

	std::cout << "Profiling search to depth " << args.depth << " on: " << args.fen << "\n";

	search::SearchResult result;
	{
		Sampler sampler(kOutputPath, kSampleHz);
		result = search::think(pos, limits, tt, move_history, stop_requested, nullptr, history);
	}

	std::cout << "score=" << result.score << " depth_reached=" << result.depth_reached
	          << " nodes=" << result.nodes << "\n\n";

	Aggregator aggregator(kOutputPath);
	aggregator.processFile();
	aggregator.output();

	return 0;
}
