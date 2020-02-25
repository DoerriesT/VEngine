#include "LuminanceHistogramReduceAveragePass.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <glm/vec3.hpp>
#include "GlobalVar.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/luminanceHistogramReduceAverage_bindings.h"
}

void VEngine::LuminanceHistogramAveragePass::addToGraph(RenderGraph &graph, const Data &data)
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

		float logMin = g_ExposureHistogramLogMin;
		float logMax = g_ExposureHistogramLogMax;
		logMin = glm::min(logMin, logMax - 1e-7f); // ensure logMin is a little bit less than logMax

		float lowerBoundPercentage = g_ExposureLowPercentage;
		float upperBoundPercentage = g_ExposureHighPercentage;
		upperBoundPercentage = glm::clamp(upperBoundPercentage, 0.01f, 1.0f);
		lowerBoundPercentage = glm::clamp(lowerBoundPercentage, 0.0f, upperBoundPercentage - 0.01f);

		PushConsts pushConsts;
		pushConsts.precomputedTermUp = 1.0f - exp(-data.m_passRecordContext->m_commonRenderData->m_timeDelta * g_ExposureSpeedUp);
		pushConsts.precomputedTermDown = 1.0f - exp(-data.m_passRecordContext->m_commonRenderData->m_timeDelta * g_ExposureSpeedDown);
		pushConsts.invScale = logMax - logMin;
		pushConsts.bias = -logMin * (1.0f / pushConsts.invScale);
		pushConsts.lowerBound = static_cast<uint32_t>(width * height * lowerBoundPercentage);
		pushConsts.upperBound = static_cast<uint32_t>(width * height * upperBoundPercentage);
		
		// ensure at least one pixel passes
		if (pushConsts.lowerBound == pushConsts.upperBound)
		{
			pushConsts.lowerBound -= glm::min(pushConsts.lowerBound, 1u);
			pushConsts.upperBound += 1;
		}

		pushConsts.currentResourceIndex = data.m_passRecordContext->m_commonRenderData->m_curResIdx;
		pushConsts.previousResourceIndex = data.m_passRecordContext->m_commonRenderData->m_prevResIdx;


		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		vkCmdDispatch(cmdBuf, 1, 1, 1);
	});
}
