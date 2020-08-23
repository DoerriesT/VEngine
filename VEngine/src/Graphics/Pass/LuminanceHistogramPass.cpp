#include "LuminanceHistogramPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "GlobalVar.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/luminanceHistogram.hlsli"
}

void VEngine::LuminanceHistogramPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_luminanceHistogramBufferHandle), {gal::ResourceState::WRITE_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_lightImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Luminance Histogram", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/hlsl/luminanceHistogram_cs.spv");

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		cmdList->bindPipeline(pipeline);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *lightImageView = registry.getImageView(data.m_lightImageHandle);
			DescriptorBufferInfo histoBufferInfo = registry.getBufferInfo(data.m_luminanceHistogramBufferHandle);
			DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

			DescriptorSetUpdate2 updates[] =
			{
				Initializers::texture(&lightImageView, INPUT_IMAGE_BINDING),
				Initializers::rwByteBuffer(&histoBufferInfo, LUMINANCE_HISTOGRAM_BINDING),
				Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
			};

			descriptorSet->update((uint32_t)std::size(updates), updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

		float logMin = g_ExposureHistogramLogMin;
		float logMax = g_ExposureHistogramLogMax;
		logMin = glm::min(logMin, logMax - 1e-7f); // ensure logMin is a little bit less than logMax


		PushConsts pushConsts;
		pushConsts.scale = 1.0f / (logMax - logMin);
		pushConsts.bias = -logMin * pushConsts.scale;
		pushConsts.width = width;

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		cmdList->dispatch(height, 1, 1);
	});
}
