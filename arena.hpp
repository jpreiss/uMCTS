#pragma once

#include <forward_list>
#include <vector>

// a simple one-type arena allocator built using STL containers.
// Arena allocation often has big performance benefits in tree code.

template <typename T, int NBlock = 4096>
class Arena
{
public:
	// arguments are forwarded to T constructor.
	template <typename... Args> T *alloc(Args... args)
	{
		if (blocks.empty() || blocks.front().size() == NBlock) {
			blocks.emplace_front();
			blocks.front().reserve(NBlock);
		}
		blocks.front().emplace_back(std::forward<Args...>(args...));
		return &blocks.front().back();
	}

	void clear()
	{
		// TODO could reuse blocks, but better to set NBlock large enough
		// that the malloc/freeing should be insignificant
		blocks.clear();
	}

private:
	std::forward_list<std::vector<T>> blocks;
};

