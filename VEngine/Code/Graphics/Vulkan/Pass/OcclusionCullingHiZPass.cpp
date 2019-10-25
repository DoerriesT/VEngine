#include "OcclusionCullingHiZPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <glm/ext.hpp>

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/occlusionCullingHiZ_bindings.h"
}

void VEngine::OcclusionCullingHiZPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_depthPyramidImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_visibilityBufferHandle), ResourceState::WRITE_STORAGE_BUFFER_COMPUTE_SHADER},
	};

	graph.addPass("Occlusion Culling HiZ", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/occlusionCullingHiZ_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_aabbBufferInfo, AABB_DATA_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_visibilityBufferHandle), VISIBILITY_BUFFER_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_depthPyramidImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX]), HIZ_IMAGE_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		const auto &renderData = *data.m_passRecordContext->m_commonRenderData;
		PushConsts pushConsts;
		pushConsts.jitteredViewProjectionMatrix = renderData.m_prevJitteredViewProjectionMatrix;
		pushConsts.resolution = glm::vec2(renderData.m_width / 2, renderData.m_height / 2);
		pushConsts.instanceOffset = data.m_drawOffset;
		pushConsts.instanceCount = data.m_drawCount;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		vkCmdDispatch(cmdBuf, (data.m_drawCount + 63) / 64, 1, 1);

//		vkCmdFillBuffer(cmdBuf, registry.getBuffer(data.m_visibilityBufferHandle), 0, data.m_drawCount * 4, 1);
	});
}
