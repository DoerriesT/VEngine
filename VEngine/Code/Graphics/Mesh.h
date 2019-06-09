#pragma once
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <vector>

namespace VEngine
{
	typedef glm::vec3 VertexPosition;
	typedef glm::vec3 VertexNormal;
	typedef glm::vec2 VertexTexCoord;

	struct Vertex
	{
		VertexPosition position;
		VertexNormal normal;
		VertexTexCoord texCoord;
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
		glm::vec3 m_minCorner;
		glm::vec3 m_maxCorner;
		uint32_t m_vertexCount;
		uint16_t m_indexCount;
		uint8_t *m_positions;
		uint8_t *m_normals;
		uint8_t *m_texCoords;
		uint16_t *m_indices;
	};
}