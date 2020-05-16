#include "VolumetricFogApplyPass.h"
#include <glm/vec4.hpp>
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Utility/Utility.h"
#include "Graphics/gal/Initializers.h"

extern bool g_fogLookupDithering;

using namespace VEngine::gal;

namespace
{
	constexpr size_t numHaltonSamples = 32;
	float haltonX[numHaltonSamples];
	float haltonY[numHaltonSamples];
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogApply.hlsli"
}

void VEngine::VolumetricFogApplyPass::addToGraph(rg::RenderGraph &graph, const Data &data)
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
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::READ_WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_volumetricFogImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_indirectSpecularLightImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_brdfLutImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_specularRoughnessImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_normalImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_reflectionProbeBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Volumetric Fog Apply", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricFogApply_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);
				ImageView *depthImageView = registry.getImageView(data.m_depthImageViewHandle);
				ImageView *volumetricFogImageView = registry.getImageView(data.m_volumetricFogImageViewHandle);
				ImageView *indirectSpecularImageView = registry.getImageView(data.m_indirectSpecularLightImageViewHandle);
				ImageView *brdfLutImageView = registry.getImageView(data.m_brdfLutImageViewHandle);
				ImageView *specularRoughnessImageView = registry.getImageView(data.m_specularRoughnessImageViewHandle);
				ImageView *normalImageView = registry.getImageView(data.m_normalImageViewHandle);
				DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);
				DescriptorBufferInfo reflProbeMaskBufferInfo = registry.getBufferInfo(data.m_reflectionProbeBitMaskBufferHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&volumetricFogImageView, VOLUMETRIC_FOG_IMAGE_BINDING),
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
					Initializers::sampledImage(&indirectSpecularImageView, INDIRECT_SPECULAR_LIGHT_IMAGE_BINDING),
					Initializers::sampledImage(&brdfLutImageView, BRDF_LUT_IMAGE_BINDING),
					Initializers::sampledImage(&specularRoughnessImageView, SPEC_ROUGHNESS_IMAGE_BINDING),
					Initializers::sampledImage(&normalImageView, NORMAL_IMAGE_BINDING),
					Initializers::sampledImage(&data.m_reflectionProbeImageView, REFLECTION_PROBE_IMAGE_BINDING),
					Initializers::storageBuffer(&data.m_reflectionProbeDataBufferInfo, REFLECTION_PROBE_DATA_BINDING),
					Initializers::storageBuffer(&reflProbeMaskBufferInfo, REFLECTION_PROBE_BIT_MASK_BINDING),
					Initializers::storageBuffer(&data.m_reflectionProbeZBinsBufferInfo, REFLECTION_PROBE_Z_BINS_BINDING),
					Initializers::storageBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);
			}

			const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;

			PushConsts pushConsts;
			pushConsts.invViewMatrix = data.m_passRecordContext->m_commonRenderData->m_invViewMatrix;
			pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
			pushConsts.noiseScale = glm::vec2(1.0f / 64.0f);
			const size_t haltonIdx = data.m_passRecordContext->m_commonRenderData->m_frame % numHaltonSamples;
			pushConsts.noiseJitter = glm::vec2(haltonX[haltonIdx], haltonY[haltonIdx]);// *0.0f;
			pushConsts.noiseTexId = data.m_noiseTextureHandle - 1;
			pushConsts.width = width;
			pushConsts.height = height;
			pushConsts.texelWidth = 1.0f / width;
			pushConsts.texelHeight = 1.0f / height;
			pushConsts.useNoise = g_fogLookupDithering;
			pushConsts.probeCount = data.m_passRecordContext->m_commonRenderData->m_reflectionProbeCount;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		});
}
