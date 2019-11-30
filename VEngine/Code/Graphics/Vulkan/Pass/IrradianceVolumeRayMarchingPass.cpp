#include "IrradianceVolumeRayMarchingPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/irradianceVolumeRayMarching_bindings.h"
}

void VEngine::IrradianceVolumeRayMarchingPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ ResourceViewHandle(data.m_voxelSceneOpacityImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_voxelSceneImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_rayMarchingResultImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_queueBufferHandle), ResourceState::READ_STORAGE_BUFFER_COMPUTE_SHADER },
		{ ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::READ_INDIRECT_BUFFER },
	};

	graph.addPass("Irradiance Volume Ray Marching", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			VKComputePipelineDescription pipelineDesc;
			{
				strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/irradianceVolumeRayMarching_comp.spv");
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(GRID_WIDTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_WIDTH);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(GRID_HEIGHT_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_HEIGHT);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(GRID_DEPTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(GRID_BASE_SCALE_CONST_ID, 1.0f / RendererConsts::IRRADIANCE_VOLUME_BASE_SIZE);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(CASCADES_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_CASCADES);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(VOXEL_GRID_WIDTH_CONST_ID, RendererConsts::VOXEL_SCENE_WIDTH);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(VOXEL_GRID_HEIGHT_CONST_ID, RendererConsts::VOXEL_SCENE_HEIGHT);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(VOXEL_GRID_DEPTH_CONST_ID, RendererConsts::VOXEL_SCENE_DEPTH);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(VOXEL_BASE_SCALE_CONST_ID, 1.0f / RendererConsts::VOXEL_SCENE_BASE_SIZE);
				pipelineDesc.m_computeShaderStage.m_specializationInfo.addEntry(VOXEL_CASCADES_CONST_ID, RendererConsts::VOXEL_SCENE_CASCADES);

				pipelineDesc.finalize();
			}

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_voxelSceneOpacityImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_REPEAT_IDX]), OPACITY_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_voxelSceneImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_REPEAT_IDX]), VOXEL_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_rayMarchingResultImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_queueBufferHandle), QUEUE_BUFFER_BINDING);

				writer.commit();
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			PushConsts pushConsts;
			pushConsts.cameraPosition = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
			pushConsts.time = data.m_passRecordContext->m_commonRenderData->m_time;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatchIndirect(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), 0);
		}, true);
}
