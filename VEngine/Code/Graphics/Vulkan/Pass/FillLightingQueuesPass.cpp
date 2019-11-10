#include "FillLightingQueuesPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/fillLightingQueues_bindings.h"
}

void VEngine::FillLightingQueuesPass::addToGraph(RenderGraph &graph, const Data &data)
{
	const bool firstTime = data.m_passRecordContext->m_commonRenderData->m_frame == 0;
	ResourceUsageDescription passUsages[]
	{
		{ ResourceViewHandle(data.m_ageImageHandle), (firstTime ? ResourceState::WRITE_IMAGE_TRANSFER : ResourceState::READ_STORAGE_IMAGE_COMPUTE_SHADER), true, ResourceState::READ_STORAGE_IMAGE_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_queueBufferHandle), ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
	};

	graph.addPass("Fill Lighting Queues", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			if (data.m_passRecordContext->m_commonRenderData->m_frame == 0)
			{
				VkClearColorValue clearColor{};
				VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
				vkCmdClearColorImage(cmdBuf, registry.getImage(data.m_ageImageHandle), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

				VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
				imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.image = registry.getImage(data.m_ageImageHandle);
				imageBarrier.subresourceRange = range;

				vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
			}

			VKComputePipelineDescription pipelineDesc;
			{
				strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/fillLightingQueues_comp.spv");
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(GRID_WIDTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_WIDTH);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(GRID_HEIGHT_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_HEIGHT);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(GRID_DEPTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(GRID_BASE_SCALE_CONST_ID, 1.0f / RendererConsts::IRRADIANCE_VOLUME_BASE_SIZE);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(CASCADES_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_CASCADES);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(QUEUE_CAPACITY_CONST_ID, 4096);

				pipelineDesc.finalize();
			}

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_ageImageHandle, ResourceState::READ_STORAGE_IMAGE_COMPUTE_SHADER), AGE_IMAGE_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_queueBufferHandle), QUEUE_BUFFER_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_indirectBufferHandle), INDIRECT_CMD_BINDING);
				
				writer.commit();
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			PushConsts pushConsts;
			pushConsts.cameraPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
			pushConsts.time = data.m_passRecordContext->m_commonRenderData->m_time;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatch(cmdBuf, (RendererConsts::IRRADIANCE_VOLUME_WIDTH + 7) / 8, (RendererConsts::IRRADIANCE_VOLUME_HEIGHT + 7) / 8, RendererConsts::IRRADIANCE_VOLUME_DEPTH);
		}, true);
}
