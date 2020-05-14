#pragma once
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct FrustumCullData
	{
		enum
		{
			STATIC_LIGHTS_CONTENT_TYPE_BIT = 1 << 0,
			STATIC_OPAQUE_CONTENT_TYPE_BIT = 1 << 1,
			STATIC_ALPHA_TESTED_CONTENT_TYPE_BIT = 1 << 2,
			STATIC_TRANSPARENT_CONTENT_TYPE_BIT = 1 << 3,
			DYNAMIC_LIGHTS_CONTENT_TYPE_BIT = 1 << 4,
			DYNAMIC_OPAQUE_CONTENT_TYPE_BIT = 1 << 5,
			DYNAMIC_ALPHA_TESTED_CONTENT_TYPE_BIT = 1 << 6,
			DYNAMIC_TRANSPARENT_CONTENT_TYPE_BIT = 1 << 7,
			ALL_CONTENT_TYPE_BIT = (DYNAMIC_TRANSPARENT_CONTENT_TYPE_BIT << 1) - 1,
		};

		glm::vec4 m_planes[6] = {};
		uint32_t m_planeCount;
		uint32_t m_renderListIndex;
		uint32_t m_contentTypeFlags;
		float m_depthRange;

		FrustumCullData() = default;
		explicit FrustumCullData(const glm::mat4 &matrix, uint32_t planeCount, uint32_t renderListIndex, uint32_t flags, float depthRange);
	};
}