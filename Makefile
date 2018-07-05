mcts: *.hpp *.cpp
	clang++ -std=c++1y -O3 -g main.cpp -o mcts

clean:
	rm mcts
