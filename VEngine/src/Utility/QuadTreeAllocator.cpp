#include "QuadTreeAllocator.h"
#include "Utility.h"
#include <cassert>

VEngine::QuadTreeAllocator::QuadTreeAllocator(uint32_t size, uint32_t minAllocSize, uint32_t maxAllocSize)
	:m_nodes(),
	m_levelCount(),
	m_nodeCount(),
	m_size(size),
	m_minAllocSize(minAllocSize),
	m_maxAllocSize(maxAllocSize),
	m_log2Size(Utility::findLastSetBit(size))
{
	assert(size > minAllocSize && size > maxAllocSize && maxAllocSize >= minAllocSize);
	assert(Utility::isPowerOfTwo(size) && Utility::isPowerOfTwo(minAllocSize) && Utility::isPowerOfTwo(maxAllocSize));

	m_levelCount = m_log2Size - Utility::findLastSetBit(minAllocSize) + 1;

	m_nodeCount = 1;
	uint32_t multiplier = 1;
	for (uint32_t i = 1; i < m_levelCount; ++i)
	{
		multiplier *= 4;
		m_nodeCount += multiplier;
	}

	m_nodes = new Node[m_nodeCount];
	assert(m_nodes);

	uint32_t nodeIndex = 0;
	m_nodes[0].m_maxFreeSize = m_size;
	buildTree(m_nodes[0], 0, nodeIndex);
}

VEngine::QuadTreeAllocator::~QuadTreeAllocator()
{
	delete[] m_nodes;
	m_nodes = nullptr;
}

bool VEngine::QuadTreeAllocator::alloc(uint32_t size, uint32_t &tileOffsetX, uint32_t &tileOffsetY, uint32_t &tileSize)
{
	// clamp size between min and max alloc size
	size = size < m_minAllocSize ? m_minAllocSize : size;
	size = size > m_maxAllocSize ? m_maxAllocSize : size;

	// round to next power of two
	size = Utility::nextPowerOfTwo(size);

	uint32_t requiredLevel = Utility::findLastSetBit(m_size) - Utility::findLastSetBit(size);

	auto *result = allocNode(m_nodes[0], requiredLevel);
	if (result)
	{
		tileOffsetX = result->m_positionX;
		tileOffsetY = result->m_positionY;
		tileSize = 1u << (m_log2Size - result->m_level);
		return true;
	}

	return false;
}

void VEngine::QuadTreeAllocator::free(uint32_t tileOffsetX, uint32_t tileOffsetY, uint32_t tileSize)
{
	assert(tileSize >= m_minAllocSize && tileSize <= m_maxAllocSize);
	assert(tileOffsetX + tileSize <= m_size && tileOffsetY + tileSize <= m_size);
	assert(Utility::isPowerOfTwo(tileSize));

	freeNode(m_nodes[0], tileSize, tileOffsetX, tileOffsetY);
}

void VEngine::QuadTreeAllocator::freeAll()
{
	m_nodes[0].m_maxFreeSize = m_size;
	freeAllRecursive(m_nodes[0], 0);
}

void VEngine::QuadTreeAllocator::buildTree(Node &parent, uint32_t level, uint32_t &nodeIndex)
{
	++level;
	if (level == m_levelCount)
	{
		return;
	}

	for (size_t i = 0; i < 4; ++i)
	{
		parent.m_childIndices[i] = ++nodeIndex;
		assert(nodeIndex < m_nodeCount);
		auto &currentNode = m_nodes[parent.m_childIndices[i]];
		uint32_t tileSize = 1u << (m_log2Size - level);
		uint32_t offsetX = (i == 0 || i == 2) ? 0 : tileSize;
		uint32_t offsetY = (i < 2) ? 0 : tileSize;
		currentNode.m_positionX = offsetX + parent.m_positionX;
		currentNode.m_positionY = offsetY + parent.m_positionY;
		currentNode.m_level = level;
		currentNode.m_maxFreeSize = tileSize;

		buildTree(currentNode, level, nodeIndex);
	}
}

VEngine::QuadTreeAllocator::Node *VEngine::QuadTreeAllocator::allocNode(Node &currentNode, uint32_t level)
{
	assert(level < m_levelCount);
	uint32_t requestedSize = (1u << (m_log2Size - level));

	// there is not enough space in this entire subtree
	if (currentNode.m_maxFreeSize < requestedSize)
	{
		return nullptr;
	}

	// there is enough space in this subtree and we are at the correct level -> the whole tile is free -> we can return it
	if (level == currentNode.m_level)
	{
		assert(currentNode.m_maxFreeSize == requestedSize);
		// mark tile and subtree as occupied
		currentNode.m_maxFreeSize = 0;
		return &currentNode;
	}

	// check if children have enough free space and allocate from them.
	// we are guaranteed not to be in a leaf node because otherwise we would have found the correct level already
	for (size_t i = 0; i < 4; ++i)
	{
		auto *result = allocNode(m_nodes[currentNode.m_childIndices[i]], level);
		// one of the child nodes down the subtree managed to allocate
		if (result)
		{
			// determine the new m_maxFreeSize value: take the maximum of all children.
			// since a child node was just allocated, it is guaranteed that we can simply take the maximum and not the size of the current tile
			uint32_t maxFree = 0;
			for (size_t j = 0; j < 4; ++j)
			{
				uint32_t childMaxFree = m_nodes[currentNode.m_childIndices[j]].m_maxFreeSize;
				maxFree = childMaxFree > maxFree ? childMaxFree : maxFree;
			}
			currentNode.m_maxFreeSize = maxFree;
			return result;
		}
	}

	// we shouldnt be able to reach this point because we should have exited earlier, when we tested for currentNode.m_maxFreeSize < requestedSize
	assert(false);
	return nullptr;
}

void VEngine::QuadTreeAllocator::freeNode(Node &currentNode, uint32_t size, uint32_t offsetX, uint32_t offsetY)
{
	const uint32_t currentSize = 1u << (m_log2Size - currentNode.m_level);
	// found correct level -> should be correct tile
	if (currentSize == size)
	{
		assert(currentNode.m_positionX == offsetX && currentNode.m_positionY == offsetY && currentNode.m_maxFreeSize == 0);
	}
	// we need to go deeper -> find the correct child
	else
	{
		uint32_t currentHalfSize = currentSize / 2;
		uint32_t rightEdgeX = offsetX + size;
		uint32_t bottomEdgeY = offsetY + size;

		uint32_t childIdx = bottomEdgeY > (currentNode.m_positionY + currentHalfSize) ? 2 : 0;
		childIdx = rightEdgeX > (currentNode.m_positionX + currentHalfSize) ? childIdx + 1 : childIdx;

		freeNode(m_nodes[currentNode.m_childIndices[childIdx]], size, offsetX, offsetY);
	}

	// recalculate maximum free size
	bool allFree = true;
	uint32_t childSize = currentSize / 2;
	uint32_t maxFree = 0;
	for (size_t j = 0; j < 4; ++j)
	{
		uint32_t childIdx = currentNode.m_childIndices[j];
		uint32_t childMaxFree = childIdx != -1 ? m_nodes[childIdx].m_maxFreeSize : childSize;
		maxFree = childMaxFree > maxFree ? childMaxFree : maxFree;
		assert(childMaxFree <= childSize);
		if (childMaxFree != childSize)
		{
			allFree = false;
		}
	}
	currentNode.m_maxFreeSize = allFree ? currentSize : maxFree;
}

void VEngine::QuadTreeAllocator::freeAllRecursive(Node &parent, uint32_t level)
{
	++level;
	if (level == m_levelCount)
	{
		return;
	}

	for (size_t i = 0; i < 4; ++i)
	{
		auto &currentNode = m_nodes[parent.m_childIndices[i]];
		uint32_t tileSize = 1u << (m_log2Size - level);
		currentNode.m_maxFreeSize = tileSize;

		freeAllRecursive(currentNode, level);
	}
}
