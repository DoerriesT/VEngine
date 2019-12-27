#include "IrradianceVolumeRayMarching2Pass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/irradianceVolumeRayMarching2_bindings.h"
}

void VEngine::IrradianceVolumeRayMarching2Pass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_brickPtrImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ ResourceViewHandle(data.m_binVisBricksBufferHandle), ResourceState::READ_STORAGE_BUFFER_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_colorBricksBufferHandle), ResourceState::READ_STORAGE_BUFFER_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_rayMarchingResultImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_rayMarchingResultDistanceImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_queueBufferHandle), ResourceState::READ_STORAGE_BUFFER_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::READ_INDIRECT_BUFFER },
	};

	graph.addPass("Irradiance Volume Ray Marching 2", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			SpecEntry specEntries[]
			{
				SpecEntry(GRID_WIDTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_WIDTH),
				SpecEntry(GRID_HEIGHT_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_HEIGHT),
				SpecEntry(GRID_DEPTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH),
				SpecEntry(GRID_BASE_SCALE_CONST_ID, 1.0f / RendererConsts::IRRADIANCE_VOLUME_BASE_SIZE),
				SpecEntry(CASCADES_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_CASCADES),
				SpecEntry(BRICK_VOLUME_WIDTH_CONST_ID, RendererConsts::BRICK_VOLUME_WIDTH),
				SpecEntry(BRICK_VOLUME_HEIGHT_CONST_ID, RendererConsts::BRICK_VOLUME_HEIGHT),
				SpecEntry(BRICK_VOLUME_DEPTH_CONST_ID, RendererConsts::BRICK_VOLUME_DEPTH),
				SpecEntry(VOXEL_GRID_WIDTH_CONST_ID, RendererConsts::BRICK_VOLUME_WIDTH * RendererConsts::BINARY_VIS_BRICK_SIZE),
				SpecEntry(VOXEL_GRID_HEIGHT_CONST_ID, RendererConsts::BRICK_VOLUME_HEIGHT * RendererConsts::BINARY_VIS_BRICK_SIZE),
				SpecEntry(VOXEL_GRID_DEPTH_CONST_ID, RendererConsts::BRICK_VOLUME_DEPTH * RendererConsts::BINARY_VIS_BRICK_SIZE),
				SpecEntry(VOXEL_SCALE_CONST_ID, 1.0f / (RendererConsts::BRICK_SIZE / RendererConsts::BINARY_VIS_BRICK_SIZE)),
				SpecEntry(BIN_VIS_BRICK_SIZE_CONST_ID, RendererConsts::BINARY_VIS_BRICK_SIZE),
				SpecEntry(COLOR_BRICK_SIZE_CONST_ID, RendererConsts::COLOR_BRICK_SIZE),
			};

			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/irradianceVolumeRayMarching2_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeBufferView(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, registry.getBufferView(data.m_binVisBricksBufferHandle), BIN_VIS_IMAGE_BUFFER_BINDING);
				writer.writeBufferView(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, registry.getBufferView(data.m_colorBricksBufferHandle), COLOR_IMAGE_BUFFER_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_brickPtrImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_REPEAT_IDX]), BRICK_PTR_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_rayMarchingResultImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_rayMarchingResultDistanceImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER), DISTANCE_IMAGE_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_queueBufferHandle), QUEUE_BUFFER_BINDING);

				writer.commit();
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			const ivec3 gridSize = ivec3(RendererConsts::BRICK_VOLUME_WIDTH, RendererConsts::BRICK_VOLUME_HEIGHT, RendererConsts::BRICK_VOLUME_DEPTH);
			const float voxelScale = 1.0f / RendererConsts::BRICK_SIZE;

			PushConsts pushConsts;
			pushConsts.cameraPosition = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
			pushConsts.time = data.m_passRecordContext->m_commonRenderData->m_time;
			pushConsts.voxelGridOffset = (ivec3(round(vec3(data.m_passRecordContext->m_commonRenderData->m_cameraPosition) * voxelScale)) - (gridSize / 2)) * int32_t(RendererConsts::BINARY_VIS_BRICK_SIZE);

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatchIndirect(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), 0);
		}, true);
}
