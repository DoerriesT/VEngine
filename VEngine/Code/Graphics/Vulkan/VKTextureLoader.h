#pragma once
#include <vector>
#include <bitset>
#include <vulkan/vulkan.h>
#include "VKImage.h"
#include "VKBuffer.h"
#include "VKRenderResources.h"

namespace VEngine
{
	class VKTextureLoader
	{
	public:
		explicit VKTextureLoader();
		VKTextureLoader(const VKTextureLoader &) = delete;
		VKTextureLoader(const VKTextureLoader &&) = delete;
		VKTextureLoader &operator= (const VKTextureLoader &) = delete;
		VKTextureLoader &operator= (const VKTextureLoader &&) = delete;
		~VKTextureLoader();
		size_t load(const char *filepath);
		void free(size_t id);
		void getDescriptorImageInfos(const VkDescriptorImageInfo **data, size_t &count);

	private:
		struct VKTexture
		{
			VkImageView m_view;
			VkSampler m_sampler;
			VKImage m_image;
		};

		std::bitset<TEXTURE_ARRAY_SIZE> m_usedSlots;
		VKBuffer m_stagingBuffer;
		VKTexture m_dummyTexture;
		VKTexture m_textures[TEXTURE_ARRAY_SIZE];
		VkDescriptorImageInfo m_descriptorImageInfos[TEXTURE_ARRAY_SIZE];
	};
}