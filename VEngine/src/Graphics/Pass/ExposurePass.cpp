#include "ExposurePass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <glm/vec3.hpp>
#include "GlobalVar.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/exposure.hlsli"
}

void VEngine::ExposurePass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_luminanceHistogramBufferHandle), {gal::ResourceState::READ_WRITE_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_avgLuminanceBufferHandle), {gal::ResourceState::READ_WRITE_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::WRITE_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Luminance Histogram Average", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/hlsl/exposure_cs.spv");

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		cmdList->bindPipeline(pipeline);
		
		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			DescriptorBufferInfo histoBufferInfo = registry.getBufferInfo(data.m_luminanceHistogramBufferHandle);
			DescriptorBufferInfo avgLumBufferInfo = registry.getBufferInfo(data.m_avgLuminanceBufferHandle);
			DescriptorBufferInfo exposureBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

			DescriptorSetUpdate updates[] =
			{
				Initializers::storageBuffer(&histoBufferInfo, LUMINANCE_HISTOGRAM_BINDING),
				Initializers::storageBuffer(&avgLumBufferInfo, LUMINANCE_VALUES_BINDING),
				Initializers::storageBuffer(&exposureBufferInfo, EXPOSURE_DATA_BINDING),
			};

			descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

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
		pushConsts.exposureCompensation = g_ExposureCompensation;
		pushConsts.exposureMin = g_ExposureMin;
		pushConsts.exposureMax = g_ExposureFixed ? g_ExposureFixedValue : g_ExposureMax;
		pushConsts.exposureMax = glm::max(pushConsts.exposureMax, 1e-7f);
		pushConsts.exposureMin = glm::clamp(pushConsts.exposureMin, 1e-7f, pushConsts.exposureMax);
		pushConsts.fixExposureToMax = g_ExposureFixed ? 1 : 0;

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);
		
		cmdList->dispatch(1, 1, 1);
	});
}
