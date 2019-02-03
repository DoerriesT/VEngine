#pragma once
#include "VKImage.h"
#include "VKBuffer.h"
#include <vector>

#define MAX_UNIFORM_BUFFER_INSTANCE_COUNT (512)
#define TEXTURE_ARRAY_SIZE (512)
#define MAX_DIRECTIONAL_LIGHTS (1)
#define MAX_POINT_LIGHTS (65536)
#define MAX_SPOT_LIGHTS (65536)
#define MAX_SHADOW_DATA (512)
#define Z_BINS (8192)

namespace VEngine
{
	class VKRenderer;

	struct VKRenderResources
	{
		friend VKRenderer;
	public:
		// images
		VKImage m_depthAttachment;
		VKImage m_albedoAttachment;
		VKImage m_normalAttachment;
		VKImage m_materialAttachment;
		VKImage m_velocityAttachment;
		VKImage m_lightAttachment;
		VKImage m_shadowTexture;

		// views
		VkImageView m_depthAttachmentView;
		VkImageView m_albedoAttachmentView;
		VkImageView m_normalAttachmentView;
		VkImageView m_materialAttachmentView;
		VkImageView m_velocityAttachmentView;
		VkImageView m_lightAttachmentView;
		VkImageView m_shadowTextureView;

		// buffers
		VKBuffer m_vertexBuffer;
		VKBuffer m_indexBuffer;
		VKBuffer m_perFrameDataUniformBuffer;
		VKBuffer m_perDrawDataUniformBuffer;
		VKBuffer m_directionalLightDataStorageBuffer;
		VKBuffer m_pointLightDataStorageBuffer;
		VKBuffer m_spotLightDataStorageBuffer;
		VKBuffer m_shadowDataStorageBuffer;
		VKBuffer m_zBinStorageBuffer;
		VKBuffer m_lightCullDataStorageBuffer;
		VKBuffer m_lightIndexStorageBuffer;

		// fbos
		VkFramebuffer m_geometryFillFramebuffer;
		VkFramebuffer m_shadowFramebuffer;
		VkFramebuffer m_forwardFramebuffer;

		// cmdbufs
		VkCommandBuffer m_tilingCommandBuffer;
		VkCommandBuffer m_geometryFillCommandBuffer;
		VkCommandBuffer m_shadowsCommandBuffer;
		VkCommandBuffer m_lightingCommandBuffer;
		VkCommandBuffer m_forwardCommandBuffer;

		// descriptors
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

		// samplers
		VkSampler m_shadowSampler;
		VkSampler m_linearSampler;
		VkSampler m_pointSampler;
		
		// sync
		VkEvent m_shadowsFinishedEvent;

		VKRenderResources(const VKRenderResources &) = delete;
		VKRenderResources(const VKRenderResources &&) = delete;
		VKRenderResources &operator= (const VKRenderResources &) = delete;
		VKRenderResources &operator= (const VKRenderResources &&) = delete;
		~VKRenderResources();
		void init(unsigned int width, unsigned int height);
		void createFramebuffer(unsigned int width, unsigned int height, VkRenderPass geometryFillRenderPass, VkRenderPass shadowRenderPass, VkRenderPass forwardRenderPass);
		void createUniformBuffer(VkDeviceSize perFrameSize, VkDeviceSize perDrawSize);
		void createStorageBuffers();
		void createCommandBuffers();
		void createDescriptors();
		void createEvents();
		void resize(unsigned int width, unsigned int height);
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);
		void updateTextureArray(const VkDescriptorImageInfo *data, size_t count);

	private:
		explicit VKRenderResources();
		void createResizableTextures(unsigned int width, unsigned int height);
		void createAllTextures(unsigned int width, unsigned int height);
		void deleteResizableTextures();
		void deleteAllTextures();
	};
}