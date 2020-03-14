#pragma once
#include <glm/vec3.hpp>
#include <vector>

namespace VEngine
{
	struct BVHNode
	{
		glm::vec3 m_aabbMin;
		glm::vec3 m_aabbMax;
		uint32_t m_offset; // primitive offset for leaf; second child offset for interior node
		uint32_t m_primitiveCountAxis;  // 0-7 padding; 8-15 axis; 16-31 prim count (0 -> interior node)
	};

	struct Triangle
	{
		glm::vec3 m_vertices[3];
	};

	class BVH
	{
	public:
		void build(size_t triangleCount, const glm::vec3 *vertices, uint32_t maxLeafTriangles);
		const std::vector<BVHNode> &getNodes() const;
		const std::vector<Triangle> &getTriangles() const;
		uint32_t getDepth(uint32_t node = 0) const;
		bool validate();
		bool trace(const glm::vec3 &origin, const glm::vec3 &dir, float &t);
		
	private:
		
		uint32_t m_maxLeafTriangles = 1;
		std::vector<BVHNode> m_nodes;
		std::vector<Triangle> m_triangles;

		uint32_t buildRecursive(size_t begin, size_t end);
		bool validateRecursive(uint32_t node, bool *reachedLeafs);
	};
}