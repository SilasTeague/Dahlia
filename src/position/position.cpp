#include "position/position.h"

#include <cctype>
#include <cstdlib>

#include "movegen/attacks.h"
#include "position/zobrist.h"

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

void clear_castling_right_for_square(Position& pos, Square sq) {
	switch (sq) {
		case e1: pos.castling_rights = static_cast<CastlingRights>(pos.castling_rights & ~WHITE_CASTLING); break;
		case e8: pos.castling_rights = static_cast<CastlingRights>(pos.castling_rights & ~BLACK_CASTLING); break;
		case a1: pos.castling_rights = static_cast<CastlingRights>(pos.castling_rights & ~WHITE_000); break;
		case h1: pos.castling_rights = static_cast<CastlingRights>(pos.castling_rights & ~WHITE_00); break;
		case a8: pos.castling_rights = static_cast<CastlingRights>(pos.castling_rights & ~BLACK_000); break;
		case h8: pos.castling_rights = static_cast<CastlingRights>(pos.castling_rights & ~BLACK_00); break;
		default: break;
	}
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

	pos.zobrist_key = compute_zobrist_key(pos);

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

void make_move(Position& pos, Move m, StateInfo& undo) {
	Color us = pos.side_to_move;
	Color them = static_cast<Color>(us ^ 1);
	Bitboard from_bb = 1ULL << m.from;
	Bitboard to_bb = 1ULL << m.to;

	Piece moving = pos.board[m.from];
	bool is_en_passant = (moving == PAWN && m.to == pos.en_passant_square);
	Piece captured = is_en_passant ? NULL_PIECE : pos.board[m.to];

	undo.captured_piece = is_en_passant ? PAWN : captured;
	undo.castling_rights = pos.castling_rights;
	undo.en_passant_square = pos.en_passant_square;
	undo.halfmove_clock = pos.halfmove_clock;
	undo.zobrist_key = pos.zobrist_key;

	uint64_t& key = pos.zobrist_key;

	if (is_en_passant) {
		Square captured_sq = static_cast<Square>(us == WHITE ? m.to - 8 : m.to + 8);
		pos.pieces[them][PAWN] &= ~(1ULL << captured_sq);
		pos.board[captured_sq] = NULL_PIECE;
		key ^= zobrist::tables.piece_square[them][PAWN][captured_sq];
	} else if (captured != NULL_PIECE) {
		pos.pieces[them][captured] &= ~to_bb;
		key ^= zobrist::tables.piece_square[them][captured][m.to];
	}

	pos.pieces[us][moving] &= ~from_bb;
	pos.board[m.from] = NULL_PIECE;
	key ^= zobrist::tables.piece_square[us][moving][m.from];

	Piece final_piece = moving;
	if (m.promotion != NO_PROMOTION) {
		switch (m.promotion) {
			case PROMOTION_QUEEN: final_piece = QUEEN; break;
			case PROMOTION_ROOK: final_piece = ROOK; break;
			case PROMOTION_BISHOP: final_piece = BISHOP; break;
			case PROMOTION_KNIGHT: final_piece = KNIGHT; break;
			default: break;
		}
	}
	pos.pieces[us][final_piece] |= to_bb;
	pos.board[m.to] = final_piece;
	key ^= zobrist::tables.piece_square[us][final_piece][m.to];

	if (moving == KING) {
		auto move_rook = [&](Square rook_from, Square rook_to) {
			pos.pieces[us][ROOK] &= ~(1ULL << rook_from);
			pos.pieces[us][ROOK] |= (1ULL << rook_to);
			pos.board[rook_from] = NULL_PIECE;
			pos.board[rook_to] = ROOK;
			key ^= zobrist::tables.piece_square[us][ROOK][rook_from];
			key ^= zobrist::tables.piece_square[us][ROOK][rook_to];
		};
		if (m.from == e1 && m.to == g1) move_rook(h1, f1);
		else if (m.from == e1 && m.to == c1) move_rook(a1, d1);
		else if (m.from == e8 && m.to == g8) move_rook(h8, f8);
		else if (m.from == e8 && m.to == c8) move_rook(a8, d8);
	}

	key ^= zobrist::tables.castling[pos.castling_rights];
	clear_castling_right_for_square(pos, m.from);
	clear_castling_right_for_square(pos, m.to);
	key ^= zobrist::tables.castling[pos.castling_rights];

	if (pos.en_passant_square != NULL_SQUARE) {
		key ^= zobrist::tables.en_passant_file[pos.en_passant_square % 8];
	}
	pos.en_passant_square = NULL_SQUARE;
	if (moving == PAWN) {
		int diff = static_cast<int>(m.to) - static_cast<int>(m.from);
		if (diff == 16) pos.en_passant_square = static_cast<Square>(m.from + 8);
		if (diff == -16) pos.en_passant_square = static_cast<Square>(m.from - 8);
	}
	if (pos.en_passant_square != NULL_SQUARE) {
		key ^= zobrist::tables.en_passant_file[pos.en_passant_square % 8];
	}

	if (moving == PAWN || captured != NULL_PIECE || is_en_passant) {
		pos.halfmove_clock = 0;
	} else {
		pos.halfmove_clock++;
	}
	if (us == BLACK) pos.fullmove_count++;

	pos.side_to_move = them;
	key ^= zobrist::tables.side_to_move;

	recompute_aggregates(pos);
}

void unmake_move(Position& pos, Move m, const StateInfo& undo) {
	Color us = static_cast<Color>(pos.side_to_move ^ 1);
	Color them = pos.side_to_move;
	pos.side_to_move = us;

	Piece final_piece = pos.board[m.to];
	Piece moving = (m.promotion != NO_PROMOTION) ? PAWN : final_piece;

	pos.pieces[us][final_piece] &= ~(1ULL << m.to);
	pos.board[m.to] = NULL_PIECE;
	pos.pieces[us][moving] |= (1ULL << m.from);
	pos.board[m.from] = moving;

	if (moving == KING) {
		auto move_rook_back = [&](Square rook_from, Square rook_to) {
			// rook_from/rook_to named from make_move's perspective (pre-move -> post-move).
			pos.pieces[us][ROOK] &= ~(1ULL << rook_to);
			pos.pieces[us][ROOK] |= (1ULL << rook_from);
			pos.board[rook_to] = NULL_PIECE;
			pos.board[rook_from] = ROOK;
		};
		if (m.from == e1 && m.to == g1) move_rook_back(h1, f1);
		else if (m.from == e1 && m.to == c1) move_rook_back(a1, d1);
		else if (m.from == e8 && m.to == g8) move_rook_back(h8, f8);
		else if (m.from == e8 && m.to == c8) move_rook_back(a8, d8);
	}

	bool is_en_passant = (moving == PAWN && m.to == undo.en_passant_square);
	if (is_en_passant) {
		Square captured_sq = static_cast<Square>(us == WHITE ? m.to - 8 : m.to + 8);
		pos.pieces[them][PAWN] |= (1ULL << captured_sq);
		pos.board[captured_sq] = PAWN;
	} else if (undo.captured_piece != NULL_PIECE) {
		pos.pieces[them][undo.captured_piece] |= (1ULL << m.to);
		pos.board[m.to] = undo.captured_piece;
	}

	pos.castling_rights = undo.castling_rights;
	pos.en_passant_square = undo.en_passant_square;
	pos.halfmove_clock = undo.halfmove_clock;
	pos.zobrist_key = undo.zobrist_key;
	if (us == BLACK) pos.fullmove_count--;

	recompute_aggregates(pos);
}

bool is_in_check(const Position& pos, Color color) {
	Bitboard king_bb = pos.pieces[color][KING];
	if (!king_bb) return false;
	Square king_sq = static_cast<Square>(__builtin_ctzll(king_bb));
	return is_attacked(pos, king_sq, static_cast<Color>(color ^ 1));
}
