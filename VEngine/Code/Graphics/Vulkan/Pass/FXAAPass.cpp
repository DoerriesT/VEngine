#include "FXAAPass.h"
#include <glm/vec2.hpp>
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "GlobalVar.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using vec2 = glm::vec2;
#include "../../../../../Application/Resources/Shaders/fxaa_bindings.h"
}

void VEngine::FXAAPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_resultImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_inputImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
	};

	graph.addPass("FXAA", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineDesc pipelineDesc;
		pipelineDesc.setComputeShader("Resources/Shaders/fxaa_comp.spv");
		pipelineDesc.finalize();

		auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_inputImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX]), INPUT_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_resultImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		PushConsts pushConsts;
		pushConsts.invScreenSizeInPixels = 1.0f / glm::vec2(width, height);
		pushConsts.fxaaQualitySubpix = g_FXAAQualitySubpix;
		pushConsts.fxaaQualityEdgeThreshold = g_FXAAQualityEdgeThreshold;
		pushConsts.fxaaQualityEdgeThresholdMin = g_FXAAQualityEdgeThresholdMin;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		VKUtility::dispatchComputeHelper(cmdBuf, width, height, 1, 8, 8, 1);
	});
}
