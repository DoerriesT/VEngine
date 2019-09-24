#include "DeferredShadowsPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include <glm/mat4x4.hpp>
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/deferredShadows_bindings.h"
}

void VEngine::DeferredShadowsPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_resultImageViewHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_depthImageViewHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_shadowImageViewHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
	};

	graph.addPass("Deferred Shadows", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/deferredShadows_comp.spv");
			pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(DIRECTIONAL_LIGHT_COUNT_CONST_ID, glm::min(data.m_passRecordContext->m_commonRenderData->m_directionalLightCount, 4u));
			pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(WIDTH_CONST_ID, width);
			pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(HEIGHT_CONST_ID, height);
			pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(TEXEL_WIDTH_CONST_ID, 1.0f / width);
			pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(TEXEL_HEIGHT_CONST_ID, 1.0f / height);

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_resultImageViewHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_depthImageViewHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, data.m_passRecordContext->m_renderResources->m_pointSamplerClamp), DEPTH_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_shadowImageViewHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER), SHADOW_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLER, { data.m_passRecordContext->m_renderResources->m_shadowSampler }, SHADOW_SAMPLER_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLER, { data.m_passRecordContext->m_renderResources->m_pointSamplerClamp }, POINT_SAMPLER_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_lightDataBufferInfo, DIRECTIONAL_LIGHT_DATA_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		PushConsts pushConsts;
		pushConsts.invViewProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredViewProjectionMatrix;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		VKUtility::dispatchComputeHelper(cmdBuf, width, height, 1, 8, 8, 1);
	});
}
