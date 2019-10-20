#include "ClearVoxelsPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <iostream>

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/clearVoxels_bindings.h"
}

void VEngine::ClearVoxelsPass::addToGraph(RenderGraph &graph, const Data &data)
{
	glm::vec3 camPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
	glm::vec3 prevCamPos = data.m_passRecordContext->m_commonRenderData->m_prevInvViewMatrix[3];

	ivec3 cameraCoord = ivec3(round(camPos * 8.0f));
	ivec3 prevCameraCoord = ivec3(round(prevCamPos * 8.0f));

	// nothing to clear
	if (cameraCoord == prevCameraCoord)
	{
		return;
	}

	ivec3 diff = cameraCoord - prevCameraCoord;

	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_voxelImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
	};

	graph.addPass("Clear Voxels", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			VKComputePipelineDescription pipelineDesc;
			{
				strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/clearVoxels_comp.spv");

				pipelineDesc.finalize();
			}

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_pointSamplerClamp;

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_voxelImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), VOXEL_IMAGE_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);


			PushConsts pushConsts;
			pushConsts.camCoord = ivec4(cameraCoord, 0);
			pushConsts.diff = ivec4(diff, 0);

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatch(cmdBuf, (128 + 7) / 8, (64 + 7) / 8, 128);
		});
}
