#include "SSRPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <random>
#include "Utility/Utility.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	constexpr size_t numHaltonSamples = 1024;
	float haltonX[numHaltonSamples];
	float haltonY[numHaltonSamples];

	using namespace glm;
#include "../../../../Application/Resources/Shaders/ssr_bindings.h"
}

void VEngine::SSRPass::addToGraph(rg::RenderGraph &graph, const Data &data)
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
		{rg::ResourceViewHandle(data.m_rayHitPDFImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_maskImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_hiZPyramidImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_normalImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("SSR", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/ssr_comp.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				ImageView *rayHitPDFImageView = registry.getImageView(data.m_rayHitPDFImageHandle);
				ImageView *maskImageView = registry.getImageView(data.m_maskImageHandle);
				ImageView *hiZImageView = registry.getImageView(data.m_hiZPyramidImageHandle);
				ImageView *normalImageView = registry.getImageView(data.m_normalImageHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&rayHitPDFImageView, RAY_HIT_PDF_IMAGE_BINDING),
					Initializers::storageImage(&maskImageView, MASK_IMAGE_BINDING),
					Initializers::sampledImage(&hiZImageView, HIZ_PYRAMID_IMAGE_BINDING),
					Initializers::sampledImage(&normalImageView, NORMAL_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
				};

				descriptorSet->update(5, updates);
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

			PushConsts pushConsts;
			pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
			pushConsts.projectionMatrix = data.m_passRecordContext->m_commonRenderData->m_projectionMatrix;
			// hiZMaxLevel needs to be clamped to some resolution dependent upper bound in order to avoid artifacts in the right screen corner
			// TODO: figure out why the artifacts appear and find a better fix
			pushConsts.hiZMaxLevel = static_cast<float>(glm::min(maxLevel, 7u));
			pushConsts.noiseScale = glm::vec2(1.0f / 64.0f);
			const size_t haltonIdx = data.m_passRecordContext->m_commonRenderData->m_frame % numHaltonSamples;
			pushConsts.noiseJitter = glm::vec2(haltonX[haltonIdx], haltonY[haltonIdx]);// *0.0f;
			pushConsts.noiseTexId = data.m_noiseTextureHandle - 1;
			pushConsts.bias = data.m_bias;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		});
}
