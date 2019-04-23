#include "VKLightingPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"

using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/lighting_bindings.h"

void VEngine::VKLightingPass::addToGraph(FrameGraph::Graph &graph, Data &data)
{
	graph.addComputePass("Lighting Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		// constant data buffer
		{
			FrameGraph::BufferDescription desc = {};
			desc.m_name = "Lighting Pass Constant Data Buffer";
			desc.m_concurrent = true;
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(ConstantData);
			desc.m_hostVisible = true;

			data.m_constantDataBufferHandle = graph.createBuffer(desc);
		}

		builder.readUniformBuffer(data.m_constantDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readStorageBuffer(data.m_shadowDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readStorageBuffer(data.m_directionalLightDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readStorageBuffer(data.m_pointLightDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readStorageBuffer(data.m_pointLightZBinsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readStorageBuffer(data.m_pointLightBitMaskBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_depthImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_albedoImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_normalImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_metalnessRougnessOcclusionImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_shadowAtlasImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		builder.writeStorageImage(data.m_resultImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/lighting_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[9] = {};

			// constant data
			VkDescriptorBufferInfo constantBufferInfo = registry.getBufferInfo(data.m_constantDataBufferHandle);
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = CONSTANT_DATA_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].pBufferInfo = &constantBufferInfo;

			// result image
			VkDescriptorImageInfo resultImageInfo = registry.getImageInfo(data.m_resultImageHandle);
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = RESULT_IMAGE_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptorWrites[1].pImageInfo = &resultImageInfo;

			// gbuffer images
			VkDescriptorImageInfo depthImageInfo = registry.getImageInfo(data.m_depthImageHandle);
			VkDescriptorImageInfo albedoImageInfo = registry.getImageInfo(data.m_albedoImageHandle);
			VkDescriptorImageInfo normalImageInfo = registry.getImageInfo(data.m_normalImageHandle);
			VkDescriptorImageInfo mroImageInfo = registry.getImageInfo(data.m_metalnessRougnessOcclusionImageHandle);

			VkDescriptorImageInfo gbufferImageInfo[] = { depthImageInfo, albedoImageInfo, normalImageInfo, mroImageInfo };

			for (auto &info : gbufferImageInfo)
			{
				info.sampler = data.m_renderResources->m_pointSamplerClamp;
			}

			descriptorWrites[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[2].dstSet = descriptorSet;
			descriptorWrites[2].dstBinding = GBUFFER_IMAGES_BINDING;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorCount = 4;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[2].pImageInfo = gbufferImageInfo;

			// shadow atlas
			VkDescriptorImageInfo shadowAtlasImageInfo = registry.getImageInfo(data.m_shadowAtlasImageHandle);
			shadowAtlasImageInfo.sampler = data.m_renderResources->m_shadowSampler;
			descriptorWrites[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[3].dstSet = descriptorSet;
			descriptorWrites[3].dstBinding = SHADOW_ATLAS_BINDING;
			descriptorWrites[3].dstArrayElement = 0;
			descriptorWrites[3].descriptorCount = 1;
			descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[3].pImageInfo = &shadowAtlasImageInfo;

			// directional light
			VkDescriptorBufferInfo directionalLightBufferInfo = registry.getBufferInfo(data.m_directionalLightDataBufferHandle);
			descriptorWrites[4] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[4].dstSet = descriptorSet;
			descriptorWrites[4].dstBinding = DIRECTIONAL_LIGHT_DATA_BINDING;
			descriptorWrites[4].dstArrayElement = 0;
			descriptorWrites[4].descriptorCount = 1;
			descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[4].pBufferInfo = &directionalLightBufferInfo;

			// point light
			VkDescriptorBufferInfo pointLightBufferInfo = registry.getBufferInfo(data.m_pointLightDataBufferHandle);
			descriptorWrites[5] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[5].dstSet = descriptorSet;
			descriptorWrites[5].dstBinding = POINT_LIGHT_DATA_BINDING;
			descriptorWrites[5].dstArrayElement = 0;
			descriptorWrites[5].descriptorCount = 1;
			descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[5].pBufferInfo = &pointLightBufferInfo;

			// shadow data
			VkDescriptorBufferInfo shadowDataBufferInfo = registry.getBufferInfo(data.m_shadowDataBufferHandle);
			descriptorWrites[6] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[6].dstSet = descriptorSet;
			descriptorWrites[6].dstBinding = SHADOW_DATA_BINDING;
			descriptorWrites[6].dstArrayElement = 0;
			descriptorWrites[6].descriptorCount = 1;
			descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[6].pBufferInfo = &shadowDataBufferInfo;

			// point light z bins
			VkDescriptorBufferInfo pointLightZBinsbufferInfo = registry.getBufferInfo(data.m_pointLightZBinsBufferHandle);
			descriptorWrites[7] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[7].dstSet = descriptorSet;
			descriptorWrites[7].dstBinding = POINT_LIGHT_Z_BINS_BINDING;
			descriptorWrites[7].dstArrayElement = 0;
			descriptorWrites[7].descriptorCount = 1;
			descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[7].pBufferInfo = &pointLightZBinsbufferInfo;

			// point light mask
			VkDescriptorBufferInfo pointLightMaskBufferInfo = registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle);
			descriptorWrites[8] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[8].dstSet = descriptorSet;
			descriptorWrites[8].dstBinding = POINT_LIGHT_MASK_BINDING;
			descriptorWrites[8].dstArrayElement = 0;
			descriptorWrites[8].descriptorCount = 1;
			descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[8].pBufferInfo = &pointLightMaskBufferInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
		}

		// write constant data
		{
			ConstantData *constantData = (ConstantData *)registry.mapMemory(data.m_constantDataBufferHandle);
			{
				constantData->viewMatrix = data.m_commonRenderData->m_viewMatrix;
				constantData->invViewMatrix = data.m_commonRenderData->m_invViewMatrix;
				constantData->invJitteredProjectionMatrix = data.m_commonRenderData->m_invJitteredProjectionMatrix;
				constantData->pointLightCount = data.m_commonRenderData->m_pointLightCount;
			}
			registry.unmapMemory(data.m_constantDataBufferHandle);
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		VKUtility::dispatchComputeHelper(cmdBuf, data.m_width, data.m_height, 1, 8, 8, 1);
	});
}
