#include "uci/uci.h"

#include <atomic>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "movegen/attacks.h"
#include "movegen/movegen.h"
#include "position/history.h"
#include "position/position.h"
#include "search/search.h"

namespace {

constexpr const char* kStartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr int kDefaultHashMb = 16;
constexpr long long kMaxMoveOverheadMs = 1000;

std::string square_to_uci(Square sq) {
	std::string s;
	s += static_cast<char>('a' + (sq % 8));
	s += static_cast<char>('1' + (sq / 8));
	return s;
}

std::string move_to_uci(Move m) {
	std::string s = square_to_uci(m.from) + square_to_uci(m.to);
	switch (m.promotion) {
		case PROMOTION_QUEEN: s += 'q'; break;
		case PROMOTION_ROOK: s += 'r'; break;
		case PROMOTION_BISHOP: s += 'b'; break;
		case PROMOTION_KNIGHT: s += 'n'; break;
		default: break;
	}
	return s;
}

Square square_from_uci(std::string_view s) {
	int file = s[0] - 'a';
	int rank = s[1] - '1';
	return static_cast<Square>(rank * 8 + file);
}

Move move_from_uci(std::string_view s) {
	Square from = square_from_uci(s.substr(0, 2));
	Square to = square_from_uci(s.substr(2, 2));
	Promotion promotion = NO_PROMOTION;
	if (s.size() > 4) {
		switch (s[4]) {
			case 'q': promotion = PROMOTION_QUEEN; break;
			case 'r': promotion = PROMOTION_ROOK; break;
			case 'b': promotion = PROMOTION_BISHOP; break;
			case 'n': promotion = PROMOTION_KNIGHT; break;
			default: break;
		}
	}
	return Move{from, to, promotion};
}

// "position [startpos | fen <fen fields>] [moves <uci moves...>]"
//
// The move list is also where the engine learns which positions have already
// occurred: GUIs re-send the whole game on every `position`, so `history` is
// rebuilt from scratch rather than appended to.
void handle_position(std::istringstream& iss, Position& pos, PositionHistory& history) {
	history.clear();

	std::string token;
	iss >> token;

	if (token == "startpos") {
		pos = parse_fen(kStartFen);
		iss >> token;  // consumes "moves" if present, else leaves token stale (harmless below)
	} else if (token == "fen") {
		std::vector<std::string> fen_fields;
		while (iss >> token && token != "moves") {
			fen_fields.push_back(token);
		}
		std::string fen;
		for (size_t i = 0; i < fen_fields.size(); i++) {
			if (i) fen += ' ';
			fen += fen_fields[i];
		}
		pos = parse_fen(fen);
	}

	if (token == "moves") {
		std::string move_str;
		while (iss >> move_str) {
			history.push(pos.zobrist_key);  // the position this move is played *from*
			StateInfo undo;  // discarded: UCI applies moves forward only, no unmake needed
			make_move(pos, move_from_uci(move_str), undo);
		}
	}
}

// "go [depth D] [movetime MS] [wtime MS] [btime MS] [winc MS] [binc MS]
//  [movestogo N] [infinite]" -- unrecognized tokens (e.g. "nodes", "ponder",
// "searchmoves ...") are consumed harmlessly by the `iss >> token` loop below
// since they're not matched by any branch.
search::SearchLimits parse_go_limits(std::istringstream& iss) {
	search::SearchLimits limits;
	std::string token;
	while (iss >> token) {
		if (token == "depth") iss >> limits.depth;
		else if (token == "movetime") iss >> limits.movetime_ms;
		else if (token == "wtime") iss >> limits.wtime_ms;
		else if (token == "btime") iss >> limits.btime_ms;
		else if (token == "winc") iss >> limits.winc_ms;
		else if (token == "binc") iss >> limits.binc_ms;
		else if (token == "movestogo") iss >> limits.movestogo;
		else if (token == "infinite") limits.infinite = true;
	}
	return limits;
}

// "setoption name <Hash|Move Overhead> value <N>". Option names can be
// multi-word ("Move Overhead"), so the name tokens are rejoined with the
// single spaces the protocol splits them on. Out-of-range values are ignored,
// leaving the current setting in place.
void handle_setoption(std::istringstream& iss, search::TranspositionTable& tt, long long& move_overhead_ms) {
	std::string token;
	iss >> token;  // "name"
	std::string name;
	while (iss >> token && token != "value") {
		if (!name.empty()) name += ' ';
		name += token;
	}
	if (name == "Hash") {
		int megabytes = 0;
		if (iss >> megabytes && megabytes > 0) tt.resize(static_cast<size_t>(megabytes));
	} else if (name == "Move Overhead") {
		long long overhead = 0;
		if (iss >> overhead && overhead >= 0 && overhead <= kMaxMoveOverheadMs) move_overhead_ms = overhead;
	}
}

// Owns the position/TT/search-thread state for one UCI session. `go` runs
// search::think on a dedicated thread so the stdin read-loop stays
// responsive (`stop`/`isready`/`quit`) for the duration of a search
// (REFERENCE.md 3.8/3.9/3.10). All output goes through write_line(), which
// serializes the search thread's `info` lines against the reader thread's
// `readyok`/`bestmove`/etc. under one mutex -- otherwise two threads writing
// to `out` concurrently could interleave mid-line.
class Engine {
 public:
	explicit Engine(std::ostream& out) : out_(out), pos_(parse_fen(kStartFen)) {}

	// Guarantees the search thread (if any) is stopped and joined before the
	// Engine's members are torn down, however run_uci_loop exits (`quit`, or
	// the input stream simply running out without one).
	~Engine() { stop_and_join(); }

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	void handle_uci() {
		std::lock_guard<std::mutex> lock(io_mutex_);
		out_ << "id name Dahlia\n";
		out_ << "id author Silas Teague\n";
		out_ << "option name Hash type spin default " << kDefaultHashMb << " min 1 max 1024\n";
		out_ << "option name Move Overhead type spin default " << search::kDefaultMoveOverheadMs
		     << " min 0 max " << kMaxMoveOverheadMs << "\n";
		out_ << "uciok\n";
		out_.flush();
	}

	// Must answer even while a search is in flight -- GUIs use isready to
	// confirm the engine is alive/responsive mid-search, not just at startup.
	void handle_isready() { write_line("readyok"); }

	// UCI compliant GUIs only send ucinewgame/setoption/position between a
	// `bestmove` and the next `go`, never during a search, so no guard
	// against a concurrent search is needed here (REFERENCE.md 3.10).
	void handle_ucinewgame() {
		pos_ = parse_fen(kStartFen);
		history_.clear();
		tt_.clear();
		// Move-ordering state carried into an unrelated game is stale bias, not
		// a head start: the history table's whole claim is "these moves keep
		// working *here*" (REFERENCE.md 3.8).
		move_history_.clear();
	}

	void handle_setoption(std::istringstream& iss) { ::handle_setoption(iss, tt_, move_overhead_ms_); }

	void handle_position(std::istringstream& iss) { ::handle_position(iss, pos_, history_); }

	// A `go` that arrives while a search is already running is rejected
	// outright rather than queued -- see docs/adr/0003-async-search-stop.md.
	void handle_go(std::istringstream& iss) {
		search::SearchLimits limits = parse_go_limits(iss);
		limits.move_overhead_ms = move_overhead_ms_;  // from setoption, not from `go`
		if (search_running_.load(std::memory_order_relaxed)) return;

		if (search_thread_.joinable()) search_thread_.join();  // reap the previous (finished) search

		stop_requested_.store(false, std::memory_order_relaxed);
		search_running_.store(true, std::memory_order_relaxed);

		search_thread_ = std::thread(
			[this](Position search_pos, PositionHistory search_history, search::SearchLimits search_limits) {
				auto on_info = [this](const std::string& line) { write_line(line); };
				search::SearchResult result =
					search::think(search_pos, search_limits, tt_, move_history_, stop_requested_, on_info,
					              search_history);

				write_line(result.best_move.from == NULL_SQUARE ? "bestmove 0000"
				                                                 : "bestmove " + move_to_uci(result.best_move));
				search_running_.store(false, std::memory_order_relaxed);
			},
			pos_, history_, limits);
	}

	// Signals the search thread; does not block waiting for it to finish, so
	// isready/further commands stay responsive while it winds down.
	void handle_stop() { stop_requested_.store(true, std::memory_order_relaxed); }

	void quit() { stop_and_join(); }

 private:
	void write_line(const std::string& line) {
		std::lock_guard<std::mutex> lock(io_mutex_);
		out_ << line << "\n";
		out_.flush();
	}

	void stop_and_join() {
		stop_requested_.store(true, std::memory_order_relaxed);
		if (search_thread_.joinable()) search_thread_.join();
	}

	std::ostream& out_;
	std::mutex io_mutex_;

	Position pos_;
	PositionHistory history_;
	search::TranspositionTable tt_{kDefaultHashMb};
	// Like tt_, owned here rather than by the search so it survives across the
	// moves of a game; touched only by the search thread while a search is
	// running, and only by handle_ucinewgame() when one isn't.
	search::HistoryTable move_history_;
	// Read on the reader thread when launching a search, written only by
	// setoption -- which UCI guarantees never arrives mid-search -- so it
	// needs no synchronization; the value is copied into the limits the
	// search thread gets.
	long long move_overhead_ms_ = search::kDefaultMoveOverheadMs;

	std::atomic<bool> stop_requested_{false};
	std::atomic<bool> search_running_{false};
	std::thread search_thread_;
};

}  // namespace

void run_uci_loop(std::istream& in, std::ostream& out) {
	init_attack_tables();

	Engine engine(out);
	std::string line;

	while (std::getline(in, line)) {
		std::istringstream iss(line);
		std::string cmd;
		iss >> cmd;

		if (cmd == "uci") {
			engine.handle_uci();
		} else if (cmd == "isready") {
			engine.handle_isready();
		} else if (cmd == "ucinewgame") {
			engine.handle_ucinewgame();
		} else if (cmd == "setoption") {
			engine.handle_setoption(iss);
		} else if (cmd == "position") {
			engine.handle_position(iss);
		} else if (cmd == "go") {
			engine.handle_go(iss);
		} else if (cmd == "stop") {
			engine.handle_stop();
		} else if (cmd == "quit") {
			engine.quit();
			break;
		}
	}
}
