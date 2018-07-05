#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>

#include "arena.hpp"
#include "fastlog.hpp"

/*
Basic implementation of Monte Carlo Tree Search game-playing algorithm.

Code is optimized in the following ways:
- using memory arena for tree nodes instead of malloc/new.
- fast approximate logarithm to compute UCB values.
Both of these optimizations are verified with profiler to make a difference.

The next step would be to use multiple threads, but then
it is no longer straightforward to follow the UCT exploration rule precisely.
MCTS is not embarrassingly parallel and implementing UCT exactly
would require things like mutexes, barriers, etc.
"Root parallelization" is a simple and good-performing approach
where you do N independent MCTS's and merge them at the end.
(Guillaume Chaslot et al., "Parallel Monte-Carlo Tree Search",
Int'l Conference on Computers and Games, 2008.)
*/

namespace mcts
{

using uint = unsigned int;

using WinState = int;
static WinState const WIN = 1;
static WinState const TIE = 0;
static WinState const LOSS = -1;
static WinState const NONE = -2;

/*
NOTE: this is not real Concepts code!

// description of the game interface requred by MCTS
concept Game
{
	// the maximum number of possible moves at any game state.
	// must be constexpr.
	static uint constexpr n_moves();

	// whose turn it is. must be in {0, 1}. 0 goes first.
	uint player_turn() const;

	// see constants above class. return NONE if game is not over.
	WinState winner() const;

	// number of valid moves at current state. must be 0 < n <= n_moves.
	uint n_valid_moves() const;

	// true if move 0 <= k < n_moves is valid at current state.
	bool is_valid(int move) const;

	// play move k return a new state (note, this is a const method!)
	TicTacToe move(uint k) const;
};
*/

template <typename Game>
class Node
{
public:
	Node(Game &&state) : state(state) {}
	Game state;

	using MyArena = Arena<Node>;

	Node *clone(MyArena &arena) const
	{
		Node *node = arena.alloc(state);
		*node = *this;
		for (Node * &p : node->children) {
			if (p != nullptr) {
				p = p->clone(arena);
			}
		}
		return node;
	}

	bool is_leaf() const
	{
		return state.winner() != NONE;
	}

	bool is_move_explored(uint move) const
	{
		return children[move] != nullptr;
	}

	Node *child(uint move) const
	{
		return children[move];
	}

	// do a rollout according to the UCT exploration strategy.
	template <typename RandomGen>
	WinState ucb_rollout(RandomGen &rng, MyArena &arena)
	{
		WinState winner = state.winner();
		if (winner != NONE) return winner;

		// random_rollout updates counts
		if (n_unplayed_moves() > 0) return random_rollout(rng, arena);

		// UCB policy
		uint const player = state.player_turn();
		uint const move = ucb_move();
		assert(children[move] != nullptr);
		winner = children[move]->ucb_rollout(rng, arena);
		_update(move, winner);
		return winner;
	}

	// do a random rollout until a game end state is reached.
	// aka "simulation" in the UCT paper.
	template <typename RandomGen>
	WinState random_rollout(RandomGen &rng, MyArena &arena)
	{
		WinState winner = state.winner();
		if (winner != NONE) {
			tot_tries = 1.0f;
			return winner;
		}

		uint move = random_unplayed_move(rng);
		auto node = arena.alloc(state.move(move));
		winner = node->random_rollout(rng, arena);
		assert(children[move] == nullptr);
		children[move] = std::move(node);
		_update(move, winner);
		return winner;
	}

	// compute the number of moves that have not been explored yet.
	inline uint n_unplayed_moves() const
	{
		uint count = 0;
		for (uint i = 0; i < Game::n_moves(); ++i) {
			count += (tries[i] == 0 && state.is_valid(i));
		}
		return count;
	}

	// TODO factor common of random_move and random_unplayed_move
	template <typename RandomGen>
	uint random_move(RandomGen &rng) const
	{
		uint n = state.n_valid_moves();
		std::uniform_int_distribution<uint> dist(1, n);
		uint imove = dist(rng);
		uint count = 0;
		for (uint i = 0; i < Game::n_moves(); ++i) {
			count += state.is_valid(i);
			if (count == imove) {
				return i;
			}
		}
		assert(false);
		return 0xFFFFFFFF;
	}

	// find a random move that has not been played yet.
	template <typename RandomGen>
	uint random_unplayed_move(RandomGen &rng) const
	{
		uint n = n_unplayed_moves();
		assert(n > 0);
		std::uniform_int_distribution<uint> dist(1, n);
		uint imove = dist(rng);
		// TODO can we use a std::algorithm here?
		uint count = 0;
		for (uint i = 0; i < Game::n_moves(); ++i) {
			count += (tries[i] == 0 && state.is_valid(i));
			if (count == imove) {
				return i;
			}
		}
		assert(false);
		return 0xFFFFFFFF;
	}

	// move according to the UCB (upper confidence bound) exploration strategy
	uint ucb_move() const
	{
		assert(n_unplayed_moves() == 0);

		uint player = state.player_turn();
		float const flip = (player == 0) ? 1.0f : -1.0f;
		auto ucb = [this, flip](uint move) {
			float const mean = flip * wins[move] / tries[move];
			return mean + sqrtf(2.0f * fastlog(tot_tries+1e-4f) / tries[move]);
		};

		float ucb_max = -std::numeric_limits<float>::infinity();
		uint i_max = 0xFFFFFFFF;
		for (uint i = 0; i < Game::n_moves(); ++i) {
			if (!state.is_valid(i)) continue;
			// exit early if one of our children is a winning leaf state.
			if (player == 0 && children[i]->state.winner() == WIN) {
				return i;
			}
			if (player == 1 && children[i]->state.winner() == LOSS) {
				return i;
			}
			float ucb_i = ucb(i);
			if (ucb_i > ucb_max) {
				ucb_max = ucb_i;
				i_max = i;
			}
		}
		assert(i_max != 0xFFFFFFFF);
		return i_max;
	}

private:
	float tot_tries = 0.0f;
	std::array<Node *, Game::n_moves()> children = {};
	std::array<float, Game::n_moves()> tries = {};
	std::array<float, Game::n_moves()> wins = {};

	void _update(uint move, float delta)
	{
		assert(delta != NONE);
		++tries[move];
		++tot_tries;
		wins[move] += delta;
	}
};

// play an entire game between the MCTS agent and random agent.

template <typename Game, typename RandomGen>
std::pair<std::vector<Game>, std::vector<uint>>
play_vs_random(RandomGen &prng, size_t n_rollouts)
{
	Arena<Node<Game>> arena;
	Node<Game> *tree = arena.alloc(Game());
	std::vector<Game> state_history = { tree->state, };
	std::vector<uint> move_history;

	while (true) {
		if (tree->state.winner() != NONE) {
			break;
		}
		uint const player = tree->state.player_turn();
		uint move;
		if (player == 0) {
			// execute rollouts for MCTS policy
			for (int i = 0; i < n_rollouts; ++i) {
				tree->ucb_rollout(prng, arena);
			}
			move = tree->ucb_move();
		} else if (player == 1) {
			// execute opponent random policy
			move = tree->random_move(prng);
			// this should actually be valid. TODO: handle case
			assert(tree->is_move_explored(move));
		} else {
			assert(false);
		}
		move_history.push_back(move);
		tree = tree->child(move);
		state_history.push_back(tree->state);
	}

	return std::make_pair(std::move(state_history), std::move(move_history));
}

} // namespace mcts
