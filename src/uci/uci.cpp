#include "uci/uci.h"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "movegen/attacks.h"
#include "movegen/movegen.h"
#include "position/position.h"
#include "search/search.h"

namespace {

constexpr const char* kStartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

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
void handle_position(std::istringstream& iss, Position& pos) {
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

// Milestone 3 (REFERENCE.md 3.8): plain iterative-deepening negamax
// alpha-beta replaces Milestone 2's uniformly random move choice.
void handle_go(Position& pos, std::istringstream& iss, std::ostream& out) {
	search::SearchLimits limits = parse_go_limits(iss);
	search::SearchResult result = search::think(pos, limits, &out);

	if (result.best_move.from == NULL_SQUARE) {
		out << "bestmove 0000\n";
		return;
	}
	out << "bestmove " << move_to_uci(result.best_move) << "\n";
}

}  // namespace

void run_uci_loop(std::istream& in, std::ostream& out) {
	init_attack_tables();

	Position pos = parse_fen(kStartFen);
	std::string line;

	while (std::getline(in, line)) {
		std::istringstream iss(line);
		std::string cmd;
		iss >> cmd;

		if (cmd == "uci") {
			out << "id name Dahlia\n";
			out << "id author Silas Teague\n";
			out << "uciok\n";
		} else if (cmd == "isready") {
			out << "readyok\n";
		} else if (cmd == "ucinewgame") {
			pos = parse_fen(kStartFen);
		} else if (cmd == "position") {
			handle_position(iss, pos);
		} else if (cmd == "go") {
			handle_go(pos, iss, out);
		} else if (cmd == "stop") {
			// Search runs synchronously inside handle_go, so by the time a
			// "stop" line is read here any prior "go" has already finished --
			// there's no in-flight search to interrupt yet. True async stop
			// needs a background search thread (REFERENCE.md 3.8/3.9's SMP note).
		} else if (cmd == "quit") {
			break;
		}
		// GUIs can hang waiting on buffered output (REFERENCE.md 2.4/3.10).
		out.flush();
	}
}
