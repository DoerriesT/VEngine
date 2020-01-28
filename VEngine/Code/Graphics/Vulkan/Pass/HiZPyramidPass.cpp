#include "HiZPyramidPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/hiZPyramid_bindings.h"
}

void VEngine::HiZPyramidPass::addToGraph(RenderGraph &graph, const Data &data, OutData &outData)
{
	uint32_t levels = 1;
	{
		uint32_t w = data.m_passRecordContext->m_commonRenderData->m_width;
		uint32_t h = data.m_passRecordContext->m_commonRenderData->m_height;
		while (w > 1 || h > 1)
		{
			++levels;
			w /= 2;
			h /= 2;
		}
	}

	uint32_t resultWidth = data.m_passRecordContext->m_commonRenderData->m_width;
	uint32_t resultHeight = data.m_passRecordContext->m_commonRenderData->m_height;

	if (!data.m_copyFirstLevel)
	{
		resultWidth = glm::max(resultWidth / 2u, 1u);
		resultHeight = glm::max(resultHeight / 2u, 1u);
		levels = glm::max(levels - 1u, 1u);
	}

	// create result image
	ImageDescription desc = {};
	desc.m_name = "Hi-Z Pyramid Image";
	desc.m_concurrent = false;
	desc.m_clear = false;
	desc.m_clearValue.m_imageClearValue = {};
	desc.m_width = resultWidth;
	desc.m_height = resultHeight;
	desc.m_layers = 1;
	desc.m_levels = levels;
	desc.m_samples = 1;
	desc.m_format = VK_FORMAT_R32_SFLOAT;
	desc.m_usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

	outData.m_resultImageHandle = graph.createImage(desc);

	ImageViewDescription viewDesc{};
	viewDesc.m_name = "Hi-Z Pyramid";
	viewDesc.m_imageHandle = outData.m_resultImageHandle;
	viewDesc.m_subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1 };
	outData.m_resultImageViewHandle = graph.createImageView(viewDesc);

	// create image views for each mip
	ImageViewHandle viewHandles[14];
	for (uint32_t i = 0; i < levels; ++i)
	{
		ImageViewDescription viewDesc{};
		viewDesc.m_name = "Hi-Z Pyramid Mip Level";
		viewDesc.m_imageHandle = outData.m_resultImageHandle;
		viewDesc.m_subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 };
		viewHandles[i] = graph.createImageView(viewDesc);
	}

	ResourceUsageDescription passUsages[15]; // 14 levels maximum + input image
	for (uint32_t i = 0; i < levels; ++i)
	{
		ResourceViewHandle resViewHandle = ResourceViewHandle(viewHandles[i]);

		// last level isnt read from in this pass
		if (i == (levels - 1))
		{
			passUsages[i] = { resViewHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER };
		}
		else
		{
			passUsages[i] = { resViewHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER, true, ResourceState::READ_TEXTURE_COMPUTE_SHADER };
		}
	}
	passUsages[levels] = { ResourceViewHandle(data.m_inputImageViewHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER };

	graph.addPass("Hi-Z Pyramid", QueueType::GRAPHICS, levels + 1, passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// create pipeline description
			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader(data.m_maxReduction ? "Resources/Shaders/hiZPyramid_MAX_comp.spv" : "Resources/Shaders/hiZPyramid_MIN_comp.spv");
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkSampler pointSampler = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
			uint32_t mipWidth = data.m_passRecordContext->m_commonRenderData->m_width;
			uint32_t mipHeight = data.m_passRecordContext->m_commonRenderData->m_height;

			for (uint32_t i = 0; i < levels; ++i)
			{
				// update descriptor set
				{
					VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

					VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);
					if (i == 0)
					{
						writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_inputImageViewHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSampler), INPUT_IMAGE_BINDING);
					}
					else
					{
						writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(viewHandles[i - 1], ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSampler), INPUT_IMAGE_BINDING);
					}
					writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(viewHandles[i], ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);
					writer.commit();

					vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);
				}

				PushConsts pushConsts;
				pushConsts.prevMipSize = glm::ivec2(mipWidth, mipHeight);
				if (i != 0 || !data.m_copyFirstLevel)
				{
					mipWidth = glm::max(mipWidth / 2u, 1u);
					mipHeight = glm::max(mipHeight / 2u, 1u);
				}
				pushConsts.width = mipWidth;
				pushConsts.height = mipHeight;
				pushConsts.copyOnly = (i == 0 && data.m_copyFirstLevel) ? 1 : 0;

				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				vkCmdDispatch(cmdBuf, (mipWidth + 15) / 16, (mipHeight + 15) / 16, 1);

				// dont insert a barrier after the last iteration: we dont know the dstStage/dstAccess, so let the RenderGraph figure it out
				if (i < (levels - 1))
				{
					VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
					imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarrier.image = registry.getImage(viewHandles[i]);
					imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 };

					vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
				}
			}
		}, data.m_forceExecution);
}
