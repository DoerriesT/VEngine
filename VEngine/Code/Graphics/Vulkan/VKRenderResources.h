#pragma once
#include "VKImageData.h"
#include "VKBufferData.h"
#include <vector>

#define MAX_UNIFORM_BUFFER_INSTANCE_COUNT (512)
#define TEXTURE_ARRAY_SIZE (512)
#define MAX_DIRECTIONAL_LIGHTS (1)
#define MAX_POINT_LIGHTS (1024)
#define MAX_SPOT_LIGHTS (1024)
#define MAX_SHADOW_DATA (512)

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
		VKImageData m_shadowTexture;
		VKBufferData m_vertexBuffer;
		VKBufferData m_indexBuffer;
		VKBufferData m_mainUniformBuffer;
		VKBufferData m_lightDataStorageBuffer;
		VKBufferData m_lightIndexStorageBuffer;
		VkDeviceSize m_perFrameDataSize;
		VkDeviceSize m_perDrawDataSize;
		VkDeviceSize m_directionalLightDataSize;
		VkDeviceSize m_pointLightDataSize;
		VkDeviceSize m_spotLightDataSize;
		VkDeviceSize m_shadowDataSize;
		VkFramebuffer m_mainFramebuffer;
		VkFramebuffer m_shadowFramebuffer;
		VkCommandBuffer m_mainCommandBuffer;
		VkDescriptorPool m_descriptorPool;
		VkDescriptorSetLayout m_perFrameDataDescriptorSetLayout;
		VkDescriptorSetLayout m_perDrawDataDescriptorSetLayout;
		VkDescriptorSetLayout m_textureDescriptorSetLayout;
		VkDescriptorSetLayout m_lightingInputDescriptorSetLayout;
		VkDescriptorSetLayout m_lightDataDescriptorSetLayout;
		VkDescriptorSetLayout m_cullDataDescriptorSetLayout;
		VkDescriptorSetLayout m_lightIndexDescriptorSetLayout;
		VkDescriptorSet m_perFrameDataDescriptorSet;
		VkDescriptorSet m_perDrawDataDescriptorSet;
		VkDescriptorSet m_textureDescriptorSet;
		VkDescriptorSet m_lightingInputDescriptorSet;
		VkDescriptorSet m_lightDataDescriptorSet;
		VkDescriptorSet m_cullDataDescriptorSet;
		VkDescriptorSet m_lightIndexDescriptorSet;
		VkSampler m_shadowSampler;
		VkSampler m_dummySampler;
		VkImage m_dummyImage;
		VkImageView m_dummyImageView;
		VkDeviceMemory m_dummyMemory;
		VkEvent m_shadowsFinishedEvent;

		VKRenderResources(const VKRenderResources &) = delete;
		VKRenderResources(const VKRenderResources &&) = delete;
		VKRenderResources &operator= (const VKRenderResources &) = delete;
		VKRenderResources &operator= (const VKRenderResources &&) = delete;
		~VKRenderResources();
		void init(unsigned int width, unsigned int height);
		void createFramebuffer(unsigned int width, unsigned int height, VkRenderPass mainRenderPass, VkRenderPass shadowRenderPass);
		void createUniformBuffer(VkDeviceSize perFrameSize, VkDeviceSize perDrawSize);
		void createStorageBuffers();
		void createCommandBuffers();
		void createDummyTexture();
		void createDescriptors();
		void createEvents();
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