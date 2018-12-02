#pragma once
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vector>

namespace VEngine
{
	struct PerDrawData
	{
		glm::vec4 m_albedoFactorMetallic;
		glm::vec4 m_emissiveFactorRoughness;
		glm::mat4 m_modelMatrix;
		uint32_t m_albedoTexture;
		uint32_t m_normalTexture;
		uint32_t m_metallicTexture;
		uint32_t m_roughnessTexture;
		uint32_t m_occlusionTexture;
		uint32_t m_emissiveTexture;
		uint32_t m_displacementTexture;
		uint32_t padding;
	};

	struct DrawItem
	{
		PerDrawData m_perDrawData;
		uint64_t m_vertexOffset;
		uint32_t m_baseIndex;
		uint32_t m_indexCount;
	};

	struct DrawLists
	{
		std::vector<DrawItem> m_opaqueItems;
		std::vector<DrawItem> m_maskedItems;
		std::vector<DrawItem> m_blendedItems;
	};
}