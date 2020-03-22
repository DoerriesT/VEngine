#include "VolumetricFogScatterPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include <glm/mat4x4.hpp>
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"
#include "Graphics/gal/Initializers.h"
#include "Graphics/MappableBufferBlock.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogScatter.hlsli"
}

void VEngine::VolumetricFogScatterPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_prevResultImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_scatteringExtinctionImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_emissivePhaseImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_pointLightBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	const auto &lightData = *data.m_lightData;

	Constants consts;
	consts.viewMatrix = commonData->m_viewMatrix;
	consts.prevViewMatrix = commonData->m_prevViewMatrix;
	consts.prevProjMatrix = commonData->m_prevProjectionMatrix;
	consts.frustumCornerTL = { data.m_frustumCorners[0][0], data.m_frustumCorners[0][1], data.m_frustumCorners[0][2] };
	consts.jitterX = data.m_jitter[0];
	consts.frustumCornerTR = { data.m_frustumCorners[1][0], data.m_frustumCorners[1][1], data.m_frustumCorners[1][2] };
	consts.jitterY = data.m_jitter[1];
	consts.frustumCornerBL = { data.m_frustumCorners[2][0], data.m_frustumCorners[2][1], data.m_frustumCorners[2][2] };
	consts.jitterZ = data.m_jitter[2];
	consts.frustumCornerBR = { data.m_frustumCorners[3][0], data.m_frustumCorners[3][1], data.m_frustumCorners[3][2] };
	consts.cascadeOffset = static_cast<int32_t>(lightData.m_shadowMatricesOffsetCount >> 16);
	consts.cameraPos = commonData->m_cameraPosition;
	consts.cascadeCount = static_cast<int32_t>(lightData.m_shadowMatricesOffsetCount & 0xFFFF);
	consts.sunDirection = glm::normalize(glm::vec3(commonData->m_invViewMatrix * glm::vec4(glm::vec3(lightData.m_direction), 0.0f)));
	consts.pointLightCount = commonData->m_pointLightCount;
	consts.sunLightRadiance = lightData.m_color;
	consts.spotLightCount = commonData->m_spotLightCount;
	consts.reprojectedTexCoordScaleBias = *(glm::vec4 *)data.m_reprojectedTexCoordScaleBias;

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
				DescriptorBufferInfo pointLightMaskBufferInfo = registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle);
				DescriptorBufferInfo spotLightMaskBufferInfo = registry.getBufferInfo(data.m_spotLightBitMaskBufferHandle);

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
					Initializers::storageBuffer(&data.m_pointLightDataBufferInfo, POINT_LIGHT_DATA_BINDING),
					Initializers::storageBuffer(&data.m_pointLightZBinsBufferInfo, POINT_LIGHT_Z_BINS_BUFFER_BINDING),
					Initializers::storageBuffer(&pointLightMaskBufferInfo, POINT_LIGHT_BIT_MASK_BUFFER_BINDING),
					Initializers::storageBuffer(&data.m_spotLightDataBufferInfo, SPOT_LIGHT_DATA_BINDING),
					Initializers::storageBuffer(&data.m_spotLightZBinsBufferInfo, SPOT_LIGHT_Z_BINS_BUFFER_BINDING),
					Initializers::storageBuffer(&spotLightMaskBufferInfo, SPOT_LIGHT_BIT_MASK_BUFFER_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}


			const auto &imageDesc = registry.getImage(data.m_resultImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;
			uint32_t d = imageDesc.m_depth;

			cmdList->dispatch((w + 1) / 2, (h + 1) / 2, (d + 15) / 16);
		});
}
