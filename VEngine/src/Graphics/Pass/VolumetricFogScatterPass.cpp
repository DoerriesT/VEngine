#include "VolumetricFogScatterPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include <glm/mat4x4.hpp>
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"
#include "Graphics/gal/Initializers.h"
#include <glm/packing.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Graphics/MappableBufferBlock.h"
#include "Utility/Utility.h"

float g_volumeDepth = 64.0f;

using namespace VEngine::gal;

namespace
{
	constexpr size_t numHaltonSamples = 64;
	float haltonX[numHaltonSamples];
	float haltonY[numHaltonSamples];
	float haltonZ[numHaltonSamples];

#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogScatter.hlsli"
}

void VEngine::VolumetricFogScatterPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	static bool initialized = false;
	if (!initialized)
	{
		initialized = true;
		for (size_t i = 0; i < numHaltonSamples; ++i)
		{
			haltonX[i] = Utility::halton(i + 1, 2);
			haltonY[i] = Utility::halton(i + 1, 3);
			haltonZ[i] = Utility::halton(i + 1, 5);
		}
	}

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_prevResultImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_scatteringExtinctionImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_emissivePhaseImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
	};

	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	auto proj = glm::perspective(commonData->m_fovy, commonData->m_width / float(commonData->m_height), commonData->m_nearPlane, g_volumeDepth);
	auto invViewProj = glm::inverse(proj * commonData->m_viewMatrix);
	glm::vec4 frustumCorners[4];
	frustumCorners[0] = invViewProj * glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f);
	frustumCorners[1] = invViewProj * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	frustumCorners[2] = invViewProj * glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f);
	frustumCorners[3] = invViewProj * glm::vec4(1.0f, -1.0f, 1.0f, 1.0f);

	for (auto &c : frustumCorners)
	{
		c /= c.w;
		c -= commonData->m_cameraPosition;
	}

	const size_t haltonIdx = data.m_passRecordContext->m_commonRenderData->m_frame % numHaltonSamples;
	const auto &lightData = *data.m_lightData;

	Constants consts;
	consts.prevViewMatrix = commonData->m_prevViewMatrix;
	consts.prevProjMatrix = commonData->m_prevProjectionMatrix;
	consts.frustumCornerTL = frustumCorners[0];
	consts.jitterX = haltonX[haltonIdx];
	consts.frustumCornerTR = frustumCorners[1];
	consts.jitterY = haltonY[haltonIdx];
	consts.frustumCornerBL = frustumCorners[2];
	consts.jitterZ = haltonZ[haltonIdx];
	consts.frustumCornerBR = frustumCorners[3];
	consts.cascadeOffset = static_cast<int32_t>(lightData.m_shadowMatricesOffsetCount >> 16);
	consts.cameraPos = commonData->m_cameraPosition;
	consts.cascadeCount = static_cast<int32_t>(lightData.m_shadowMatricesOffsetCount & 0xFFFF);
	consts.sunDirection = glm::normalize(glm::vec3(commonData->m_invViewMatrix * glm::vec4(glm::vec3(lightData.m_direction), 0.0f)));
	consts.sunLightRadiance = lightData.m_color;

	memcpy(uboDataPtr, &consts, sizeof(consts));

	graph.addPass("Volumetric Fog Scatter", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricFogScatter_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);
				ImageView *prevResultImageView = registry.getImageView(data.m_prevResultImageViewHandle);
				ImageView *scatteringExtinctionImageView = registry.getImageView(data.m_scatteringExtinctionImageViewHandle);
				ImageView *emissivePhaseImageView = registry.getImageView(data.m_emissivePhaseImageViewHandle);
				ImageView *shadowSpaceImageView = registry.getImageView(data.m_shadowImageViewHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::sampledImage(&prevResultImageView, PREV_RESULT_IMAGE_BINDING),
					Initializers::sampledImage(&scatteringExtinctionImageView, SCATTERING_EXTINCTION_IMAGE_BINDING),
					Initializers::sampledImage(&emissivePhaseImageView, EMISSIVE_PHASE_IMAGE_BINDING),
					Initializers::sampledImage(&shadowSpaceImageView, SHADOW_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_shadowSampler, SHADOW_SAMPLER_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
					Initializers::storageBuffer(&data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING),
					Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}


			const auto &imageDesc = registry.getImage(data.m_resultImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;
			uint32_t d = imageDesc.m_depth;

			cmdList->dispatch((w + 7) / 8, (h + 7) / 8, d);
		});
}
