#include "uci/uci.h"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "movegen/attacks.h"
#include "movegen/movegen.h"
#include "position/position.h"

namespace {

constexpr const char* kStartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

MoveList generate_legal_moves(Position& pos) {
	MoveList pseudo;
	generate_pseudo_legal_moves(pseudo, pos);

	MoveList legal;
	Color us = pos.side_to_move;
	for (int i = 0; i < pseudo.count; i++) {
		StateInfo undo;
		make_move(pos, pseudo.moves[i], undo);
		if (!is_in_check(pos, us)) legal.push(pseudo.moves[i]);
		unmake_move(pos, pseudo.moves[i], undo);
	}
	return legal;
}

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

// Milestone 2 placeholder: no eval/search yet, so `go` (ignoring all of its
// time/depth/node parameters for now) just picks a uniformly random legal
// move. Milestone 3 replaces this with alpha-beta search.
void handle_go(Position& pos, std::ostream& out, std::mt19937& rng) {
	MoveList legal = generate_legal_moves(pos);
	if (legal.count == 0) {
		out << "bestmove 0000\n";
		return;
	}
	std::uniform_int_distribution<int> dist(0, legal.count - 1);
	out << "bestmove " << move_to_uci(legal.moves[dist(rng)]) << "\n";
}

}  // namespace

void run_uci_loop(std::istream& in, std::ostream& out, unsigned int seed) {
	init_attack_tables();

	Position pos = parse_fen(kStartFen);
	std::mt19937 rng(seed);
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
			handle_go(pos, out, rng);
		} else if (cmd == "stop") {
			// No asynchronous search yet (Milestone 3+); nothing to stop.
		} else if (cmd == "quit") {
			break;
		}
		// GUIs can hang waiting on buffered output (REFERENCE.md 2.4/3.10).
		out.flush();
	}
}
