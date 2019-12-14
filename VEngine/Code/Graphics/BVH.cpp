#include "BVH.h"
#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

void VEngine::BVH::build(size_t triangleCount, const glm::vec3 *vertices, uint32_t maxLeafTriangles)
{
	m_maxLeafTriangles = maxLeafTriangles;
	m_nodes.clear();
	m_nodes.reserve(triangleCount - 1 + triangleCount);
	m_triangles.clear();
	m_triangles.resize(triangleCount);
	memcpy(m_triangles.data(), vertices, triangleCount * sizeof(Triangle));

	buildRecursive(0, triangleCount);
}

const std::vector<VEngine::BVHNode> &VEngine::BVH::getNodes() const
{
	return m_nodes;
}

const std::vector<VEngine::Triangle> &VEngine::BVH::getTriangles() const
{
	return m_triangles;
}

uint32_t VEngine::BVH::getDepth(uint32_t node) const
{
	if ((m_nodes[node].m_primitiveCountAxis >> 16))
	{
		return 1;
	}
	return 1 + glm::max(getDepth(++node), getDepth(m_nodes[node].m_offset));
}

bool VEngine::BVH::validate()
{
	bool *reachedLeaves = new bool[m_triangles.size()];
	memset(reachedLeaves, 0, m_triangles.size() * sizeof(bool));
	bool ret = validateRecursive(0, reachedLeaves);
	for (size_t i = 0; i < m_triangles.size(); ++i)
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

static bool intersectAABB(const glm::vec3 invDir, const glm::vec3 originDivDir, const glm::vec3 aabbMin, const glm::vec3 aabbMax, const float rayTMin, float &rayTMax)
{
	const glm::vec3 tMin = aabbMin * invDir - originDivDir;
	const glm::vec3 tMax = aabbMax * invDir - originDivDir;
	const glm::vec3 t1 = min(tMin, tMax);
	const glm::vec3 t2 = max(tMin, tMax);
	const float tNear = glm::max(glm::max(t1.x, t1.y), glm::max(t1.z, 0.0f));
	const float tFar = glm::min(glm::min(t2.x, t2.y), t2.z);
	bool hit = tNear <= tFar && tNear <= rayTMax && tNear >= rayTMin;
	//rayTMax = hit ? tNear : rayTMax;
	return hit;
}

static bool intersectTriangle(const glm::vec3 rayOrigin, const glm::vec3 rayDir, const glm::vec3 v0, const glm::vec3 v1, const glm::vec3 v2, const float rayTMin, float &rayTMax, glm::vec2 &uv)
{
	const glm::vec3 e0 = v1 - v0;
	const glm::vec3 e1 = v2 - v0;
	const glm::vec3 s0 = cross(rayDir, e1);
	const float  invd = 1.0 / (dot(s0, e0));
	const glm::vec3 d = rayOrigin - v0;
	const float  b0 = dot(d, s0) * invd;
	const glm::vec3 s1 = cross(d, e0);
	const float  b1 = dot(rayDir, s1) * invd;
	const float temp = dot(e1, s1) * invd;

	bool hit = !(b0 < 0.0 || b0 > 1.0 || b1 < 0.0 || b0 + b1 > 1.0 || temp < 0.0 || temp > rayTMax || temp < rayTMin);
	uv = glm::vec2(b0, b1);
	rayTMax = hit ? temp : rayTMax;
	return hit;
}

bool VEngine::BVH::trace(const glm::vec3 &origin, const glm::vec3 &dir, float &t)
{
	float rayTMin = 0.0;
	float rayTMax = 10000.0;
	glm::vec3 rayOrigin = origin;
	glm::vec3 rayDir = normalize(dir);
	glm::vec3 invRayDir;
	invRayDir[0] = 1.0 / (rayDir[0] != 0.0 ? rayDir[0] : pow(2.0, -80.0));
	invRayDir[1] = 1.0 / (rayDir[1] != 0.0 ? rayDir[1] : pow(2.0, -80.0));
	invRayDir[2] = 1.0 / (rayDir[2] != 0.0 ? rayDir[2] : pow(2.0, -80.0));
	glm::vec3 originDivDir = rayOrigin * invRayDir;
	glm::bvec3 dirIsNeg = lessThan(invRayDir, glm::vec3(0.0));

	uint32_t nodesToVisit[32];
	uint32_t toVisitOffset = 0;
	uint32_t currentNodeIndex = 0;

	glm::vec2 uv = glm::vec2(0.0);
	uint32_t triangleIdx = -1;
	uint32_t iterations = 0;
	glm::vec3 bary{};

	while (true)
	{
		++iterations;
		const BVHNode &node = m_nodes[currentNodeIndex];
		if (intersectAABB(invRayDir, originDivDir, node.m_aabbMin,node.m_aabbMax, rayTMin, rayTMax))
		{
			const uint32_t primitiveCount = (node.m_primitiveCountAxis >> 16);
			if (primitiveCount > 0)
			{
				for (uint32_t i = 0; i < primitiveCount; ++i)
				{
					Triangle triangle = m_triangles[node.m_offset + i];
					if (intersectTriangle(rayOrigin, rayDir, triangle.m_vertices[0], triangle.m_vertices[1], triangle.m_vertices[2], rayTMin, rayTMax, uv))
					{
						triangleIdx = node.m_offset + i;
					}
				}
				if (toVisitOffset == 0)
				{
					break;
				}
				currentNodeIndex = nodesToVisit[--toVisitOffset];
			}
			else
			{
				if (dirIsNeg[(node.m_primitiveCountAxis >> 8) & 0xFF])
				{
					nodesToVisit[toVisitOffset++] = currentNodeIndex + 1;
					currentNodeIndex = node.m_offset;
				}
				else
				{
					nodesToVisit[toVisitOffset++] = node.m_offset;
					currentNodeIndex = currentNodeIndex + 1;
				}
			}
		}
		else
		{
			if (toVisitOffset == 0)
			{
				break;
			}
			currentNodeIndex = nodesToVisit[--toVisitOffset];
		}
	}
	t = rayTMax;
	return triangleIdx != -1;
}

uint32_t VEngine::BVH::buildRecursive(size_t begin, size_t end)
{
	uint32_t nodeIndex = m_nodes.size();
	m_nodes.push_back({});
	BVHNode node = {};
	node.m_aabbMin = glm::vec3(std::numeric_limits<float>::max());
	node.m_aabbMax = glm::vec3(std::numeric_limits<float>::lowest());

	// compute node aabb
	for (size_t i = begin; i < end; ++i)
	{
		const auto &nodeVertices = m_triangles[i].m_vertices;
		node.m_aabbMin = glm::min(node.m_aabbMin, nodeVertices[0]);
		node.m_aabbMin = glm::min(node.m_aabbMin, nodeVertices[1]);
		node.m_aabbMin = glm::min(node.m_aabbMin, nodeVertices[2]);
		

		node.m_aabbMax = glm::max(node.m_aabbMax, nodeVertices[0]);
		node.m_aabbMax = glm::max(node.m_aabbMax, nodeVertices[1]);
		node.m_aabbMax = glm::max(node.m_aabbMax, nodeVertices[2]);
	}

	if (end - begin > m_maxLeafTriangles)
	{
		// determine best axis to split this node
		auto aabbExtent = node.m_aabbMax - node.m_aabbMin;
		uint32_t axis = (aabbExtent[0] > aabbExtent[1] ? 0 : aabbExtent[1] > aabbExtent[2] ? 1 : 2);
		node.m_primitiveCountAxis |= axis << 8;

		// sort triangles along split axis
		std::sort(m_triangles.data() + begin, m_triangles.data() + end, [&](const auto &lhs, const auto &rhs)
			{
				return ((lhs.m_vertices[0] + lhs.m_vertices[1] + lhs.m_vertices[2]) / 3.0f)[axis] < ((rhs.m_vertices[0] + rhs.m_vertices[1] + rhs.m_vertices[2]) / 3.0f)[axis];
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
		uint32_t leftChildIndex = buildRecursive(begin, bestSplitCandidate);
		assert(leftChildIndex == nodeIndex + 1);
		node.m_offset = buildRecursive(bestSplitCandidate, end);
	}
	else
	{
		// leaf node
		node.m_offset = begin;
		node.m_primitiveCountAxis |= (end - begin) << 16;
	}
	m_nodes[nodeIndex] = node;
	return nodeIndex;
}

bool VEngine::BVH::validateRecursive(uint32_t node, bool *reachedLeafs)
{
	if (m_nodes[node].m_primitiveCountAxis >> 16)
	{
		for (size_t i = 0; i < (m_nodes[node].m_primitiveCountAxis >> 16); ++i)
		{
			if (reachedLeafs[m_nodes[node].m_offset + i])
			{
				return false;
			}
			reachedLeafs[m_nodes[node].m_offset + i] = true;
		}
		
		return true;
	}
	else
	{
		return validateRecursive(node + 1, reachedLeafs) && validateRecursive(m_nodes[node].m_offset, reachedLeafs);
	}
}
