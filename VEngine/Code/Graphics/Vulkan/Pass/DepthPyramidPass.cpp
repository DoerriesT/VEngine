#include "DepthPyramidPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/depthPyramid_bindings.h"
}

void VEngine::DepthPyramidPass::addToGraph(RenderGraph &graph, const Data &data)
{
	uint32_t levels = 0;
	{
		uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width / 2;
		uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height / 2;

		while (width || height)
		{
			width /= 2;
			height /= 2;
			++levels;
		}
	}

	ImageViewDescription viewDesc{};
	viewDesc.m_name = "Depth Pyramid Mip Level";
	viewDesc.m_imageHandle = data.m_resultImageHandle;

	ImageViewHandle viewHandles[14];

	ResourceUsageDescription passUsages[15]; // 14 levels maximum + input image
	for (uint32_t i = 0; i < levels; ++i)
	{
		viewDesc.m_subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 };
		viewHandles[i] = graph.createImageView(viewDesc);
		ResourceViewHandle resViewHandle = ResourceViewHandle(viewHandles[i]);

		// last level isnt read from in this pass
		if (i == (levels - 1))
		{
			passUsages[i] = { resViewHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER };
		}
		else
		{
			passUsages[i] = { resViewHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER, true, ResourceState::READ_STORAGE_IMAGE_COMPUTE_SHADER };
		}
	}
	passUsages[levels] = { ResourceViewHandle(data.m_inputImageViewHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER };

	graph.addPass("Depth Pyramid Downsample", QueueType::GRAPHICS, levels + 1, passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		// clear prev image in first frame
		//if (data.m_passRecordContext->m_commonRenderData->m_frame == 0)
		//{
		//	VkImage prevDepthImage = registry.getImage(data.m_inputImageViewHandle);
		//	VkImageSubresourceRange range{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
		//
		//	VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		//	imageBarrier.srcAccessMask = 0;
		//	imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		//	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		//	imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		//	imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		//	imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		//	imageBarrier.image = prevDepthImage;
		//	imageBarrier.subresourceRange = range;
		//
		//	vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
		//
		//	VkClearDepthStencilValue clearValue{ 0.0f, 0 };
		//	vkCmdClearDepthStencilImage(cmdBuf, prevDepthImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
		//
		//	imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		//	imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		//	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		//	imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		//
		//	vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
		//}

		// create pipeline description
		ComputePipelineDesc pipelineDesc;
		pipelineDesc.setComputeShader("Resources/Shaders/depthPyramid_comp.spv");
		pipelineDesc.finalize();

		auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

		VkSampler pointSampler = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
		glm::ivec2 prevMipSize = glm::ivec2(data.m_passRecordContext->m_commonRenderData->m_width, data.m_passRecordContext->m_commonRenderData->m_height);
		uint32_t mipWidth = glm::max(prevMipSize.x / 2, 1);
		uint32_t mipHeight = glm::max(prevMipSize.y / 2, 1);

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
					writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { pointSampler, registry.getImageView(viewHandles[i - 1]), VK_IMAGE_LAYOUT_GENERAL }, INPUT_IMAGE_BINDING);
				}
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(viewHandles[i], ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);
				writer.commit();

				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);
			}

			PushConsts pushConsts;
			pushConsts.prevMipSize = prevMipSize;
			pushConsts.width = mipWidth;
			pushConsts.height = mipHeight;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			VKUtility::dispatchComputeHelper(cmdBuf, mipWidth, mipHeight, 1, 16, 16, 1);

			prevMipSize = glm::ivec2(mipWidth, mipHeight);
			mipWidth = max(mipWidth / 2u, 1u);
			mipHeight = max(mipHeight / 2u, 1u);

			// dont insert a barrier after the last iteration: we dont know the dstStage/dstAccess, so let the RenderGraph figure it out
			if (i < (levels - 1))
			{
				//VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				//imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				//imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				//imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				//imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				//imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				//imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				//imageBarrier.image = registry.getImage(viewHandles[i]);
				//imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 };

				VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
				memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
			}
		}
	}, true);
}
