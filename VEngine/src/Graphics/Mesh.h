#pragma once
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/fwd.hpp>
#include <vector>
#include "Handles.h"

namespace VEngine
{
	typedef glm::u16vec3 VertexPosition;
	typedef glm::u16vec4 VertexQTangent;
	typedef glm::u16vec2 VertexTexCoord;

	struct Vertex
	{
		VertexPosition position;
		VertexQTangent qtangent;
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
		glm::vec2 m_minTexCoord;
		glm::vec2 m_maxTexCoord;
		uint32_t m_vertexCount;
		uint32_t m_indexCount;
		uint8_t *m_positions;
		uint8_t *m_qtangents;
		uint8_t *m_texCoords;
		uint16_t *m_indices;
	};
}