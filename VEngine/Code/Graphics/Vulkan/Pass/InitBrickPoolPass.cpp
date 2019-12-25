#include "InitBrickPoolPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

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
		 {ResourceViewHandle(data.m_binVisBricksBufferHandle), ResourceState::WRITE_BUFFER_TRANSFER},
		 {ResourceViewHandle(data.m_colorBricksBufferHandle), ResourceState::WRITE_BUFFER_TRANSFER},
	};

	graph.addPass("Init Brick Pool", QueueType::GRAPHICS, 1, passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// clear bricks
			vkCmdFillBuffer(cmdBuf, registry.getBuffer(data.m_binVisBricksBufferHandle), 0, registry.getBufferDescription(data.m_binVisBricksBufferHandle).m_size, 0);
			vkCmdFillBuffer(cmdBuf, registry.getBuffer(data.m_colorBricksBufferHandle), 0, registry.getBufferDescription(data.m_colorBricksBufferHandle).m_size, 0);

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

			constexpr uint32_t brickCount = 1024 * 32;

			PushConsts pushConsts;
			pushConsts.count  = brickCount;
			pushConsts.offset = 0;
			pushConsts.clearNextFree = true;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatch(cmdBuf, (brickCount + 255) / 256, 1, 1);

		}, true);
}
