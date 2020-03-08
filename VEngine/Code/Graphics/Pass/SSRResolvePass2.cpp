#include "SSRResolvePass2.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext2.h"
#include "Graphics/RenderData.h"
#include "Utility/Utility.h"
#include <glm/detail/func_trigonometric.hpp>
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	constexpr size_t numHaltonSamples = 1024;
	float haltonX[numHaltonSamples];
	float haltonY[numHaltonSamples];

	using namespace glm;
#include "../../../../Application/Resources/Shaders/ssrResolve_bindings.h"
}

void VEngine::SSRResolvePass2::addToGraph(rg::RenderGraph2 &graph, const Data &data)
{
	static bool initialized = false;
	if (!initialized)
	{
		initialized = true;
		for (size_t i = 0; i < numHaltonSamples; ++i)
		{
			haltonX[i] = Utility::halton(i + 1, 2);
			haltonY[i] = Utility::halton(i + 1, 3);
		}
	}

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_resultMaskImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_rayHitPDFImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_maskImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_normalImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_albedoImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_prevColorImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_velocityImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("SSR Resolve", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry2 &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/ssrResolve_comp.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);
				ImageView *resultMaskImageView = registry.getImageView(data.m_resultMaskImageHandle);
				ImageView *rayHitPdfImageView = registry.getImageView(data.m_rayHitPDFImageHandle);
				ImageView *maskImageView = registry.getImageView(data.m_maskImageHandle);
				ImageView *depthImageView = registry.getImageView(data.m_depthImageHandle);
				ImageView *normalImageView = registry.getImageView(data.m_normalImageHandle);
				ImageView *prevColorImageView = registry.getImageView(data.m_prevColorImageHandle);
				ImageView *velocityImageView = registry.getImageView(data.m_velocityImageHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::storageImage(&resultMaskImageView, RESULT_MASK_IMAGE_BINDING),
					Initializers::sampledImage(&rayHitPdfImageView, RAY_HIT_PDF_IMAGE_BINDING),
					Initializers::sampledImage(&maskImageView, MASK_IMAGE_BINDING),
					Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&normalImageView, NORMAL_IMAGE_BINDING),
					Initializers::sampledImage(&prevColorImageView, PREV_COLOR_IMAGE_BINDING),
					Initializers::sampledImage(&velocityImageView, VELOCITY_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
				};

				descriptorSet->update(10, updates);
			}

			DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet };
			cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);

			uint32_t maxLevel = 1;
			{
				uint32_t w = width;
				uint32_t h = height;
				while (w > 1 || h > 1)
				{
					++maxLevel;
					w /= 2;
					h /= 2;
				}
			}

			const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
			const auto &projMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredProjectionMatrix;

			PushConsts pushConsts;
			pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
			pushConsts.depthProjectParams = glm::vec2(projMatrix[2][2], projMatrix[3][2]);
			pushConsts.noiseScale = glm::vec2(1.0f / 64.0f);
			const size_t haltonIdx = data.m_passRecordContext->m_commonRenderData->m_frame % numHaltonSamples;
			pushConsts.noiseJitter = glm::vec2(haltonX[haltonIdx], haltonY[haltonIdx]);// *0.0f;
			pushConsts.noiseTexId = data.m_noiseTextureHandle - 1;
			pushConsts.diameterToScreen = 0.5f / tanf(glm::radians(data.m_passRecordContext->m_commonRenderData->m_fovy) * 0.5f) * glm::min(width, height);
			pushConsts.bias = data.m_bias * 0.5f;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		});
}
