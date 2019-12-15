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

static float calcSurfaceArea(const glm::vec3 &minCorner, const glm::vec3 &maxCorner)
{
	if (glm::any(glm::greaterThan(minCorner, maxCorner)))
	{
		return 0.0f;
	}
	const glm::vec3 extent = maxCorner - minCorner;
	return (extent[0] * extent[1] + extent[0] * extent[2] + extent[1] * extent[2]) * 2.0f;
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
	const float  invd = 1.0f / (dot(s0, e0));
	const glm::vec3 d = rayOrigin - v0;
	const float  b0 = dot(d, s0) * invd;
	const glm::vec3 s1 = cross(d, e0);
	const float  b1 = dot(rayDir, s1) * invd;
	const float temp = dot(e1, s1) * invd;

	bool hit = !(b0 < 0.0f || b0 > 1.0f || b1 < 0.0f || b0 + b1 > 1.0f || temp < 0.0f || temp > rayTMax || temp < rayTMin);
	uv = glm::vec2(b0, b1);
	rayTMax = hit ? temp : rayTMax;
	return hit;
}

bool VEngine::BVH::trace(const glm::vec3 &origin, const glm::vec3 &dir, float &t)
{
	float rayTMin = 0.0f;
	float rayTMax = 10000.0f;
	glm::vec3 rayOrigin = origin;
	glm::vec3 rayDir = normalize(dir);
	glm::vec3 invRayDir;
	invRayDir[0] = 1.0f / (rayDir[0] != 0.0f ? rayDir[0] : pow(2.0f, -80.0f));
	invRayDir[1] = 1.0f / (rayDir[1] != 0.0f ? rayDir[1] : pow(2.0f, -80.0f));
	invRayDir[2] = 1.0f / (rayDir[2] != 0.0f ? rayDir[2] : pow(2.0f, -80.0f));
	glm::vec3 originDivDir = rayOrigin * invRayDir;
	glm::bvec3 dirIsNeg = lessThan(invRayDir, glm::vec3(0.0f));

	uint32_t nodesToVisit[32];
	uint32_t toVisitOffset = 0;
	uint32_t currentNodeIndex = 0;

	glm::vec2 uv = glm::vec2(0.0f);
	uint32_t triangleIdx = -1;
	uint32_t iterations = 0;
	glm::vec3 bary{};

	while (true)
	{
		++iterations;
		const BVHNode &node = m_nodes[currentNodeIndex];
		if (intersectAABB(invRayDir, originDivDir, node.m_aabbMin, node.m_aabbMax, rayTMin, rayTMax))
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
	uint32_t nodeIndex = static_cast<uint32_t>(m_nodes.size());
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
		struct BinInfo
		{
			uint32_t m_count = 0;
			glm::vec3 m_aabbMin = glm::vec3(std::numeric_limits<float>::max());
			glm::vec3 m_aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
		};
		BinInfo bins[3][8];

		const auto nodeAabbExtent = glm::max(node.m_aabbMax - node.m_aabbMin, glm::vec3(0.00000001f));

		for (size_t i = begin; i < end; ++i)
		{
			const auto &verts = m_triangles[i].m_vertices;
			const glm::vec3 centroid = ((verts[0] + verts[1] + verts[2]) / 3.0f);
			const glm::vec3 relativeOffset = (centroid - node.m_aabbMin) / nodeAabbExtent;

			glm::ivec3 binIndices = glm::clamp(glm::ivec3(relativeOffset * 8.0f), glm::ivec3(0), glm::ivec3(7));
			for (size_t j = 0; j < 3; ++j)
			{
				auto &bin = bins[j][binIndices[j]];
				bin.m_count += 1;

				bin.m_aabbMin = glm::min(bin.m_aabbMin, verts[0]);
				bin.m_aabbMin = glm::min(bin.m_aabbMin, verts[1]);
				bin.m_aabbMin = glm::min(bin.m_aabbMin, verts[2]);


				bin.m_aabbMax = glm::max(bin.m_aabbMax, verts[0]);
				bin.m_aabbMax = glm::max(bin.m_aabbMax, verts[1]);
				bin.m_aabbMax = glm::max(bin.m_aabbMax, verts[2]);
			}

		}

		const float invNodeSurfaceArea = 1.0f / glm::max(calcSurfaceArea(node.m_aabbMin, node.m_aabbMax), 0.000000001f);
		float lowestCost = std::numeric_limits<float>::max();
		uint32_t bestAxis = 0;
		uint32_t bestBin = 0;

		float costs[3][8];
		for (size_t i = 0; i < 3; ++i)
		{
			for (size_t j = 0; j < 7; ++j)
			{
				glm::vec3 b0aabbMin = glm::vec3(std::numeric_limits<float>::max());
				glm::vec3 b0aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
				glm::vec3 b1aabbMin = glm::vec3(std::numeric_limits<float>::max());
				glm::vec3 b1aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
				uint32_t count0 = 0;
				uint32_t count1 = 0;
				for (size_t k = 0; k <= j; ++k)
				{
					b0aabbMin = glm::min(b0aabbMin, bins[i][k].m_aabbMin);
					b0aabbMax = glm::max(b0aabbMax, bins[i][k].m_aabbMax);
					count0 += bins[i][k].m_count;
				}
				for (size_t k = j + 1; k < 8; ++k)
				{
					b1aabbMin = glm::min(b1aabbMin, bins[i][k].m_aabbMin);
					b1aabbMax = glm::max(b1aabbMax, bins[i][k].m_aabbMax);
					count1 += bins[i][k].m_count;
				}
				float a0 = calcSurfaceArea(b0aabbMin, b0aabbMax);
				float a1 = calcSurfaceArea(b1aabbMin, b1aabbMax);
				float cost = 0.125f + (count0 * a0 + count1 * a1) * invNodeSurfaceArea;
				cost = count0 == 0 || count1 == 0 ? std::numeric_limits<float>::max() : cost; // avoid 0/N partitions
				costs[i][j] = cost;
				assert(cost == cost);
				if (cost < lowestCost)
				{
					lowestCost = cost;
					bestAxis = i;
					bestBin = j;
				}
			}
		}

		const Triangle *pMid = std::partition(m_triangles.data() + begin, m_triangles.data() + end, [&](const auto &item)
			{
				const auto &triangle = item.m_vertices;
				glm::vec3 centroid = ((triangle[0] + triangle[1] + triangle[2]) / 3.0f);
				float relativeOffset = ((centroid[bestAxis] - node.m_aabbMin[bestAxis]) / nodeAabbExtent[bestAxis]);
				uint32_t bucketIdx = static_cast<uint32_t>(glm::clamp(8.0f * relativeOffset, 0.0f, 7.0f));
				return bucketIdx <= bestBin;
			});

		uint32_t bestSplitCandidate = static_cast<uint32_t>(pMid - m_triangles.data());

		// couldnt partition triangles; take middle instead
		if (bestSplitCandidate == begin || bestSplitCandidate == end)
		{
			bestAxis = nodeAabbExtent[0] < nodeAabbExtent[1] ? 0 : nodeAabbExtent[1] < nodeAabbExtent[2] ? 1 : 2;
			bestSplitCandidate = (begin + end) / 2;
			std::nth_element(m_triangles.data() + begin, m_triangles.data() + bestSplitCandidate, m_triangles.data() + end, [&](const auto &lhs, const auto &rhs)
				{
					return ((lhs.m_vertices[0][bestAxis] + lhs.m_vertices[1][bestAxis] + lhs.m_vertices[2][bestAxis]) / 3.0f) < ((rhs.m_vertices[0][bestAxis] + rhs.m_vertices[1][bestAxis] + rhs.m_vertices[2][bestAxis]) / 3.0f);
				});
		}

		assert(bestSplitCandidate != begin && bestSplitCandidate != end);

		// compute children
		uint32_t leftChildIndex = buildRecursive(begin, bestSplitCandidate);
		assert(leftChildIndex == nodeIndex + 1);
		node.m_offset = buildRecursive(bestSplitCandidate, end);
		node.m_primitiveCountAxis |= (bestAxis << 8);
	}
	else
	{
		// leaf node
		node.m_offset = static_cast<uint32_t>(begin);
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
