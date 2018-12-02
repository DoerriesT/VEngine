#pragma once
#include "VKImageData.h"
#include "VKBufferData.h"

#define MAX_UNIFORM_BUFFER_INSTANCE_COUNT (512)

namespace VEngine
{
	class VKRenderer;

	struct VKRenderResources
	{
		friend VKRenderer;
	public:
		VKImageData m_colorAttachment;
		VKImageData m_depthAttachment;
		VKBufferData m_vertexBuffer;
		VKBufferData m_indexBuffer;
		VKBufferData m_mainUniformBuffer;
		VkDeviceSize m_perFrameDataSize;
		VkDeviceSize m_perDrawDataSize;
		VkFramebuffer m_mainFramebuffer;
		VkCommandBuffer m_mainCommandBuffer;
		VkCommandBuffer m_forwardCommandBuffer;

		VKRenderResources(const VKRenderResources &) = delete;
		VKRenderResources(const VKRenderResources &&) = delete;
		VKRenderResources &operator= (const VKRenderResources &) = delete;
		VKRenderResources &operator= (const VKRenderResources &&) = delete;
		~VKRenderResources();
		void init(unsigned int width, unsigned int height);
		void createFramebuffer(unsigned int width, unsigned int height, VkRenderPass renderPass);
		void createUniformBuffer(VkDeviceSize perFrameSize, VkDeviceSize perDrawSize);
		void createCommandBuffers();
		void resize(unsigned int width, unsigned int height);
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);

	private:
		explicit VKRenderResources();
		void createResizableTextures(unsigned int width, unsigned int height);
		void createAllTextures(unsigned int width, unsigned int height);
		void deleteResizableTextures();
		void deleteAllTextures();
	};
}