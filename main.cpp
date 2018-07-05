#include <iostream>
#include <random>

#include "mcts.hpp"
#include "tictactoe.hpp"

int main(int argc, char **argv)
{
	// allow deterministic seeding from command line.
	int seed;
	if (argc > 1) {
		seed = std::stoi(argv[1]);
	} else {
		seed = std::random_device()();
	}
	std::default_random_engine prng;
	prng.seed(seed);

	size_t const ROLLOUTS = 100'000;
	auto history = mcts::play_vs_random<TicTacToe>(prng, ROLLOUTS);

	for (auto &&state : history.first) {
		std::cout << state << "\n";
	}
	auto winner = history.first.back().winner();
	if (winner == mcts::TIE) {
		std::cout << "Tie game\n";
	}
	else {
		std::cout << "player " << winner << " wins\n";
	}
}

