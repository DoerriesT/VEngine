#include "ClearBricksPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/Module/SparseVoxelBricksModule.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/clearBricks_bindings.h"
}

void VEngine::ClearBricksPass::addToGraph(RenderGraph &graph, const Data &data)
{
	const glm::vec3 camPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
	const glm::vec3 prevCamPos = data.m_passRecordContext->m_commonRenderData->m_prevInvViewMatrix[3];
	const glm::ivec3 cameraCoord = ivec3(round(camPos / SparseVoxelBricksModule::BRICK_SIZE));
	const glm::ivec3 prevCameraCoord = ivec3(round(prevCamPos / SparseVoxelBricksModule::BRICK_SIZE));
	const glm::ivec3 diff = cameraCoord - prevCameraCoord;

	if (diff == glm::ivec3(0))
	{
		return;
	}

	ResourceUsageDescription passUsages[]
	{
		 { ResourceViewHandle(data.m_brickPointerImageHandle), ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER },
		 { ResourceViewHandle(data.m_binVisBricksImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER },
		 { ResourceViewHandle(data.m_colorBricksImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER },
		 { ResourceViewHandle(data.m_freeBricksBufferHandle), ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
	};

	graph.addPass("Clear Bricks", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// create pipeline description
			SpecEntry specEntries[]
			{
				SpecEntry(BRICK_VOLUME_WIDTH_CONST_ID, SparseVoxelBricksModule::BRICK_VOLUME_WIDTH),
				SpecEntry(BRICK_VOLUME_HEIGHT_CONST_ID, SparseVoxelBricksModule::BRICK_VOLUME_HEIGHT),
				SpecEntry(BRICK_VOLUME_DEPTH_CONST_ID, SparseVoxelBricksModule::BRICK_VOLUME_DEPTH),
				SpecEntry(BIN_VIS_BRICK_SIZE_CONST_ID, SparseVoxelBricksModule::BINARY_VIS_BRICK_SIZE),
				SpecEntry(COLOR_BRICK_SIZE_CONST_ID, SparseVoxelBricksModule::COLOR_BRICK_SIZE),
			};

			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/clearBricks_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_brickPointerImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER), BRICK_PTR_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_binVisBricksImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), BIN_VIS_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_colorBricksImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), COLOR_IMAGE_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_freeBricksBufferHandle), FREE_BRICKS_BUFFER_BINDING);

				writer.commit();
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			PushConsts pushConsts;
			pushConsts.cameraCoord = ivec4(round(data.m_passRecordContext->m_commonRenderData->m_cameraPosition / SparseVoxelBricksModule::BRICK_SIZE));
			pushConsts.diff = pushConsts.cameraCoord - ivec4(round(data.m_passRecordContext->m_commonRenderData->m_prevInvViewMatrix[3] / SparseVoxelBricksModule::BRICK_SIZE));

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatch(cmdBuf, 
				(SparseVoxelBricksModule::BRICK_VOLUME_WIDTH + 3) / 4, 
				(SparseVoxelBricksModule::BRICK_VOLUME_HEIGHT + 3) / 4, 
				(SparseVoxelBricksModule::BRICK_VOLUME_DEPTH + 3) / 4);

		}, true);
}
