#pragma once
#include "VKImageData.h"
#include "VKBufferData.h"
#include <vector>

#define MAX_UNIFORM_BUFFER_INSTANCE_COUNT (512)
#define TEXTURE_ARRAY_SIZE (512)

namespace VEngine
{
	class VKRenderer;
	struct VKTexture;

	struct VKRenderResources
	{
		friend VKRenderer;
	public:
		VKImageData m_depthAttachment;
		VKImageData m_albedoAttachment;
		VKImageData m_normalAttachment;
		VKImageData m_materialAttachment;
		VKImageData m_velocityAttachment;
		VKImageData m_lightAttachment;
		VKBufferData m_vertexBuffer;
		VKBufferData m_indexBuffer;
		VKBufferData m_mainUniformBuffer;
		VkDeviceSize m_perFrameDataSize;
		VkDeviceSize m_perDrawDataSize;
		VkFramebuffer m_mainFramebuffer;
		VkCommandBuffer m_mainCommandBuffer;
		VkCommandBuffer m_geometryCommandBuffer;
		VkCommandBuffer m_geometryAlphaMaskCommandBuffer;
		VkCommandBuffer m_lightingCommandBuffer;
		VkCommandBuffer m_forwardCommandBuffer;
		VkDescriptorPool m_descriptorPool;
		VkDescriptorSetLayout m_perFrameDataDescriptorSetLayout;
		VkDescriptorSetLayout m_perDrawDataDescriptorSetLayout;
		VkDescriptorSetLayout m_textureDescriptorSetLayout;
		VkDescriptorSetLayout m_lightingInputDescriptorSetLayout;
		VkDescriptorSet m_perFrameDataDescriptorSet;
		VkDescriptorSet m_perDrawDataDescriptorSet;
		VkDescriptorSet m_textureDescriptorSet;
		VkDescriptorSet m_lightingInputDescriptorSet;
		VkSampler m_dummySampler;
		VkImage m_dummyImage;
		VkImageView m_dummyImageView;
		VkDeviceMemory m_dummyMemory;

		VKRenderResources(const VKRenderResources &) = delete;
		VKRenderResources(const VKRenderResources &&) = delete;
		VKRenderResources &operator= (const VKRenderResources &) = delete;
		VKRenderResources &operator= (const VKRenderResources &&) = delete;
		~VKRenderResources();
		void init(unsigned int width, unsigned int height);
		void createFramebuffer(unsigned int width, unsigned int height, VkRenderPass renderPass);
		void createUniformBuffer(VkDeviceSize perFrameSize, VkDeviceSize perDrawSize);
		void createCommandBuffers();
		void createDummyTexture();
		void createDescriptors();
		void resize(unsigned int width, unsigned int height);
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);
		void updateTextureArray(const std::vector<VKTexture *> &textures);

	private:
		explicit VKRenderResources();
		void createResizableTextures(unsigned int width, unsigned int height);
		void createAllTextures(unsigned int width, unsigned int height);
		void deleteResizableTextures();
		void deleteAllTextures();
	};
}