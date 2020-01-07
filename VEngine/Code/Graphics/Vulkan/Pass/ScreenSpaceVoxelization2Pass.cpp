#include "ScreenSpaceVoxelization2Pass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/screenSpaceVoxelization2_bindings.h"
}

void VEngine::ScreenSpaceVoxelization2Pass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_brickPointerImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_colorBricksBufferHandle), ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_pointLightBitMaskBufferHandle), ResourceState::READ_STORAGE_BUFFER_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_depthImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_uvImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_ddxyLengthImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_ddxyRotMaterialIdImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_tangentSpaceImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_deferredShadowImageViewHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ ResourceViewHandle(data.m_irradianceVolumeImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_irradianceVolumeDepthImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER },
	};

	graph.addPass("Screen Space Voxelization 2", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			SpecEntry specEntries[]
			{
				SpecEntry(DIRECTIONAL_LIGHT_COUNT_CONST_ID, data.m_passRecordContext->m_commonRenderData->m_directionalLightCount),
				SpecEntry(BRICK_VOLUME_WIDTH_CONST_ID, RendererConsts::BRICK_VOLUME_WIDTH),
				SpecEntry(BRICK_VOLUME_HEIGHT_CONST_ID, RendererConsts::BRICK_VOLUME_HEIGHT),
				SpecEntry(BRICK_VOLUME_DEPTH_CONST_ID, RendererConsts::BRICK_VOLUME_DEPTH),
				SpecEntry(INV_VOXEL_BRICK_SIZE_CONST_ID, 1.0f / RendererConsts::BINARY_VIS_BRICK_SIZE),
				SpecEntry(VOXEL_SCALE_CONST_ID, 1.0f / (RendererConsts::BRICK_SIZE / float(RendererConsts::BINARY_VIS_BRICK_SIZE))),
				SpecEntry(BIN_VIS_BRICK_SIZE_CONST_ID, RendererConsts::BINARY_VIS_BRICK_SIZE),
				SpecEntry(COLOR_BRICK_SIZE_CONST_ID, RendererConsts::COLOR_BRICK_SIZE),
				SpecEntry(IRRADIANCE_VOLUME_WIDTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_WIDTH),
				SpecEntry(IRRADIANCE_VOLUME_HEIGHT_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_HEIGHT),
				SpecEntry(IRRADIANCE_VOLUME_DEPTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH),
				SpecEntry(IRRADIANCE_VOLUME_BASE_SCALE_CONST_ID, 1.0f / RendererConsts::IRRADIANCE_VOLUME_BASE_SIZE),
				SpecEntry(IRRADIANCE_VOLUME_CASCADES_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_CASCADES),
				SpecEntry(IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH),
				SpecEntry(IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH),
			};

			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/screenSpaceVoxelization2_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
				VkSampler linearSamplerRepeat = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_brickPointerImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), BRICK_PTR_IMAGE_BINDING);
				writer.writeBufferView(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, registry.getBufferView(data.m_colorBricksBufferHandle), COLOR_IMAGE_BUFFER_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_depthImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), DEPTH_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_uvImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), UV_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_ddxyLengthImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), DDXY_LENGTH_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_ddxyRotMaterialIdImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), DDXY_ROT_MATERIAL_ID_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_tangentSpaceImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), TANGENT_SPACE_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_deferredShadowImageViewHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), DEFERRED_SHADOW_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, linearSamplerRepeat), IRRADIANCE_VOLUME_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeDepthImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, linearSamplerRepeat), IRRADIANCE_VOLUME_DEPTH_IMAGE_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_directionalLightDataBufferInfo, DIRECTIONAL_LIGHT_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_pointLightDataBufferInfo, POINT_LIGHT_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_pointLightZBinsBufferInfo, POINT_LIGHT_Z_BINS_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle), POINT_LIGHT_MASK_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet };
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 2, descriptorSets, 0, nullptr);

			const glm::mat4 rowMajorInvViewMatrix = glm::transpose(data.m_passRecordContext->m_commonRenderData->m_invViewMatrix);

			PushConsts pushConsts;
			pushConsts.invJitteredProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
			pushConsts.invViewMatrixRow0 = rowMajorInvViewMatrix[0];
			pushConsts.invViewMatrixRow1 = rowMajorInvViewMatrix[1];
			pushConsts.invViewMatrixRow2 = rowMajorInvViewMatrix[2];
			pushConsts.pointLightCount = data.m_passRecordContext->m_commonRenderData->m_pointLightCount;
			pushConsts.time = data.m_passRecordContext->m_commonRenderData->m_time;
			pushConsts.deltaTime = data.m_passRecordContext->m_commonRenderData->m_timeDelta;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			const uint32_t pixelCount = 1024 * 32;

			vkCmdDispatch(cmdBuf, (pixelCount + 63) / 64, 1, 1);
		}, true);
}
