#pragma once
#include <vector>
#include <bitset>
#include "Graphics/Vulkan/volk.h"
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
		TextureHandle *m_freeHandles;
		uint32_t m_freeHandleCount;
		VKBuffer &m_stagingBuffer;
		// first element is dummy image
		VkImageView m_views[RendererConsts::TEXTURE_ARRAY_SIZE + 1];
		VkImage m_images[RendererConsts::TEXTURE_ARRAY_SIZE + 1];
		VKAllocationHandle m_allocationHandles[RendererConsts::TEXTURE_ARRAY_SIZE + 1];
		VkDescriptorImageInfo m_descriptorImageInfos[RendererConsts::TEXTURE_ARRAY_SIZE];
	};
}