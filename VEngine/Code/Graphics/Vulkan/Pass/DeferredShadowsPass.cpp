#include "DeferredShadowsPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include <glm/mat4x4.hpp>
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/deferredShadows_bindings.h"
}

void VEngine::DeferredShadowsPass::addToGraph(RenderGraph &graph, const Data &data)
{
	for (size_t i = 0; i < data.m_lightDataCount; ++i)
	{
		ImageViewHandle resultImageViewHandle;

		ImageViewDescription viewDesc{};
		viewDesc.m_name = "Deferred Shadows Layer";
		viewDesc.m_imageHandle = data.m_resultImageHandle;
		viewDesc.m_subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, static_cast<uint32_t>(i), 1 };
		resultImageViewHandle = graph.createImageView(viewDesc);

		ResourceUsageDescription passUsages[]
		{
			{ResourceViewHandle(resultImageViewHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
			{ResourceViewHandle(data.m_depthImageViewHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
			{ResourceViewHandle(data.m_tangentSpaceImageViewHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
			{ResourceViewHandle(data.m_shadowImageViewHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		};

		graph.addPass("Deferred Shadows", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
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
				};

				ComputePipelineDesc pipelineDesc;
				pipelineDesc.setComputeShader("Resources/Shaders/deferredShadows_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
				pipelineDesc.finalize();

				auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

				VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

				// update descriptor sets
				{
					VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];

					VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

					writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(resultImageViewHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);
					writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_depthImageViewHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), DEPTH_IMAGE_BINDING);
					writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_tangentSpaceImageViewHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), TANGENT_SPACE_IMAGE_BINDING);
					writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_shadowImageViewHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER), SHADOW_IMAGE_BINDING);
					writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLER, { data.m_passRecordContext->m_renderResources->m_shadowSampler }, SHADOW_SAMPLER_BINDING);
					//writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLER, { data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX] }, POINT_SAMPLER_BINDING);
					writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING);
					writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_shadowBiasBufferInfo, SHADOW_BIAS_BUFFER_BINDING);

					writer.commit();
				}

				vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

				const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
				const auto &lightData = data.m_lightData[i];

				PushConsts pushConsts;
				pushConsts.invViewMatrix = data.m_passRecordContext->m_commonRenderData->m_invViewMatrix;
				pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
				pushConsts.direction = glm::vec3(lightData.m_direction);
				pushConsts.cascadeDataOffset = static_cast<int32_t>(lightData.m_shadowMatricesOffsetCount >> 16);
				pushConsts.cascadeCount = static_cast<int32_t>(lightData.m_shadowMatricesOffsetCount & 0xFFFF);

				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				VKUtility::dispatchComputeHelper(cmdBuf, width, height, 1, 8, 8, 1);
			});
	}
}
