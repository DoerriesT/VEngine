#include "VKLightingPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"

namespace
{
	using mat3x4 = glm::mat3x4;
	using vec4 = glm::vec4;
	using mat4 = glm::mat4;
	using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/lighting_bindings.h"
}

void VEngine::VKLightingPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addComputePass("Lighting Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readStorageBuffer(data.m_pointLightBitMaskBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_depthImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_albedoImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_normalImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_metalnessRougnessOcclusionImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_shadowAtlasImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		if (data.m_ssao)
		{
			builder.readTexture(data.m_occlusionImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}

		builder.writeStorageImage(data.m_resultImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, data.m_ssao ? "Resources/Shaders/lighting_comp_SSAO_ENABLED.spv" : "Resources/Shaders/lighting_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[9] = {};
			uint32_t writeCount = 0;

			// result image
			VkDescriptorImageInfo resultImageInfo = registry.getImageInfo(data.m_resultImageHandle);
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = RESULT_IMAGE_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptorWrites[writeCount].pImageInfo = &resultImageInfo;
			++writeCount;

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

			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = GBUFFER_IMAGES_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 4;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[writeCount].pImageInfo = gbufferImageInfo;
			++writeCount;

			// shadow atlas
			VkDescriptorImageInfo shadowAtlasImageInfo = registry.getImageInfo(data.m_shadowAtlasImageHandle);
			shadowAtlasImageInfo.sampler = data.m_renderResources->m_shadowSampler;
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = SHADOW_ATLAS_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[writeCount].pImageInfo = &shadowAtlasImageInfo;
			++writeCount;

			// directional light
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = DIRECTIONAL_LIGHT_DATA_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &data.m_directionalLightDataBufferInfo;
			++writeCount;

			// point light
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = POINT_LIGHT_DATA_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &data.m_pointLightDataBufferInfo;
			++writeCount;

			// shadow data
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = SHADOW_DATA_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &data.m_shadowDataBufferInfo;
			++writeCount;

			// point light z bins
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = POINT_LIGHT_Z_BINS_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &data.m_pointLightZBinsBufferInfo;
			++writeCount;

			// point light mask
			VkDescriptorBufferInfo pointLightMaskBufferInfo = registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle);
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = POINT_LIGHT_MASK_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &pointLightMaskBufferInfo;
			++writeCount;

			// occlusion image
			VkDescriptorImageInfo occlusionImageInfo = registry.getImageInfo(data.m_occlusionImageHandle);
			occlusionImageInfo.sampler = data.m_renderResources->m_pointSamplerClamp;
			if (data.m_ssao)
			{
				descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				descriptorWrites[writeCount].dstSet = descriptorSet;
				descriptorWrites[writeCount].dstBinding = OCCLUSION_IMAGE_BINDING;
				descriptorWrites[writeCount].dstArrayElement = 0;
				descriptorWrites[writeCount].descriptorCount = 1;
				descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[writeCount].pImageInfo = &occlusionImageInfo;
				++writeCount;
			}

			vkUpdateDescriptorSets(g_context.m_device, writeCount, descriptorWrites, 0, nullptr);
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		const glm::mat4 rowMajorInvViewMatrix = glm::transpose(data.m_commonRenderData->m_invViewMatrix);

		PushConsts pushConsts;
		pushConsts.invJitteredProjectionMatrix = data.m_commonRenderData->m_invJitteredProjectionMatrix;
		pushConsts.invViewMatrixRow0 = rowMajorInvViewMatrix[0];
		pushConsts.invViewMatrixRow1 = rowMajorInvViewMatrix[1];
		pushConsts.invViewMatrixRow2 = rowMajorInvViewMatrix[2];
		pushConsts.pointLightCount = data.m_commonRenderData->m_pointLightCount;
		pushConsts.directionalLightCount = data.m_commonRenderData->m_directionalLightCount;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		VKUtility::dispatchComputeHelper(cmdBuf, data.m_width, data.m_height, 1, 8, 8, 1);
	});
}
