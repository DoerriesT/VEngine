#pragma once
#include "VKImageData.h"
#include "VKBufferData.h"

namespace VEngine
{
	class VKRenderer;

	struct VKRenderResources
	{
		friend VKRenderer;
	public:
		VKImageData m_colorAttachment;
		VKImageData m_depthAttachment;
		VKBufferData m_meshBuffer;
		VKBufferData m_mainUniformBuffer;
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
		void createUniformBuffer(VkDeviceSize size);
		void createCommandBuffers();
		void resize(unsigned int width, unsigned int height);
		void reserveMeshBuffer(uint64_t size);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);

	private:
		explicit VKRenderResources();
		void createResizableTextures(unsigned int width, unsigned int height);
		void createAllTextures(unsigned int width, unsigned int height);
		void deleteResizableTextures();
		void deleteAllTextures();
	};
}