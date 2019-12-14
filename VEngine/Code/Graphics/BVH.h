#pragma once
#include <glm/vec3.hpp>
#include <vector>

namespace VEngine
{
	struct InternalNode
	{
		glm::vec3 m_aabbMin;
		glm::vec3 m_aabbMax;
		uint32_t m_leftChild;
		uint32_t m_rightChild;
	};
	struct LeafNode
	{
		glm::vec3 m_vertices[3];
	};

	class BVH
	{
	public:
		void build(size_t triangleCount, const glm::vec3 *vertices);
		const std::vector<InternalNode> &getInternalNodes() const;
		const std::vector<LeafNode> &getLeafNodes() const;
		uint32_t getDepth(uint32_t node = 0) const;
		bool validate();
		bool trace(const glm::vec3 &origin, const glm::vec3 &dir, float &t);
		
	private:
		

		std::vector<InternalNode> m_internalNodes;
		std::vector<LeafNode> m_leafNodes;

		uint32_t buildRecursive(size_t begin, size_t end);
		bool validateRecursive(uint32_t node, bool *reachedLeafs);
	};
}