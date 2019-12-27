#include "ClearBricksPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/clearBricks_bindings.h"
}

void VEngine::ClearBricksPass::addToGraph(RenderGraph &graph, const Data &data)
{
	const glm::vec3 camPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
	const glm::vec3 prevCamPos = data.m_passRecordContext->m_commonRenderData->m_prevInvViewMatrix[3];
	const glm::ivec3 cameraCoord = ivec3(round(camPos / RendererConsts::BRICK_SIZE));
	const glm::ivec3 prevCameraCoord = ivec3(round(prevCamPos / RendererConsts::BRICK_SIZE));
	const glm::ivec3 diff = cameraCoord - prevCameraCoord;

	if (diff == glm::ivec3(0))
	{
		return;
	}

	ResourceUsageDescription passUsages[]
	{
		 { ResourceViewHandle(data.m_brickPointerImageHandle), ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER },
		 { ResourceViewHandle(data.m_binVisBricksBufferHandle), ResourceState::WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
		 { ResourceViewHandle(data.m_colorBricksBufferHandle), ResourceState::WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
		 { ResourceViewHandle(data.m_freeBricksBufferHandle), ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
	};

	graph.addPass("Clear Bricks", QueueType::GRAPHICS, 1, passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// create pipeline description
			SpecEntry specEntries[]
			{
				SpecEntry(BRICK_VOLUME_WIDTH_CONST_ID, RendererConsts::BRICK_VOLUME_WIDTH),
				SpecEntry(BRICK_VOLUME_HEIGHT_CONST_ID, RendererConsts::BRICK_VOLUME_HEIGHT),
				SpecEntry(BRICK_VOLUME_DEPTH_CONST_ID, RendererConsts::BRICK_VOLUME_DEPTH),
			};

			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/clearBricks_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_brickPointerImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER), BRICK_PTR_IMAGE_BINDING);
				writer.writeBufferView(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, registry.getBufferView(data.m_binVisBricksBufferHandle), BIN_VIS_IMAGE_BUFFER_BINDING);
				writer.writeBufferView(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, registry.getBufferView(data.m_colorBricksBufferHandle), COLOR_IMAGE_BUFFER_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_freeBricksBufferHandle), FREE_BRICKS_BUFFER_BINDING);

				writer.commit();
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			PushConsts pushConsts;
			pushConsts.cameraCoord = ivec4(round(data.m_passRecordContext->m_commonRenderData->m_cameraPosition / RendererConsts::BRICK_SIZE));
			pushConsts.diff = pushConsts.cameraCoord - ivec4(round(data.m_passRecordContext->m_commonRenderData->m_prevInvViewMatrix[3] / RendererConsts::BRICK_SIZE));

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatch(cmdBuf, (RendererConsts::BRICK_VOLUME_WIDTH + 7) / 8, (RendererConsts::BRICK_VOLUME_HEIGHT + 7) / 8, RendererConsts::BRICK_VOLUME_DEPTH);

		}, true);
}
