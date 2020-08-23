#include "VolumetricFogApplyPass.h"
#include <glm/vec4.hpp>
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

extern bool g_fogLookupDithering;
extern uint32_t g_fogLookupDitherType;
extern bool g_raymarchedFog;

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogApply.hlsli"
}

void VEngine::VolumetricFogApplyPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::READ_WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_raymarchedVolumetricsImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_ssaoImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_volumetricFogImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		//{rg::ResourceViewHandle(data.m_indirectSpecularLightImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_brdfLutImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_albedoMetalnessImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_normalRoughnessImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
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
				ImageView *raymarchedVolumetricsImageView = registry.getImageView(data.m_raymarchedVolumetricsImageViewHandle);
				ImageView *ssaoImageView = registry.getImageView(data.m_ssaoImageViewHandle);
				ImageView *volumetricFogImageView = registry.getImageView(data.m_volumetricFogImageViewHandle);
				//ImageView *indirectSpecularImageView = registry.getImageView(data.m_indirectSpecularLightImageViewHandle);
				ImageView *brdfLutImageView = registry.getImageView(data.m_brdfLutImageViewHandle);
				ImageView *albedoMetalnessImageView = registry.getImageView(data.m_albedoMetalnessImageViewHandle);
				ImageView *normalRoughnessImageView = registry.getImageView(data.m_normalRoughnessImageViewHandle);
				DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);
				DescriptorBufferInfo reflProbeMaskBufferInfo = registry.getBufferInfo(data.m_reflectionProbeBitMaskBufferHandle);

				DescriptorSetUpdate2 updates[] =
				{
					Initializers::texture(&depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::texture(&volumetricFogImageView, VOLUMETRIC_FOG_IMAGE_BINDING),
					Initializers::texture(&raymarchedVolumetricsImageView, RAYMARCHED_VOLUMETRICS_IMAGE_BINDING),
					Initializers::texture(&ssaoImageView, SSAO_IMAGE_BINDING),
					Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
					//Initializers::sampledImage(&indirectSpecularImageView, INDIRECT_SPECULAR_LIGHT_IMAGE_BINDING),
					Initializers::texture(&brdfLutImageView, BRDF_LUT_IMAGE_BINDING),
					Initializers::texture(&albedoMetalnessImageView, ALBEDO_METALNESS_IMAGE_BINDING),
					Initializers::texture(&normalRoughnessImageView, NORMAL_ROUGHNESS_IMAGE_BINDING),
					Initializers::texture(&data.m_reflectionProbeImageView, REFLECTION_PROBE_IMAGE_BINDING),
					Initializers::structuredBuffer(&data.m_reflectionProbeDataBufferInfo, REFLECTION_PROBE_DATA_BINDING),
					Initializers::byteBuffer(&reflProbeMaskBufferInfo, REFLECTION_PROBE_BIT_MASK_BINDING),
					Initializers::byteBuffer(&data.m_reflectionProbeZBinsBufferInfo, REFLECTION_PROBE_Z_BINS_BINDING),
					Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
					Initializers::texture(&data.m_blueNoiseImageView, BLUE_NOISE_IMAGE_BINDING),
				};

				descriptorSet->update((uint32_t)std::size(updates), updates);

				DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
			}

			const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;

			PushConsts pushConsts;
			pushConsts.invViewMatrix = data.m_passRecordContext->m_commonRenderData->m_invViewMatrix;
			pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
			pushConsts.frame = data.m_passRecordContext->m_commonRenderData->m_frame;
			pushConsts.width = width;
			pushConsts.height = height;
			pushConsts.texelWidth = 1.0f / width;
			pushConsts.texelHeight = 1.0f / height;
			pushConsts.probeCount = data.m_passRecordContext->m_commonRenderData->m_reflectionProbeCount;
			pushConsts.ssao = data.m_ssao;
			pushConsts.raymarchedFog = g_raymarchedFog;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		});
}
