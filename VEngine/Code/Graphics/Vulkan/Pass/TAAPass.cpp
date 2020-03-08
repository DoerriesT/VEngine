#include "TAAPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "GlobalVar.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <glm/vec3.hpp>

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/taa_bindings.h"
}

void VEngine::TAAPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_depthImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_velocityImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_taaHistoryImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_lightImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_taaResolveImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
	};

	graph.addPass("TAA Resolve", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/taa_comp.spv");
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
				VkSampler linearSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_taaResolveImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_depthImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), DEPTH_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_velocityImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), VELOCITY_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_taaHistoryImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, linearSamplerClamp), HISTORY_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_lightImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, linearSamplerClamp), SOURCE_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLER, { linearSamplerClamp }, LINEAR_SAMPLER_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLER, { pointSamplerClamp }, POINT_SAMPLER_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			const float jitterOffsetLength = glm::length(glm::vec2(data.m_jitterOffsetX * width, data.m_jitterOffsetY * height));

			PushConsts pushConsts;
			pushConsts.bicubicSharpness = g_TAABicubicSharpness;
			pushConsts.temporalContrastThreshold = g_TAATemporalContrastThreshold;
			pushConsts.lowStrengthAlpha = g_TAALowStrengthAlpha;
			pushConsts.highStrengthAlpha = g_TAAHighStrengthAlpha;
			pushConsts.antiFlickeringAlpha = g_TAAAntiFlickeringAlpha;
			pushConsts.jitterOffsetWeight = exp(-2.29f * jitterOffsetLength);

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			VKUtility::dispatchComputeHelper(cmdBuf, width, height, 1, 8, 8, 1);
		});
}
