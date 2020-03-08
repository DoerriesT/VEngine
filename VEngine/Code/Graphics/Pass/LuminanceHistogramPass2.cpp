#include "LuminanceHistogramPass2.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include "Graphics/PassRecordContext2.h"
#include "Graphics/RenderData.h"
#include "GlobalVar.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/luminanceHistogram_bindings.h"
}

void VEngine::LuminanceHistogramPass2::addToGraph(rg::RenderGraph2 &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_luminanceHistogramBufferHandle), {gal::ResourceState::WRITE_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_lightImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
	};

	graph.addPass("Luminance Histogram", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry2 &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/luminanceHistogram_comp.spv");

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *lightImageView = registry.getImageView(data.m_lightImageHandle);
			DescriptorBufferInfo histoBufferInfo = registry.getBufferInfo(data.m_luminanceHistogramBufferHandle);

			DescriptorSetUpdate updates[] =
			{
				Initializers::sampledImage(&lightImageView, SOURCE_IMAGE_BINDING),
				Initializers::storageBuffer(&histoBufferInfo, LUMINANCE_HISTOGRAM_BINDING),
				Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
			};

			descriptorSet->update(3, updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

		float logMin = g_ExposureHistogramLogMin;
		float logMax = g_ExposureHistogramLogMax;
		logMin = glm::min(logMin, logMax - 1e-7f); // ensure logMin is a little bit less than logMax


		PushConsts pushConsts;
		pushConsts.scale = 1.0f / (logMax - logMin);
		pushConsts.bias = -logMin * pushConsts.scale;

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		cmdList->dispatch(height, 1, 1);
	});
}
