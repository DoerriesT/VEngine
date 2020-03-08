#include "TonemapPass2.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext2.h"
#include "Graphics/RenderData.h"
#include "GlobalVar.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	using vec4 = glm::vec4;
	using mat4 = glm::mat4;
	using uint = uint32_t;
#include "../../../../Application/Resources/Shaders/tonemap_bindings.h"
}

void VEngine::TonemapPass2::addToGraph(rg::RenderGraph2 &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_avgLuminanceBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_srcImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_dstImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_bloomImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	uint32_t usageCount = data.m_bloomEnabled ? sizeof(passUsages) / sizeof(passUsages[0]) : sizeof(passUsages) / sizeof(passUsages[0]) - 1;

	graph.addPass("Tonemap", rg::QueueType::GRAPHICS, usageCount, passUsages, [=](CommandList *cmdList, const rg::Registry2 &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/tonemap_comp.spv");

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		cmdList->bindPipeline(pipeline);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *resultImageView = registry.getImageView(data.m_dstImageHandle);
			ImageView *inputImageView = registry.getImageView(data.m_srcImageHandle);
			ImageView *bloomImageImageView = registry.getImageView(data.m_bloomEnabled ? data.m_bloomImageViewHandle : data.m_srcImageHandle); // need to bind a valid image if bloom is disabled
			DescriptorBufferInfo avgLumBufferInfo = registry.getBufferInfo(data.m_avgLuminanceBufferHandle);

			DescriptorSetUpdate updates[] =
			{
				Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
				Initializers::sampledImage(&inputImageView, SOURCE_IMAGE_BINDING),
				Initializers::sampledImage(&bloomImageImageView, BLOOM_IMAGE_BINDING),
				Initializers::storageBuffer(&avgLumBufferInfo, LUMINANCE_VALUES_BINDING),
				Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
				Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
			};

			descriptorSet->update(6, updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

		PushConsts pushConsts;
		pushConsts.luminanceIndex = static_cast<uint32_t>(data.m_passRecordContext->m_commonRenderData->m_curResIdx);
		pushConsts.applyLinearToGamma = data.m_applyLinearToGamma;
		pushConsts.bloomEnabled = data.m_bloomEnabled;
		pushConsts.bloomStrength = data.m_bloomStrength;
		pushConsts.exposureCompensation = g_ExposureCompensation;
		pushConsts.exposureMin = g_ExposureMin;
		pushConsts.exposureMax = g_ExposureFixed ? g_ExposureFixedValue : g_ExposureMax;

		pushConsts.exposureMax = glm::max(pushConsts.exposureMax, 1e-7f);
		pushConsts.exposureMin = glm::clamp(pushConsts.exposureMin, 1e-7f, pushConsts.exposureMax);

		pushConsts.fixExposureToMax = g_ExposureFixed ? 1 : 0;

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
	});
}
