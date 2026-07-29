#include "position/position.h"

#include <cctype>
#include <cstdlib>

namespace {

Piece piece_from_char(char c) {
	switch (std::tolower(static_cast<unsigned char>(c))) {
		case 'p': return PAWN;
		case 'n': return KNIGHT;
		case 'b': return BISHOP;
		case 'r': return ROOK;
		case 'q': return QUEEN;
		case 'k': return KING;
		default: return NULL_PIECE;
	}
}

char piece_to_char(Piece piece, Color color) {
	static constexpr char kWhiteChars[] = {'P', 'N', 'B', 'R', 'Q', 'K'};
	static constexpr char kBlackChars[] = {'p', 'n', 'b', 'r', 'q', 'k'};
	return (color == WHITE) ? kWhiteChars[piece] : kBlackChars[piece];
}

void recompute_aggregates(Position& pos) {
	pos.aggregates[WHITE] = 0;
	pos.aggregates[BLACK] = 0;
	for (int p = PAWN; p <= KING; p++) {
		pos.aggregates[WHITE] |= pos.pieces[WHITE][p];
		pos.aggregates[BLACK] |= pos.pieces[BLACK][p];
	}
	pos.aggregates[ALL] = pos.aggregates[WHITE] | pos.aggregates[BLACK];
}

}  // namespace

Position parse_fen(std::string_view fen) {
	Position pos{};
	for (auto& side_pieces : pos.pieces) {
		for (auto& bb : side_pieces) bb = 0;
	}
	for (auto& p : pos.board) p = NULL_PIECE;
	pos.castling_rights = NO_CASTLING;
	pos.en_passant_square = NULL_SQUARE;
	pos.halfmove_clock = 0;
	pos.fullmove_count = 1;

	size_t i = 0;

	// 1. Piece placement, rank 8 down to rank 1, file a to h.
	int rank = 7;
	int file = 0;
	for (; i < fen.size() && fen[i] != ' '; i++) {
		char c = fen[i];
		if (c == '/') {
			rank--;
			file = 0;
		} else if (std::isdigit(static_cast<unsigned char>(c))) {
			file += c - '0';
		} else {
			Piece piece = piece_from_char(c);
			Color color = std::isupper(static_cast<unsigned char>(c)) ? WHITE : BLACK;
			Square sq = static_cast<Square>(rank * 8 + file);
			pos.pieces[color][piece] |= (1ULL << sq);
			pos.board[sq] = piece;
			file++;
		}
	}
	recompute_aggregates(pos);

	// 2. Side to move.
	while (i < fen.size() && fen[i] == ' ') i++;
	pos.side_to_move = (i < fen.size() && fen[i] == 'b') ? BLACK : WHITE;
	while (i < fen.size() && fen[i] != ' ') i++;

	// 3. Castling rights.
	while (i < fen.size() && fen[i] == ' ') i++;
	for (; i < fen.size() && fen[i] != ' '; i++) {
		switch (fen[i]) {
			case 'K': pos.castling_rights = static_cast<CastlingRights>(pos.castling_rights | WHITE_00); break;
			case 'Q': pos.castling_rights = static_cast<CastlingRights>(pos.castling_rights | WHITE_000); break;
			case 'k': pos.castling_rights = static_cast<CastlingRights>(pos.castling_rights | BLACK_00); break;
			case 'q': pos.castling_rights = static_cast<CastlingRights>(pos.castling_rights | BLACK_000); break;
			default: break;  // '-'
		}
	}

	// 4. En passant square.
	while (i < fen.size() && fen[i] == ' ') i++;
	if (i < fen.size() && fen[i] != '-') {
		int ep_file = fen[i] - 'a';
		int ep_rank = fen[i + 1] - '1';
		pos.en_passant_square = static_cast<Square>(ep_rank * 8 + ep_file);
		i += 2;
	} else if (i < fen.size()) {
		i++;
	}

	// 5. Halfmove clock, 6. fullmove number (both optional in some FEN sources).
	while (i < fen.size() && fen[i] == ' ') i++;
	if (i < fen.size()) {
		pos.halfmove_clock = static_cast<uint8_t>(std::atoi(std::string(fen.substr(i)).c_str()));
	}
	while (i < fen.size() && fen[i] != ' ') i++;
	while (i < fen.size() && fen[i] == ' ') i++;
	if (i < fen.size()) {
		pos.fullmove_count = static_cast<uint16_t>(std::atoi(std::string(fen.substr(i)).c_str()));
	}

	return pos;
}

std::string to_fen(const Position& pos) {
	std::string fen;

	for (int rank = 7; rank >= 0; rank--) {
		int empty_run = 0;
		for (int file = 0; file < 8; file++) {
			Square sq = static_cast<Square>(rank * 8 + file);
			Piece piece = pos.board[sq];
			if (piece == NULL_PIECE) {
				empty_run++;
				continue;
			}
			if (empty_run > 0) {
				fen += static_cast<char>('0' + empty_run);
				empty_run = 0;
			}
			Color color = (pos.pieces[WHITE][piece] & (1ULL << sq)) ? WHITE : BLACK;
			fen += piece_to_char(piece, color);
		}
		if (empty_run > 0) fen += static_cast<char>('0' + empty_run);
		if (rank > 0) fen += '/';
	}

	fen += ' ';
	fen += (pos.side_to_move == WHITE) ? 'w' : 'b';

	fen += ' ';
	if (pos.castling_rights == NO_CASTLING) {
		fen += '-';
	} else {
		if (pos.castling_rights & WHITE_00) fen += 'K';
		if (pos.castling_rights & WHITE_000) fen += 'Q';
		if (pos.castling_rights & BLACK_00) fen += 'k';
		if (pos.castling_rights & BLACK_000) fen += 'q';
	}

	fen += ' ';
	if (pos.en_passant_square == NULL_SQUARE) {
		fen += '-';
	} else {
		fen += static_cast<char>('a' + (pos.en_passant_square % 8));
		fen += static_cast<char>('1' + (pos.en_passant_square / 8));
	}

	fen += ' ' + std::to_string(pos.halfmove_clock);
	fen += ' ' + std::to_string(pos.fullmove_count);

	return fen;
}
