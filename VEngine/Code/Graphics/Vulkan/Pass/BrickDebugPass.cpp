#include "BrickDebugPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/Module/SparseVoxelBricksModule.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/brickDebug_bindings.h"
}

void VEngine::BrickDebugPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_colorImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_brickPtrImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		 { ResourceViewHandle(data.m_binVisBricksBufferHandle), ResourceState::READ_STORAGE_BUFFER_COMPUTE_SHADER },
		 { ResourceViewHandle(data.m_colorBricksBufferHandle), ResourceState::READ_STORAGE_BUFFER_COMPUTE_SHADER },
	};

	graph.addPass("Brick Debug", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			SpecEntry specEntries[]
			{
				SpecEntry(BRICK_VOLUME_WIDTH_CONST_ID, SparseVoxelBricksModule::BRICK_VOLUME_WIDTH),
				SpecEntry(BRICK_VOLUME_HEIGHT_CONST_ID, SparseVoxelBricksModule::BRICK_VOLUME_HEIGHT),
				SpecEntry(BRICK_VOLUME_DEPTH_CONST_ID, SparseVoxelBricksModule::BRICK_VOLUME_DEPTH),
				SpecEntry(VOXEL_GRID_WIDTH_CONST_ID, SparseVoxelBricksModule::BRICK_VOLUME_WIDTH * SparseVoxelBricksModule::BINARY_VIS_BRICK_SIZE),
				SpecEntry(VOXEL_GRID_HEIGHT_CONST_ID, SparseVoxelBricksModule::BRICK_VOLUME_HEIGHT * SparseVoxelBricksModule::BINARY_VIS_BRICK_SIZE),
				SpecEntry(VOXEL_GRID_DEPTH_CONST_ID, SparseVoxelBricksModule::BRICK_VOLUME_DEPTH * SparseVoxelBricksModule::BINARY_VIS_BRICK_SIZE),
				SpecEntry(VOXEL_SCALE_CONST_ID, 1.0f / (SparseVoxelBricksModule::BRICK_SIZE / SparseVoxelBricksModule::BINARY_VIS_BRICK_SIZE)),
				SpecEntry(BIN_VIS_BRICK_SIZE_CONST_ID, SparseVoxelBricksModule::BINARY_VIS_BRICK_SIZE),
				SpecEntry(COLOR_BRICK_SIZE_CONST_ID, SparseVoxelBricksModule::COLOR_BRICK_SIZE),
				SpecEntry(WIDTH_CONST_ID, width),
				SpecEntry(HEIGHT_CONST_ID, height),
				SpecEntry(TEXEL_WIDTH_CONST_ID, 1.0f / width),
				SpecEntry(TEXEL_HEIGHT_CONST_ID, 1.0f / height),
			};

			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/brickDebug_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_colorImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);
				writer.writeBufferView(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, registry.getBufferView(data.m_binVisBricksBufferHandle), BIN_VIS_IMAGE_BUFFER_BINDING);
				writer.writeBufferView(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, registry.getBufferView(data.m_colorBricksBufferHandle), COLOR_IMAGE_BUFFER_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_brickPtrImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_REPEAT_IDX]), BRICK_PTR_IMAGE_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			const ivec3 gridSize = ivec3(SparseVoxelBricksModule::BRICK_VOLUME_WIDTH, SparseVoxelBricksModule::BRICK_VOLUME_HEIGHT, SparseVoxelBricksModule::BRICK_VOLUME_DEPTH);
			const float voxelScale = 1.0f / SparseVoxelBricksModule::BRICK_SIZE;

			PushConsts pushConsts;
			pushConsts.invViewProjection = data.m_passRecordContext->m_commonRenderData->m_invJitteredViewProjectionMatrix;
			pushConsts.gridOffset = (ivec3(round(vec3(data.m_passRecordContext->m_commonRenderData->m_cameraPosition) * voxelScale)) - (gridSize / 2)) * int32_t(SparseVoxelBricksModule::BINARY_VIS_BRICK_SIZE);

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			VKUtility::dispatchComputeHelper(cmdBuf, width, height, 1, 8, 8, 1);
		});
}
