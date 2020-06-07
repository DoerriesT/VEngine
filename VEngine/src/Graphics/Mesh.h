#pragma once
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <vector>
#include "Handles.h"

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
		Texture2DHandle m_albedoTexture;
		Texture2DHandle m_normalTexture;
		Texture2DHandle m_metallicTexture;
		Texture2DHandle m_roughnessTexture;
		Texture2DHandle m_occlusionTexture;
		Texture2DHandle m_emissiveTexture;
		Texture2DHandle m_displacementTexture;
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