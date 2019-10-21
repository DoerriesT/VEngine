#include "ClearVoxelsPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/clearVoxels_bindings.h"
}

void VEngine::ClearVoxelsPass::addToGraph(RenderGraph &graph, const Data &data)
{
	const glm::vec3 camPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
	const glm::vec3 prevCamPos = data.m_passRecordContext->m_commonRenderData->m_prevInvViewMatrix[3];

	// determine number of cascades to clear
	float curVoxelScale = 1.0f / RendererConsts::VOXEL_SCENE_BASE_SIZE;
	uint32_t usageCount = 0;
	ResourceUsageDescription passUsages[RendererConsts::VOXEL_SCENE_CASCADES];
	for (size_t i = 0; i < RendererConsts::VOXEL_SCENE_CASCADES; ++i)
	{
		const ivec3 cameraCoord = ivec3(round(camPos * curVoxelScale));
		const ivec3 prevCameraCoord = ivec3(round(prevCamPos * curVoxelScale));
		curVoxelScale *= 0.5f;

		if (cameraCoord != prevCameraCoord)
		{
			passUsages[usageCount++] = { ResourceViewHandle(data.m_voxelImageHandles[i]), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER };
		}
	};

	if (!usageCount)
	{
		return;
	}

	graph.addPass("Clear Voxels", QueueType::GRAPHICS, usageCount, passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			VKComputePipelineDescription pipelineDesc;
			{
				strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/clearVoxels_comp.spv");

				pipelineDesc.finalize();
			}

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			const glm::vec3 camPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
			const glm::vec3 prevCamPos = data.m_passRecordContext->m_commonRenderData->m_prevInvViewMatrix[3];
			float curVoxelScale = 1.0f / RendererConsts::VOXEL_SCENE_BASE_SIZE;
			for (size_t i = 0; i < RendererConsts::VOXEL_SCENE_CASCADES; ++i)
			{
				const ivec3 cameraCoord = ivec3(round(camPos * curVoxelScale));
				const ivec3 prevCameraCoord = ivec3(round(prevCamPos * curVoxelScale));
				curVoxelScale *= 0.5f;

				if (cameraCoord != prevCameraCoord)
				{
					VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

					// update descriptor sets
					{
						VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_pointSamplerClamp;

						VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

						writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_voxelImageHandles[i], ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), VOXEL_IMAGE_BINDING);

						writer.commit();
					}

					vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

					PushConsts pushConsts;
					pushConsts.camCoord = ivec4(cameraCoord, 0);
					pushConsts.diff = ivec4(cameraCoord - prevCameraCoord, 0);

					vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

					vkCmdDispatch(cmdBuf, (RendererConsts::VOXEL_SCENE_WIDTH + 7) / 8, (RendererConsts::VOXEL_SCENE_HEIGHT + 7) / 8, RendererConsts::VOXEL_SCENE_DEPTH);
				}
			}
		}, true);
}
