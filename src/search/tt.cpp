#include "search/tt.h"

#include <algorithm>

namespace search {

namespace {

size_t entry_count_for(size_t megabytes) {
	size_t requested = (megabytes * 1024 * 1024) / sizeof(TTEntry);
	size_t count = 1;
	while (count * 2 <= requested) count *= 2;
	return std::max<size_t>(count, 1);
}

}  // namespace

TranspositionTable::TranspositionTable(size_t megabytes) { resize(megabytes); }

void TranspositionTable::resize(size_t megabytes) {
	size_t count = entry_count_for(megabytes);
	entries_.assign(count, TTEntry{});
	mask_ = count - 1;
	generation_ = 0;
}

void TranspositionTable::clear() {
	std::fill(entries_.begin(), entries_.end(), TTEntry{});
	generation_ = 0;
}

void TranspositionTable::new_search() { generation_++; }

void TranspositionTable::store(uint64_t key, Move best_move, int16_t score, int depth, Bound bound) {
	TTEntry& slot = entries_[key & mask_];
	if (slot.key != 0 && slot.generation == generation_ && slot.depth > depth) return;

	slot.key = key;
	slot.best_move = best_move;
	slot.score = score;
	slot.depth = static_cast<int8_t>(depth);
	slot.bound = bound;
	slot.generation = generation_;
}

bool TranspositionTable::probe(uint64_t key, TTEntry& out) const {
	const TTEntry& slot = entries_[key & mask_];
	if (slot.key != key) return false;
	out = slot;
	return true;
}

int TranspositionTable::hashfull_permille() const {
	size_t sample = std::min<size_t>(entries_.size(), 1000);
	size_t filled = 0;
	for (size_t i = 0; i < sample; i++) {
		if (entries_[i].key != 0 && entries_[i].generation == generation_) filled++;
	}
	return static_cast<int>(sample == 0 ? 0 : filled * 1000 / sample);
}

}  // namespace search
