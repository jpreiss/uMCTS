#include "mcts.hpp"

// Tic Tac Toe example game for Monte Carlo Tree Search.

using uint = mcts::uint;

// fastest way to count bits. we store board state in bits.
static inline uint bitcount(uint v)
{
	v = v - ((v >> 1) & 0x55555555);
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
	return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
}

class TicTacToe
{
public:
	static uint constexpr n_moves() { return 9; }

	uint player_turn() const { return iplayer; }

	mcts::WinState winner() const
	{
		bool const w0 = is_win(xos[0]), w1 = is_win(xos[1]);
		if (!(w0 || w1) && ((xos[0] | xos[1]) == 0x1FF)) return mcts::TIE;
		if (w0) return mcts::WIN;
		if (w1) return mcts::LOSS;
		return mcts::NONE;
	}

	uint n_valid_moves() const
	{
		return 9 - bitcount(xos[0] | xos[1]);
	}

	bool is_valid(int move) const
	{
		uint const pos = 1u << move;
		return (pos & (xos[0] | xos[1])) == 0;
	}

	TicTacToe move(uint mv) const
	{
		TicTacToe t = *this;
		t.xos[iplayer] |= (1 << mv);
		t.iplayer = (iplayer + 1) & 1;
		return t;
	}

	friend std::ostream &operator<<(std::ostream &s, TicTacToe const &ttt);

private:
	static bool is_win(int state)
	{
		static int winning_states[] = {
			0b000'000'111, 0b000'111'000, 0b111'000'000,
			0b001'001'001, 0b010'010'010, 0b100'100'100,
			0b100'010'001, 0b001'010'100,
		};
		for (int s : winning_states) {
			if ((s & state) == s) return true;
		}
		return false;
	}

	uint xos[2] = {0, 0};
	int iplayer = 0;
};

std::ostream &operator<<(std::ostream &s, TicTacToe const &ttt)
{
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			bool const x = ttt.xos[0] & (1 << (3*i + j));
			bool const o = ttt.xos[1] & (1 << (3*i + j));
			assert(!(x && o));
			if (x) s << "X";
			else if (o) s << "O";
			else s << "-";
		}
		s << "\n";
	}
	return s;
}

