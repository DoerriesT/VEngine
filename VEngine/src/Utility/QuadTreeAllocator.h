#pragma once
#include <cstdint>

namespace VEngine
{
	class QuadTreeAllocator
	{
	public:
		explicit QuadTreeAllocator(uint32_t size, uint32_t minAllocSize, uint32_t maxAllocSize);
		~QuadTreeAllocator();
		bool alloc(uint32_t size, uint32_t &tileOffsetX, uint32_t &tileOffsetY, uint32_t &tileSize);
		void free(uint32_t tileOffsetX, uint32_t tileOffsetY, uint32_t tileSize);
		void freeAll();
	
	private:
		struct Node
		{
			uint32_t m_childIndices[4] = { uint32_t(-1), uint32_t(-1), uint32_t(-1), uint32_t(-1) };
			uint32_t m_positionX = 0;
			uint32_t m_positionY = 0;
			uint32_t m_level = 0;
			uint32_t m_maxFreeSize = 0;
		};

		Node *m_nodes;
		uint32_t m_levelCount;
		uint32_t m_nodeCount;
		uint32_t m_size;
		uint32_t m_minAllocSize;
		uint32_t m_maxAllocSize;
		uint32_t m_log2Size;

		void buildTree(Node &parent, uint32_t level, uint32_t &nodeIndex);
		Node *allocNode(Node &currentNode, uint32_t level);
		void freeNode(Node &currentNode, uint32_t size, uint32_t offsetX, uint32_t offsetY);
		void freeAllRecursive(Node &parent, uint32_t level);
	};
}