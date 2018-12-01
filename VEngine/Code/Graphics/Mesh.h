#pragma once
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <vector>

namespace VEngine
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texCoord;
	};

	struct Material
	{
		enum class Alpha
		{
			OPAQUE, MASKED, BLENDED
		};

		Alpha m_alpha;
		glm::vec3 m_albedoFactor;
		float m_metallicFactor;
		float m_roughnessFactor;
		glm::vec3 m_emissiveFactor;
		uint32_t m_albedoTexture;
		uint32_t m_normalTexture;
		uint32_t m_metallicTexture;
		uint32_t m_roughnessTexture;
		uint32_t m_occlusionTexture;
		uint32_t m_emissiveTexture;
		uint32_t m_displacementTexture;
	};

	struct SubMesh
	{
		std::string m_name;
		Material m_material;
		uint32_t m_vertexOffset;
		uint32_t m_vertexSize;
		uint32_t m_indexOffset;
		uint32_t m_indexSize;
		uint32_t m_indexCount;
		glm::vec3 m_min;
		glm::vec3 m_max;
	};

	struct Mesh
	{
		std::vector<SubMesh> m_subMeshes;
	};
}