#include "BloomUpsamplePass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/bloomUpsample_bindings.h"
}

void VEngine::BloomUpsamplePass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[BloomModule::BLOOM_MIP_COUNT * 2];
	for (uint32_t i = 0; i < BloomModule::BLOOM_MIP_COUNT; ++i)
	{
		passUsages[i] = { ResourceViewHandle(data.m_inputImageViewHandles[i]), ResourceState::READ_TEXTURE_COMPUTE_SHADER };
	}
	for (uint32_t i = 0; i < BloomModule::BLOOM_MIP_COUNT; ++i)
	{
		// first level isnt read from in this pass
		if (i == 0)
		{
			passUsages[i + BloomModule::BLOOM_MIP_COUNT] = { ResourceViewHandle(data.m_resultImageViewHandles[i]), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER };
		}
		else
		{
			passUsages[i + BloomModule::BLOOM_MIP_COUNT] = { ResourceViewHandle(data.m_resultImageViewHandles[i]), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER, true, ResourceState::READ_TEXTURE_COMPUTE_SHADER };
		}
	}


	graph.addPass("Bloom Upsample", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// create pipeline description
			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/bloomUpsample_comp.spv");
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkSampler linearSampler = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX];

			for (uint32_t i = 0; i < BloomModule::BLOOM_MIP_COUNT - 1; ++i)
			{
				uint32_t curViewIdx = BloomModule::BLOOM_MIP_COUNT - i - 2;

				// update descriptor set
				{
					VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

					VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);
					writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_resultImageViewHandles[curViewIdx], ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);
					if (i == 0)
					{
						writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_inputImageViewHandles[curViewIdx + 1], ResourceState::READ_TEXTURE_COMPUTE_SHADER, linearSampler), INPUT_IMAGE_BINDING);
					}
					else
					{
						writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_resultImageViewHandles[curViewIdx + 1], ResourceState::READ_TEXTURE_COMPUTE_SHADER, linearSampler), INPUT_IMAGE_BINDING);
					}
					
					writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_inputImageViewHandles[curViewIdx], ResourceState::READ_TEXTURE_COMPUTE_SHADER, linearSampler), PREV_RESULT_IMAGE_BINDING);
					writer.commit();

					vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);
				}

				uint32_t mipWidth = glm::max(data.m_passRecordContext->m_commonRenderData->m_width / 2u / (1 << curViewIdx), 1u);
				uint32_t mipHeight = glm::max(data.m_passRecordContext->m_commonRenderData->m_height / 2u / (1 << curViewIdx), 1u);

				PushConsts pushConsts;
				pushConsts.texelSize = 1.0f / glm::vec2(mipWidth, mipHeight);
				pushConsts.width = mipWidth;
				pushConsts.height = mipHeight;
				pushConsts.addPrevious = i != 0 ? 1 : 0;

				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				VKUtility::dispatchComputeHelper(cmdBuf, mipWidth, mipHeight, 1, 16, 16, 1);


				// dont insert a barrier after the last iteration: we dont know the dstStage/dstAccess, so let the RenderGraph figure it out
				if (i < (BloomModule::BLOOM_MIP_COUNT - 2))
				{
					VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
					imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarrier.image = registry.getImage(data.m_resultImageViewHandles[curViewIdx]);
					imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, curViewIdx, 1, 0, 1 };

					//VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
					//memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					//memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

					vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
				}
			}
		}, true);
}
