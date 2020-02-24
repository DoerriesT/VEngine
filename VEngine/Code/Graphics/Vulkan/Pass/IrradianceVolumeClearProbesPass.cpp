#include "IrradianceVolumeClearProbesPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/Module/DiffuseGIProbesModule.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/clearVoxels_bindings.h"
}

void VEngine::IrradianceVolumeClearProbesPass::addToGraph(RenderGraph &graph, const Data &data)
{
	const glm::vec3 camPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
	const glm::vec3 prevCamPos = data.m_passRecordContext->m_commonRenderData->m_prevInvViewMatrix[3];

	// determine if a clear is necessary
	bool clearRequired = false;
	float curVoxelScale = 1.0f / DiffuseGIProbesModule::IRRADIANCE_VOLUME_BASE_SIZE;
	for (size_t i = 0; i < DiffuseGIProbesModule::IRRADIANCE_VOLUME_CASCADES; ++i)
	{
		const ivec3 cameraCoord = ivec3(round(camPos * curVoxelScale));
		const ivec3 prevCameraCoord = ivec3(round(prevCamPos * curVoxelScale));
		curVoxelScale *= 0.5f;

		clearRequired = clearRequired || (cameraCoord != prevCameraCoord);
	};

	if (!clearRequired)
	{
		return;
	}

	ResourceUsageDescription passUsages[]
	{
		 { ResourceViewHandle(data.m_irradianceVolumeAgeImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER }
	};

	graph.addPass("Clear Irradiance Volume", QueueType::GRAPHICS, 1, passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// create pipeline description
			SpecEntry specEntries[]
			{
				SpecEntry(VOXEL_GRID_WIDTH_CONST_ID, DiffuseGIProbesModule::IRRADIANCE_VOLUME_WIDTH),
				SpecEntry(VOXEL_GRID_HEIGHT_CONST_ID, DiffuseGIProbesModule::IRRADIANCE_VOLUME_HEIGHT),
				SpecEntry(VOXEL_GRID_DEPTH_CONST_ID, DiffuseGIProbesModule::IRRADIANCE_VOLUME_DEPTH),
				SpecEntry(VOXEL_BASE_SCALE_CONST_ID, 1.0f / DiffuseGIProbesModule::IRRADIANCE_VOLUME_BASE_SIZE),
				SpecEntry(VOXEL_CASCADES_CONST_ID, DiffuseGIProbesModule::IRRADIANCE_VOLUME_CASCADES),
			};

			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/clearVoxels_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_irradianceVolumeAgeImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), OPACITY_IMAGE_BINDING);

				writer.commit();
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			PushConsts pushConsts;
			pushConsts.cameraPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
			pushConsts.prevCameraPos = data.m_passRecordContext->m_commonRenderData->m_prevInvViewMatrix[3];

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatch(cmdBuf, (DiffuseGIProbesModule::IRRADIANCE_VOLUME_WIDTH + 7) / 8, (DiffuseGIProbesModule::IRRADIANCE_VOLUME_HEIGHT + 7) / 8, DiffuseGIProbesModule::IRRADIANCE_VOLUME_DEPTH);
			
		}, true);
}
