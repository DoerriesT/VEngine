#include "BuildIndexBufferPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/buildIndexBuffer_bindings.h"
}

void VEngine::BuildIndexBufferPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_indirectDrawCmdBufferHandle), ResourceState::WRITE_BUFFER_TRANSFER, true, ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_filteredIndicesBufferHandle), ResourceState::WRITE_STORAGE_BUFFER_COMPUTE_SHADER},
	};

	graph.addPass("Build Index Buffer", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// prime indirect buffer with data
			{
				VkDrawIndexedIndirectCommand cmd{};
				cmd.instanceCount = 1;
				vkCmdUpdateBuffer(cmdBuf, registry.getBuffer(data.m_indirectDrawCmdBufferHandle), 0, sizeof(cmd), &cmd);

				VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

				vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
			}

			// create pipeline description
			VKComputePipelineDescription pipelineDesc;
			{
				strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/buildIndexBuffer_comp.spv");
				pipelineDesc.finalize();
			}

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_clusterInfoBufferInfo, CLUSTER_INFO_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_inputIndicesBufferInfo, INPUT_INDICES_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_indirectDrawCmdBufferHandle), DRAW_COMMAND_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_filteredIndicesBufferHandle), FILTERED_INDICES_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_positionsBufferInfo, POSITIONS_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			uint32_t currentOffset = data.m_clusterOffset;
			uint32_t clusterCount = data.m_clusterCount;
			const uint32_t limit = g_context.m_properties.limits.maxComputeWorkGroupCount[0];

			const auto &renderData = *data.m_passRecordContext->m_commonRenderData;

			while (clusterCount)
			{
				PushConsts pushConsts;
				pushConsts.viewProjection = renderData.m_jitteredViewProjectionMatrix;
				pushConsts.resolution = glm::vec2(renderData.m_width, renderData.m_height);
				pushConsts.clusterOffset = currentOffset;
				pushConsts.cullBackface = 1;

				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				uint32_t dispatchedSize = glm::min(limit, clusterCount);

				vkCmdDispatch(cmdBuf, dispatchedSize, 1, 1);

				currentOffset += dispatchedSize;
				clusterCount -= dispatchedSize;
			}
		});
}
