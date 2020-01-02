#include "VoxelizationMarkPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/voxelizationMark_bindings.h"
}

void VEngine::VoxelizationMarkPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_markImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
	};

	graph.addPass("Voxelization Mark", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// create pipeline description
			SpecEntry specEntries[]
			{
				SpecEntry(BRICK_GRID_WIDTH_CONST_ID, RendererConsts::BRICK_VOLUME_WIDTH),
				SpecEntry(BRICK_GRID_HEIGHT_CONST_ID, RendererConsts::BRICK_VOLUME_HEIGHT),
				SpecEntry(BRICK_GRID_DEPTH_CONST_ID, RendererConsts::BRICK_VOLUME_DEPTH),
				SpecEntry(BRICK_SCALE_CONST_ID, 1.0f / RendererConsts::BRICK_SIZE),
			};
			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/voxelizationMark_comp.spv");
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				const auto &renderResources = *data.m_passRecordContext->m_renderResources;

				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { renderResources.m_indexBuffer.getBuffer(), 0, renderResources.m_indexBuffer.getSize() }, INDICES_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { renderResources.m_vertexBuffer.getBuffer(), 0, RendererConsts::MAX_VERTICES * sizeof(glm::vec3) }, POSITIONS_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_markImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), MARK_IMAGE_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			const glm::vec3 camPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
			float curVoxelScale = 1.0f / RendererConsts::BRICK_SIZE;

			for (uint32_t j = 0; j < data.m_instanceDataCount; ++j)
			{
				const auto &instanceData = data.m_instanceData[j];
				const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

				PushConsts pushConsts;
				pushConsts.indexOffset = subMeshInfo.m_firstIndex;
				pushConsts.indexCount = subMeshInfo.m_indexCount;
				pushConsts.vertexOffset = subMeshInfo.m_vertexOffset;
				pushConsts.transformIndex = instanceData.m_transformIndex;
				pushConsts.gridOffset = round(camPos * curVoxelScale) - glm::floor(glm::vec3(RendererConsts::BRICK_VOLUME_WIDTH, RendererConsts::BRICK_VOLUME_HEIGHT, RendererConsts::BRICK_VOLUME_DEPTH) / 2.0f);

				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				vkCmdDispatch(cmdBuf, ((pushConsts.indexCount / 3) + 255) / 256, 1, 1);
			}
		}, true);
}
