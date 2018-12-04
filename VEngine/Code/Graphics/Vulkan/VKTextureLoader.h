#pragma once
#include <map>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>
#include "VKImageData.h"

namespace VEngine
{
	struct VKTexture
	{
		uint32_t m_id;
		VKImageData m_imageData;
		VkSampler m_sampler;
		unsigned int m_width;
		unsigned int m_height;
		unsigned int m_layers;
		unsigned int m_levels;
	};

	class VKTextureLoader
	{
	public:
		explicit VKTextureLoader();
		uint32_t load(const char *filepath);
		void free(uint32_t id);
		std::vector<VKTexture *> getTextures();

	private:
		std::map<uint32_t, VKTexture> m_idToTexture;
		uint32_t m_nextId;
		std::vector<uint32_t> m_freeIds;
	};
}