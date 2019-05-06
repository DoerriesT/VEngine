#pragma once
#include <vector>
#include <bitset>
#include "Graphics/Vulkan/volk.h"
#include "VKImage.h"
#include "VKBuffer.h"
#include "Graphics/RendererConsts.h"
#include "Handles.h"

namespace VEngine
{
	class VKTextureLoader
	{
	public:
		explicit VKTextureLoader(VKBuffer &stagingBuffer);
		VKTextureLoader(const VKTextureLoader &) = delete;
		VKTextureLoader(const VKTextureLoader &&) = delete;
		VKTextureLoader &operator= (const VKTextureLoader &) = delete;
		VKTextureLoader &operator= (const VKTextureLoader &&) = delete;
		~VKTextureLoader();
		TextureHandle load(const char *filepath);
		void free(TextureHandle handle);
		void getDescriptorImageInfos(const VkDescriptorImageInfo **data, size_t &count);

	private:
		struct VKTexture
		{
			VkImageView m_view;
			VkSampler m_sampler;
			VKImage m_image;
		};

		std::bitset<RendererConsts::TEXTURE_ARRAY_SIZE> m_usedSlots;
		VKBuffer &m_stagingBuffer;
		VKTexture m_dummyTexture;
		VKTexture m_textures[RendererConsts::TEXTURE_ARRAY_SIZE];
		VkDescriptorImageInfo m_descriptorImageInfos[RendererConsts::TEXTURE_ARRAY_SIZE];
	};
}