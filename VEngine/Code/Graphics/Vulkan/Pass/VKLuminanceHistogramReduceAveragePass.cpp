#include "VKLuminanceHistogramReduceAveragePass.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <glm/vec3.hpp>

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/luminanceHistogramReduceAverage_bindings.h"
}

void VEngine::VKLuminanceHistogramAveragePass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_luminanceHistogramBufferHandle), ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_avgLuminanceBufferHandle), ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER},
	};

	graph.addPass("Luminance Histogram Average", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineDesc pipelineDesc;
		pipelineDesc.setComputeShader("Resources/Shaders/luminanceHistogramReduceAverage_comp.spv");
		pipelineDesc.finalize();

		auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// histogram
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_luminanceHistogramBufferHandle), LUMINANCE_HISTOGRAM_BINDING);

			// avg luminance values
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_avgLuminanceBufferHandle), LUMINANCE_VALUES_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		const float lowerBoundPercentage = 0.5f;
		const float upperBoundPercentage = 0.9f;

		PushConsts pushConsts;
		pushConsts.precomputedTerm = 1.0f - exp(-data.m_passRecordContext->m_commonRenderData->m_timeDelta * 1.1f);
		pushConsts.invScale = data.m_logMax - data.m_logMin;
		pushConsts.bias = -data.m_logMin * (1.0f / pushConsts.invScale);
		pushConsts.lowerBound = static_cast<uint32_t>(width * height * lowerBoundPercentage);
		pushConsts.upperBound = static_cast<uint32_t>(width * height * upperBoundPercentage);
		pushConsts.currentResourceIndex = data.m_passRecordContext->m_commonRenderData->m_curResIdx;
		pushConsts.previousResourceIndex = data.m_passRecordContext->m_commonRenderData->m_prevResIdx;


		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		vkCmdDispatch(cmdBuf, 1, 1, 1);
	});
}
