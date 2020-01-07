#include "IndirectDiffusePass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/Vulkan/RenderPassCache.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/DeferredObjectDeleter.h"
#include "Utility/Utility.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/indirectDiffuse_bindings.h"
}

void VEngine::IndirectDiffusePass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_resultImageHandle), ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_depthImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_albedoImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_normalImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_irradianceVolumeImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_irradianceVolumeDepthImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_occlusionImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
	};
	const uint32_t usageCount = data.m_ssao ? sizeof(passUsages) / sizeof(passUsages[0]) : sizeof(passUsages) / sizeof(passUsages[0]) - 1;

	graph.addPass("Indirect Diffuse", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			SpecEntry specEntries[]
			{
				SpecEntry(WIDTH_CONST_ID, width),
				SpecEntry(HEIGHT_CONST_ID, height),
				SpecEntry(TEXEL_WIDTH_CONST_ID, 1.0f / width),
				SpecEntry(TEXEL_HEIGHT_CONST_ID, 1.0f / height),
				SpecEntry(IRRADIANCE_VOLUME_WIDTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_WIDTH),
				SpecEntry(IRRADIANCE_VOLUME_HEIGHT_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_HEIGHT),
				SpecEntry(IRRADIANCE_VOLUME_DEPTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH),
				SpecEntry(IRRADIANCE_VOLUME_BASE_SCALE_CONST_ID, 1.0f / RendererConsts::IRRADIANCE_VOLUME_BASE_SIZE),
				SpecEntry(IRRADIANCE_VOLUME_CASCADES_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_CASCADES),
				SpecEntry(IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH),
				SpecEntry(IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH),
			};

			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader(data.m_ssao ? "Resources/Shaders/indirectDiffuse_SSAO_ENABLED_comp.spv" : "Resources/Shaders/indirectDiffuse_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
				VkSampler linearSamplerRepeat = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_resultImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER, pointSamplerClamp), RESULT_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_depthImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), DEPTH_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_albedoImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), ALBEDO_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_normalImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), NORMAL_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, linearSamplerRepeat), IRRADIANCE_VOLUME_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeDepthImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, linearSamplerRepeat), IRRADIANCE_VOLUME_DEPTH_IMAGE_BINDING);

				if (data.m_ssao)
				{
					writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_occlusionImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), OCCLUSION_IMAGE_BINDING);
				}

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			mat4 transposedInvViewMatrix = glm::transpose(data.m_passRecordContext->m_commonRenderData->m_invViewMatrix);

			PushConsts pushConsts;
			pushConsts.invJitteredProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
			pushConsts.invViewMatrixRow0 = transposedInvViewMatrix[0];
			pushConsts.invViewMatrixRow1 = transposedInvViewMatrix[1];
			pushConsts.invViewMatrixRow2 = transposedInvViewMatrix[2];

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			VKUtility::dispatchComputeHelper(cmdBuf, width, height, 1, 8, 8, 1);
		});
}
