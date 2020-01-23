#include "InitBrickPoolPass.h"
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
#include "../../../../../Application/Resources/Shaders/initBrickPool_bindings.h"
}

void VEngine::InitBrickPoolPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		 { ResourceViewHandle(data.m_freeBricksBufferHandle), ResourceState::WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
		 {ResourceViewHandle(data.m_binVisBricksImageHandle), ResourceState::WRITE_IMAGE_TRANSFER},
		 {ResourceViewHandle(data.m_colorBricksImageHandle), ResourceState::WRITE_IMAGE_TRANSFER},
	};

	graph.addPass("Init Brick Pool", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// clear bricks
			VkClearColorValue clearColor{};
			VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCmdClearColorImage(cmdBuf, registry.getImage(data.m_binVisBricksImageHandle), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
			vkCmdClearColorImage(cmdBuf, registry.getImage(data.m_colorBricksImageHandle), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

			// create pipeline description
			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/initBrickPool_comp.spv");
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_freeBricksBufferHandle), FREE_BRICKS_BUFFER_BINDING);

				writer.commit();
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			PushConsts pushConsts;
			pushConsts.volumeSize = glm::uvec4(SparseVoxelBricksModule::BRICK_CACHE_WIDTH, SparseVoxelBricksModule::BRICK_CACHE_HEIGHT, SparseVoxelBricksModule::BRICK_CACHE_DEPTH, 0);
			pushConsts.offset = glm::uvec4(0);
			pushConsts.clearNextFree = true;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatch(cmdBuf, 
				(SparseVoxelBricksModule::BRICK_CACHE_WIDTH + 3) / 4, 
				(SparseVoxelBricksModule::BRICK_CACHE_HEIGHT + 3) / 4,
				(SparseVoxelBricksModule::BRICK_CACHE_DEPTH + 3) / 4);

		}, true);
}
