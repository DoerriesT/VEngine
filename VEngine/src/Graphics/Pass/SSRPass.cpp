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

#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/ssr.hlsli"
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

	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	const uint32_t imageWidth = data.m_passRecordContext->m_commonRenderData->m_width;
	const uint32_t imageHeight = data.m_passRecordContext->m_commonRenderData->m_height;

	uint32_t maxLevel = 1;
	{
		uint32_t w = imageWidth;
		uint32_t h = imageHeight;
		while (w > 1 || h > 1)
		{
			++maxLevel;
			w /= 2;
			h /= 2;
		}
	}

	const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
	const auto frameIdx = data.m_passRecordContext->m_commonRenderData->m_frame;

	glm::ivec2 subsamples[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

	Constants consts;
	consts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
	consts.projectionMatrix = data.m_passRecordContext->m_commonRenderData->m_projectionMatrix;
	consts.subsample = subsamples[frameIdx % 4];
	// hiZMaxLevel needs to be clamped to some resolution dependent upper bound in order to avoid artifacts in the right screen corner
	// TODO: figure out why the artifacts appear and find a better fix
	consts.hiZMaxLevel = static_cast<float>(glm::min(maxLevel, 7u));
	consts.noiseScale = glm::vec2(1.0f / 64.0f);
	const size_t haltonIdx = frameIdx % numHaltonSamples;
	consts.noiseJitter = glm::vec2(haltonX[haltonIdx], haltonY[haltonIdx]);// *0.0f;
	consts.width = imageWidth;
	consts.height = imageHeight;
	consts.texelWidth = 1.0f / consts.width;
	consts.texelHeight = 1.0f / consts.height;
	consts.noiseTexId = data.m_noiseTextureHandle - 1;
	consts.bias = data.m_bias;

	memcpy(uboDataPtr, &consts, sizeof(consts));

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_rayHitPDFImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_maskImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_hiZPyramidImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_normalImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_specularRoughnessImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("SSR", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/ssr_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				ImageView *rayHitPDFImageView = registry.getImageView(data.m_rayHitPDFImageHandle);
				ImageView *maskImageView = registry.getImageView(data.m_maskImageHandle);
				ImageView *hiZImageView = registry.getImageView(data.m_hiZPyramidImageHandle);
				ImageView *normalImageView = registry.getImageView(data.m_normalImageHandle);
				ImageView *specularRoughnessImageView = registry.getImageView(data.m_specularRoughnessImageHandle);

				DescriptorSetUpdate updates[] =
				{
					
					Initializers::storageImage(&rayHitPDFImageView, RAY_HIT_PDF_IMAGE_BINDING),
					Initializers::storageImage(&maskImageView, MASK_IMAGE_BINDING),
					Initializers::sampledImage(&hiZImageView, HIZ_PYRAMID_IMAGE_BINDING),
					Initializers::sampledImage(&normalImageView, NORMAL_IMAGE_BINDING),
					Initializers::sampledImage(&specularRoughnessImageView, SPEC_ROUGHNESS_IMAGE_BINDING),
					Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);
			}

			DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet };
			cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);

			cmdList->dispatch((width / 2 + 7) / 8, (height / 2 + 7) / 8, 1);
		});
}
