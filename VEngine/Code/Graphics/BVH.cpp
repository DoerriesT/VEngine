#include "BVH.h"
#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

void VEngine::BVH::build(size_t triangleCount, const glm::vec3 *vertices)
{
	m_internalNodes.clear();
	m_internalNodes.reserve(triangleCount - 1 + triangleCount);
	m_leafNodes.clear();
	m_leafNodes.resize(triangleCount);
	memcpy(m_leafNodes.data(), vertices, triangleCount * sizeof(LeafNode));

	buildRecursive(0, triangleCount);
}

const std::vector<VEngine::InternalNode> &VEngine::BVH::getInternalNodes() const
{
	return m_internalNodes;
}

const std::vector<VEngine::LeafNode> &VEngine::BVH::getLeafNodes() const
{
	return m_leafNodes;
}

uint32_t VEngine::BVH::getDepth(uint32_t node) const
{
	if (node >= (1u << 31))
	{
		return 1;
	}
	return 1 + glm::max(getDepth(m_internalNodes[node].m_leftChild), getDepth(m_internalNodes[node].m_rightChild));
}

bool VEngine::BVH::validate()
{
	bool *reachedLeaves = new bool[m_leafNodes.size()];
	memset(reachedLeaves, 0, m_leafNodes.size() * sizeof(bool));
	bool ret = validateRecursive(0, reachedLeaves);
	for (size_t i = 0; i < m_leafNodes.size(); ++i)
	{
		if (!reachedLeaves[i])
		{
			delete[] reachedLeaves;
			return false;
		}
	}
	delete[] reachedLeaves;
	return ret;
}

static bool isLeafNode(uint32_t node)
{
	return node >= (1u << 31);
}

static bool intersectAABB(const glm::vec3 &invDir, const glm::vec3 &originDivDir, const glm::vec3 &aabbMin, const glm::vec3 &aabbMax, float &tNear)
{
	const glm::vec3 tMin = aabbMin * invDir - originDivDir;
	const glm::vec3 tMax = aabbMax * invDir - originDivDir;
	const glm::vec3 t1 = min(tMin, tMax);
	const glm::vec3 t2 = max(tMin, tMax);
	tNear = glm::max(glm::max(t1.x, t1.y), glm::max(t1.z, 0.0f));
	const float tFar = glm::min(glm::min(t2.x, t2.y), t2.z);
	return tNear <= tFar;
}

static bool intersectTriangle(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2, glm::vec3 &uvt)
{
	const glm::vec3 e0 = v1 - v0;
	const glm::vec3 e1 = v2 - v0;
	const glm::vec3 s0 = glm::cross(rayDir, e1);
	const float  invd = 1.0 / (dot(s0, e0));
	const glm::vec3 d = rayOrigin - v0;
	const float  b0 = dot(d, s0) * invd;
	const glm::vec3 s1 = glm::cross(d, e0);
	const float  b1 = glm::dot(rayDir, s1) * invd;
	const float temp = glm::dot(e1, s1) * invd;

	uvt = glm::vec3(b0, b1, temp);
	return !(b0 < 0.0 || b0 > 1.0 || b1 < 0.0 || b0 + b1 > 1.0 || temp < 0.0/* || temp > isect.uvwt.w*/);
}

bool VEngine::BVH::trace(const glm::vec3 &origin, const glm::vec3 &dir, float &t)
{
	glm::vec3 rayOrigin = origin;
	glm::vec3 rayDir = glm::normalize(dir);
	glm::vec3 invRayDir = 1.0f / rayDir;
	//invRayDir[0] = 1.0 / (rayDir[0] != 0.0 ? rayDir[0] : pow(2.0, -80.0));
	//invRayDir[1] = 1.0 / (rayDir[1] != 0.0 ? rayDir[1] : pow(2.0, -80.0));
	//invRayDir[2] = 1.0 / (rayDir[2] != 0.0 ? rayDir[2] : pow(2.0, -80.0));
	glm::vec3 originDivDir = rayOrigin * invRayDir;

	uint32_t stack[32];
	uint32_t stackPtr = 0;
	stack[stackPtr++] = -1;

	uint32_t curNodeIdx = 0;
	uint32_t triangleIdx = -1;
	glm::vec3 barycentrics = glm::vec3(-1.0);
	uint32_t iterations = 0;

	while (curNodeIdx != -1)
	{
		assert(stackPtr < 32);
		++iterations;
		InternalNode node = m_internalNodes[curNodeIdx];
		if (isLeafNode(node.m_leftChild))
		{
			LeafNode triangle = m_leafNodes[node.m_leftChild - (1u << 31)];
			if (intersectTriangle(rayOrigin, rayDir, triangle.m_vertices[0], triangle.m_vertices[1], triangle.m_vertices[2], barycentrics))
			{
				triangleIdx = node.m_leftChild - (1u << 31);
				t = barycentrics.z;
				break;
			}
			else
			{
				curNodeIdx = stack[--stackPtr]; // pop
			}
		}
		else
		{
			InternalNode leftChild = m_internalNodes[node.m_leftChild];
			float leftTMin = 0.0f;
			bool hitLeft = intersectAABB(invRayDir, originDivDir, leftChild.m_aabbMin, leftChild.m_aabbMax, leftTMin);

			InternalNode rightChild = m_internalNodes[node.m_rightChild];
			float rightTMin = 0.0f;
			bool hitRight = intersectAABB(invRayDir, originDivDir, rightChild.m_aabbMin, rightChild.m_aabbMax, rightTMin);

			if (!hitLeft && !hitRight)
			{
				curNodeIdx = stack[--stackPtr]; // pop
			}
			else
			{
				curNodeIdx = hitLeft ? node.m_leftChild : node.m_rightChild;
				if (hitLeft && hitRight)
				{
					stack[stackPtr++] = node.m_rightChild; // push
				}
			}
		}
	}
	return triangleIdx != -1;
}

uint32_t VEngine::BVH::buildRecursive(size_t begin, size_t end)
{
	uint32_t nodeIndex = m_internalNodes.size();
	m_internalNodes.push_back({});
	InternalNode internalNode = {};
	internalNode.m_aabbMin = glm::vec3(std::numeric_limits<float>::max());
	internalNode.m_aabbMax = glm::vec3(std::numeric_limits<float>::lowest());

	// compute node aabb
	for (size_t i = begin; i < end; ++i)
	{
		const auto &nodeVertices = m_leafNodes[i].m_vertices;
		internalNode.m_aabbMin = glm::min(internalNode.m_aabbMin, nodeVertices[0]);
		internalNode.m_aabbMin = glm::min(internalNode.m_aabbMin, nodeVertices[1]);
		internalNode.m_aabbMin = glm::min(internalNode.m_aabbMin, nodeVertices[2]);
		

		internalNode.m_aabbMax = glm::max(internalNode.m_aabbMax, nodeVertices[0]);
		internalNode.m_aabbMax = glm::max(internalNode.m_aabbMax, nodeVertices[1]);
		internalNode.m_aabbMax = glm::max(internalNode.m_aabbMax, nodeVertices[2]);
	}

	if (end - begin > 1)
	{
		// determine best axis to split this node
		auto aabbExtent = internalNode.m_aabbMax - internalNode.m_aabbMin;
		uint32_t splitAxis = aabbExtent[0] > aabbExtent[1] ? 0 : aabbExtent[1] > aabbExtent[2] ? 1 : 2;

		// sort triangles along split axis
		std::sort(m_leafNodes.data() + begin, m_leafNodes.data() + end, [&](const auto &lhs, const auto &rhs)
			{
				return ((lhs.m_vertices[0] + lhs.m_vertices[1] + lhs.m_vertices[2]) / 3.0f)[splitAxis] < ((rhs.m_vertices[0] + rhs.m_vertices[1] + rhs.m_vertices[2]) / 3.0f)[splitAxis];
			});

		// determine best split
		uint32_t bestSplitCandidate = (end - begin) / 2 + begin;
		float lowestSurfaceArea = std::numeric_limits<float>::max();

		//const uint32_t kSplitsToTest = 64;
		//uint32_t splitInc = (end - 1) - (begin + 1) / kSplitsToTest;
		//
		//for (size_t split = begin + 1; split < end - 1; split += splitInc)
		//{
		//	glm::vec3 aabbMinLeft = glm::vec3(std::numeric_limits<float>::max());
		//	glm::vec3 aabbMaxLeft = glm::vec3(std::numeric_limits<float>::lowest());
		//	glm::vec3 aabbMinRight = glm::vec3(std::numeric_limits<float>::max());
		//	glm::vec3 aabbMaxRight = glm::vec3(std::numeric_limits<float>::lowest());
		//
		//	for (size_t i = begin; i < end; ++i)
		//	{
		//		glm::vec3 &aabbMin = i < split ? aabbMinLeft : aabbMinRight;
		//		glm::vec3 &aabbMax = i < split ? aabbMaxLeft : aabbMaxRight;
		//
		//		const auto &nodeVertices = m_leafNodes[i].m_vertices;
		//		aabbMin = glm::min(internalNode.m_aabbMin, nodeVertices[0]);
		//		aabbMax = glm::max(internalNode.m_aabbMin, nodeVertices[0]);
		//		aabbMin = glm::min(internalNode.m_aabbMin, nodeVertices[1]);
		//		aabbMax = glm::max(internalNode.m_aabbMin, nodeVertices[1]);
		//		aabbMin = glm::min(internalNode.m_aabbMin, nodeVertices[2]);
		//		aabbMax = glm::max(internalNode.m_aabbMin, nodeVertices[2]);
		//	}
		//
		//	glm::vec3 extentLeft = aabbMaxLeft - aabbMinLeft;
		//	glm::vec3 extentRight = aabbMaxRight - aabbMinRight;
		//	float surfaceArea = extentLeft[0] * extentLeft[1] * extentLeft[2] + extentRight[0] * extentRight[1] * extentRight[2];
		//	if (surfaceArea < lowestSurfaceArea)
		//	{
		//		lowestSurfaceArea = surfaceArea;
		//		bestSplitCandidate = split;
		//	}
		//}

		// compute children
		internalNode.m_leftChild = buildRecursive(begin, bestSplitCandidate);
		internalNode.m_rightChild = buildRecursive(bestSplitCandidate, end);
	}
	else
	{
		// leaf node
		internalNode.m_leftChild = begin | (1u << 31);
		internalNode.m_rightChild = -1;
	}
	m_internalNodes[nodeIndex] = internalNode;
	return nodeIndex;
}

bool VEngine::BVH::validateRecursive(uint32_t node, bool *reachedLeafs)
{
	if (m_internalNodes[node].m_leftChild >= (1u << 31))
	{
		if (reachedLeafs[m_internalNodes[node].m_leftChild - (1u << 31)])
		{
			return false;
		}
		reachedLeafs[m_internalNodes[node].m_leftChild - (1u << 31)] = true;
		return true;
	}
	return validateRecursive(m_internalNodes[node].m_leftChild, reachedLeafs) && validateRecursive(m_internalNodes[node].m_rightChild, reachedLeafs);
}
