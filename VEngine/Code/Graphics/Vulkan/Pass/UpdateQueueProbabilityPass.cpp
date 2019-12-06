#include "UpdateQueueProbabilityPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/updateQueueProbability_bindings.h"
}


void VEngine::UpdateQueueProbabilityPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		 { ResourceViewHandle(data.m_queueBufferHandle), ResourceState::WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
		 { ResourceViewHandle(data.m_prevQueueBufferHandle), ResourceState::READ_STORAGE_BUFFER_COMPUTE_SHADER },
		 { ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
	};

	graph.addPass("Update Queue Probability", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			if (data.m_passRecordContext->m_commonRenderData->m_frame == 0)
			{
				struct QueueInfo
				{
					uint passedCount = 0;
					float p = 0.9f;
				};
			
				QueueInfo queueInfo{};
			
				vkCmdUpdateBuffer(cmdBuf, registry.getBuffer(data.m_prevQueueBufferHandle), 0, sizeof(queueInfo), &queueInfo);
			
				VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
			}

			SpecEntry specEntries[]
			{
				SpecEntry(QUEUE_CAPACITY_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_QUEUE_CAPACITY),
			};

			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/updateQueueProbability_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_queueBufferHandle), QUEUE_BUFFER_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_prevQueueBufferHandle), PREV_QUEUE_BUFFER_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_indirectBufferHandle), INDIRECT_CMD_BINDING);

				writer.commit();
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			vkCmdDispatch(cmdBuf, 1, 1, 1);
		}, true);
}
