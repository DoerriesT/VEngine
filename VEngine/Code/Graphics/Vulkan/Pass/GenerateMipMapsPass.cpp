#include "GenerateMipMapsPass.h"

void VEngine::GenerateMipMapsPass::addToGraph(RenderGraph &graph, const Data &data)
{
	assert(data.m_mipCount < 14);
	ImageViewHandle viewHandles[14];
	ResourceUsageDescription passUsages[14];

	for (size_t i = 0; i < data.m_mipCount; ++i)
	{
		ImageViewDescription viewDesc{};
		viewDesc.m_name = "GenerateMipMaps Mip Level";
		viewDesc.m_imageHandle = data.m_imageHandle;
		viewDesc.m_subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(i), 1, 0, 1 };

		viewHandles[i] = graph.createImageView(viewDesc);

		if (i == 0)
		{
			passUsages[i] = { ResourceViewHandle(viewHandles[i]), ResourceState::READ_IMAGE_TRANSFER };
		}
		else if (i == data.m_mipCount - 1)
		{
			passUsages[i] = { ResourceViewHandle(viewHandles[i]), ResourceState::WRITE_IMAGE_TRANSFER };
		}
		else
		{
			passUsages[i] = { ResourceViewHandle(viewHandles[i]), ResourceState::WRITE_IMAGE_TRANSFER, true, ResourceState::READ_IMAGE_TRANSFER };
		}
	}

	graph.addPass("Generate Mip Maps", QueueType::GRAPHICS, data.m_mipCount, passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			VkImage image = registry.getImage(data.m_imageHandle);
			int32_t mipWidth = static_cast<int32_t>(data.m_width);
			int32_t mipHeight = static_cast<int32_t>(data.m_height);

			for (uint32_t i = 1; i < data.m_mipCount; ++i)
			{
				VkImageBlit blitRegion;
				blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 };
				blitRegion.srcOffsets[0] = { 0, 0, 0 };
				blitRegion.srcOffsets[1] = { mipWidth, mipHeight, 1 };

				mipWidth = mipWidth >= 2 ? mipWidth / 2 : 1;
				mipHeight = mipHeight >= 2 ? mipHeight / 2 : 1;

				blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };
				blitRegion.dstOffsets[0] = { 0, 0, 0 };
				blitRegion.dstOffsets[1] = { mipWidth, mipHeight, 1 };

				vkCmdBlitImage(cmdBuf, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_LINEAR);

				// dont insert a barrier after the last iteration: we dont know the dstStage/dstAccess, so let the RenderGraph figure it out
				if (i < (data.m_mipCount - 1))
				{
					VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarrier.image = image;
					imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 };

					vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
				}
			}
		}, true);
}
