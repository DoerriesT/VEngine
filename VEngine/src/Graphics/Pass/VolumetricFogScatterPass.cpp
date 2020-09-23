#include "VolumetricFogScatterPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"
#include <GlobalVar.h>

using namespace VEngine::gal;

extern bool g_fogJittering;
extern int g_volumetricShadow;
extern float g_fogHistoryAlpha;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogScatter.hlsli"
}

void VEngine::VolumetricFogScatterPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	assert((data.m_merged && (!data.m_vbufferOnly && !data.m_scatterOnly)) || (!data.m_merged && (data.m_vbufferOnly != data.m_scatterOnly)));

	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint64_t alignment = graph.getGraphicsDevice()->getBufferAlignment(DescriptorType2::CONSTANT_BUFFER, sizeof(Constants));
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(alignment, uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;

	Constants consts;
	consts.viewMatrixDepthRow = glm::vec4(commonData->m_viewMatrix[0][2], commonData->m_viewMatrix[1][2], commonData->m_viewMatrix[2][2], commonData->m_viewMatrix[3][2]);
	consts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
	consts.volumeResResultRes = glm::vec4(g_VolumetricFogVolumeWidth, g_VolumetricFogVolumeHeight, commonData->m_width, commonData->m_height);
	consts.frustumCornerTL = { data.m_frustumCorners[0][0], data.m_frustumCorners[0][1], data.m_frustumCorners[0][2] };
	consts.jitterX = g_fogJittering ? data.m_jitter[0] : 0.5f;
	consts.frustumCornerTR = { data.m_frustumCorners[1][0], data.m_frustumCorners[1][1], data.m_frustumCorners[1][2] };
	consts.jitterY = g_fogJittering ? data.m_jitter[1] : 0.5f;
	consts.frustumCornerBL = { data.m_frustumCorners[2][0], data.m_frustumCorners[2][1], data.m_frustumCorners[2][2] };
	consts.jitterZ = g_fogJittering ? data.m_jitter[2] : 0.5f;
	consts.frustumCornerBR = { data.m_frustumCorners[3][0], data.m_frustumCorners[3][1], data.m_frustumCorners[3][2] };
	consts.directionalLightCount = commonData->m_directionalLightCount;
	consts.cameraPos = commonData->m_cameraPosition;
	consts.directionalLightShadowedCount = commonData->m_directionalLightShadowedCount;
	consts.punctualLightCount = commonData->m_punctualLightCount;
	consts.punctualLightShadowedCount = commonData->m_punctualLightShadowedCount;
	consts.jitter1 = g_fogJittering ? glm::vec3{ data.m_jitter[3], data.m_jitter[4], data.m_jitter[5] } : glm::vec3(0.5f);
	consts.volumetricShadow = g_volumetricShadow;
	consts.globalMediaCount = commonData->m_globalParticipatingMediaCount;
	consts.localMediaCount = commonData->m_localParticipatingMediaCount;
	consts.checkerBoardCondition = commonData->m_frame & 1;
	consts.volumeTexelSize = 1.0f / glm::vec2(g_VolumetricFogVolumeWidth, g_VolumetricFogVolumeHeight);
	consts.volumeDepth = g_VolumetricFogVolumeDepth;
	consts.volumeNear = g_VolumetricFogVolumeNear;
	consts.volumeFar = g_VolumetricFogVolumeFar;

	// temporal filter params
	consts.prevViewMatrix = commonData->m_prevViewMatrix;
	consts.prevProjMatrix = commonData->m_prevProjectionMatrix;
	consts.ignoreHistory = data.m_ignoreHistory;
	consts.alpha = g_fogHistoryAlpha;

	memcpy(uboDataPtr, &consts, sizeof(consts));

	rg::ResourceUsageDescription vbufferPassUsages[]
	{
		{rg::ResourceViewHandle(data.m_scatteringExtinctionImageViewHandle), { gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_emissivePhaseImageViewHandle), { gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_localMediaBitMaskImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	rg::ResourceUsageDescription scatterPassUsages[]
	{
		{rg::ResourceViewHandle(data.m_scatteringExtinctionImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_emissivePhaseImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), { gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowAtlasImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_fomImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_directionalLightFOMImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_directionalLightFOMDepthRangeImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},

		{rg::ResourceViewHandle(data.m_historyImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
	};

	rg::ResourceUsageDescription mergedPassUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), { gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowAtlasImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_fomImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_directionalLightFOMImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_directionalLightFOMDepthRangeImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_localMediaBitMaskImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},

		{rg::ResourceViewHandle(data.m_historyImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
	};

	rg::ResourceUsageDescription *passUsages = data.m_merged ? mergedPassUsages : data.m_vbufferOnly ? vbufferPassUsages : scatterPassUsages;
	uint32_t usageCount = 0;
	if (data.m_merged)
	{
		usageCount = data.m_checkerboard ? (uint32_t)std::size(mergedPassUsages) - 1 : (uint32_t)std::size(mergedPassUsages);
	}
	else if (data.m_vbufferOnly)
	{
		usageCount = (uint32_t)std::size(vbufferPassUsages);
	}
	else
	{
		usageCount = data.m_checkerboard ? (uint32_t)std::size(scatterPassUsages) - 1 : (uint32_t)std::size(scatterPassUsages);
	}


	graph.addPass(data.m_merged ? "Volumetric Fog Scatter" : data.m_scatterOnly ? "Volumetric Fog Scatter Only" : "Volumetric Fog VBuffer Only", rg::QueueType::GRAPHICS, usageCount, passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			if (data.m_merged)
			{
				builder.setComputeShader(data.m_checkerboard ? "Resources/Shaders/hlsl/volumetricFogScatter_CHECKER_BOARD_cs" : "Resources/Shaders/hlsl/volumetricFogScatter_cs");
			}
			else if (data.m_vbufferOnly)
			{
				builder.setComputeShader(data.m_checkerboard ? "Resources/Shaders/hlsl/volumetricFogScatter_CHECKER_BOARD_VBUFFER_ONLY_cs" : "Resources/Shaders/hlsl/volumetricFogScatter_VBUFFER_ONLY_cs");
			}
			else
			{
				builder.setComputeShader(data.m_checkerboard ? "Resources/Shaders/hlsl/volumetricFogScatter_CHECKER_BOARD_IN_SCATTER_ONLY_cs" : "Resources/Shaders/hlsl/volumetricFogScatter_IN_SCATTER_ONLY_cs");
			}


			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = data.m_merged || data.m_scatterOnly ? registry.getImageView(data.m_resultImageViewHandle) : nullptr;
				ImageView *scatterExtImageView = data.m_vbufferOnly || data.m_scatterOnly ? registry.getImageView(data.m_scatteringExtinctionImageViewHandle) : nullptr;
				ImageView *emissivePhaseImageView = data.m_vbufferOnly || data.m_scatterOnly ? registry.getImageView(data.m_emissivePhaseImageViewHandle) : nullptr;
				ImageView *shadowImageView = data.m_merged || data.m_scatterOnly ? registry.getImageView(data.m_shadowImageViewHandle) : nullptr;
				ImageView *shadowAtlasImageViewHandle = data.m_merged || data.m_scatterOnly ? registry.getImageView(data.m_shadowAtlasImageViewHandle) : nullptr;
				ImageView *fomImageViewHandle = data.m_merged || data.m_scatterOnly ? registry.getImageView(data.m_fomImageViewHandle) : nullptr;
				ImageView *fomDirectionalImageView = data.m_merged || data.m_scatterOnly ? registry.getImageView(data.m_directionalLightFOMImageViewHandle) : nullptr;
				ImageView *fomDirectionalDepthRangeImageView = data.m_merged || data.m_scatterOnly ? registry.getImageView(data.m_directionalLightFOMDepthRangeImageViewHandle) : nullptr;
				ImageView *punctualLightsMaskImageView = data.m_merged || data.m_scatterOnly ? registry.getImageView(data.m_punctualLightsBitMaskImageViewHandle) : nullptr;
				ImageView *punctualLightsShadowedMaskImageView = data.m_merged || data.m_scatterOnly ? registry.getImageView(data.m_punctualLightsShadowedBitMaskImageViewHandle) : nullptr;
				ImageView *participatingMediaMaskImageView = data.m_merged || data.m_vbufferOnly ? registry.getImageView(data.m_localMediaBitMaskImageViewHandle) : nullptr;
				DescriptorBufferInfo exposureDataBufferInfo = data.m_merged || data.m_scatterOnly ? registry.getBufferInfo(data.m_exposureDataBufferHandle) : DescriptorBufferInfo{};

				ImageView *historyImageView = !data.m_checkerboard && (data.m_merged || data.m_scatterOnly) ? registry.getImageView(data.m_historyImageViewHandle) : nullptr;

				DescriptorSetUpdate2 scatterUpdates[] =
				{
					Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::texture(&shadowImageView, SHADOW_IMAGE_BINDING),
					Initializers::texture(&shadowAtlasImageViewHandle, SHADOW_ATLAS_IMAGE_BINDING),
					Initializers::texture(&fomImageViewHandle, FOM_IMAGE_BINDING),
					Initializers::texture(&fomDirectionalImageView, FOM_DIRECTIONAL_IMAGE_BINDING),
					Initializers::texture(&fomDirectionalDepthRangeImageView, FOM_DIRECTIONAL_DEPTH_RANGE_IMAGE_BINDING),
					Initializers::structuredBuffer(&data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING),
					Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::structuredBuffer(&data.m_directionalLightsBufferInfo, DIRECTIONAL_LIGHTS_BINDING),
					Initializers::structuredBuffer(&data.m_directionalLightsShadowedBufferInfo, DIRECTIONAL_LIGHTS_SHADOWED_BINDING),
					Initializers::structuredBuffer(&data.m_punctualLightsBufferInfo, PUNCTUAL_LIGHTS_BINDING),
					Initializers::byteBuffer(&data.m_punctualLightsZBinsBufferInfo, PUNCTUAL_LIGHTS_Z_BINS_BINDING),
					Initializers::texture(&punctualLightsMaskImageView, PUNCTUAL_LIGHTS_BIT_MASK_BINDING),
					Initializers::structuredBuffer(&data.m_punctualLightsShadowedBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BINDING),
					Initializers::byteBuffer(&data.m_punctualLightsShadowedZBinsBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING),
					Initializers::texture(&punctualLightsShadowedMaskImageView, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING),
					Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
					Initializers::texture(&scatterExtImageView, SCATTERING_EXTINCTION_IMAGE_BINDING),
					Initializers::texture(&emissivePhaseImageView, EMISSIVE_PHASE_IMAGE_BINDING),
					Initializers::texture(&historyImageView, HISTORY_IMAGE_BINDING),
				};

				DescriptorSetUpdate2 vbufferUpdates[] =
				{
					Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::rwTexture(&scatterExtImageView, SCATTERING_EXTINCTION_IMAGE_BINDING),
					Initializers::rwTexture(&emissivePhaseImageView, EMISSIVE_PHASE_IMAGE_BINDING),
					Initializers::structuredBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
					Initializers::structuredBuffer(&data.m_localMediaBufferInfo, LOCAL_MEDIA_BINDING),
					Initializers::byteBuffer(&data.m_localMediaZBinsBufferInfo, LOCAL_MEDIA_Z_BINS_BINDING),
					Initializers::texture(&participatingMediaMaskImageView, LOCAL_MEDIA_BIT_MASK_BINDING),
				};

				DescriptorSetUpdate2 mergedUpdates[] =
				{
					Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::texture(&shadowImageView, SHADOW_IMAGE_BINDING),
					Initializers::texture(&shadowAtlasImageViewHandle, SHADOW_ATLAS_IMAGE_BINDING),
					Initializers::texture(&fomImageViewHandle, FOM_IMAGE_BINDING),
					Initializers::texture(&fomDirectionalImageView, FOM_DIRECTIONAL_IMAGE_BINDING),
					Initializers::texture(&fomDirectionalDepthRangeImageView, FOM_DIRECTIONAL_DEPTH_RANGE_IMAGE_BINDING),
					Initializers::structuredBuffer(&data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING),
					Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::structuredBuffer(&data.m_directionalLightsBufferInfo, DIRECTIONAL_LIGHTS_BINDING),
					Initializers::structuredBuffer(&data.m_directionalLightsShadowedBufferInfo, DIRECTIONAL_LIGHTS_SHADOWED_BINDING),
					Initializers::structuredBuffer(&data.m_punctualLightsBufferInfo, PUNCTUAL_LIGHTS_BINDING),
					Initializers::byteBuffer(&data.m_punctualLightsZBinsBufferInfo, PUNCTUAL_LIGHTS_Z_BINS_BINDING),
					Initializers::texture(&punctualLightsMaskImageView, PUNCTUAL_LIGHTS_BIT_MASK_BINDING),
					Initializers::structuredBuffer(&data.m_punctualLightsShadowedBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BINDING),
					Initializers::byteBuffer(&data.m_punctualLightsShadowedZBinsBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING),
					Initializers::texture(&punctualLightsShadowedMaskImageView, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING),
					Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
					Initializers::structuredBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
					Initializers::structuredBuffer(&data.m_localMediaBufferInfo, LOCAL_MEDIA_BINDING),
					Initializers::byteBuffer(&data.m_localMediaZBinsBufferInfo, LOCAL_MEDIA_Z_BINS_BINDING),
					Initializers::texture(&participatingMediaMaskImageView, LOCAL_MEDIA_BIT_MASK_BINDING),
					Initializers::texture(&historyImageView, HISTORY_IMAGE_BINDING),
				};

				if (data.m_merged)
				{
					descriptorSet->update(data.m_checkerboard ? (uint32_t)std::size(mergedUpdates) - 1 : (uint32_t)std::size(mergedUpdates), mergedUpdates);

					DescriptorSet *sets[]
					{
						descriptorSet,
						data.m_passRecordContext->m_renderResources->m_computeTexture3DDescriptorSet,
						data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet,
						data.m_passRecordContext->m_renderResources->m_computeShadowSamplerDescriptorSet
					};
					cmdList->bindDescriptorSets(pipeline, 0, (uint32_t)std::size(sets), sets);
				}
				else if (data.m_vbufferOnly)
				{
					descriptorSet->update((uint32_t)std::size(vbufferUpdates), vbufferUpdates);

					DescriptorSet *sets[]
					{
						descriptorSet,
						data.m_passRecordContext->m_renderResources->m_computeTexture3DDescriptorSet,
						data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet,
					};
					cmdList->bindDescriptorSets(pipeline, 0, (uint32_t)std::size(sets), sets);
				}
				else
				{
					descriptorSet->update(data.m_checkerboard ? (uint32_t)std::size(scatterUpdates) - 1 : (uint32_t)std::size(scatterUpdates), scatterUpdates);

					DescriptorSet *sets[]
					{
						descriptorSet,
						data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet,
						data.m_passRecordContext->m_renderResources->m_computeShadowSamplerDescriptorSet
					};
					cmdList->bindDescriptorSets(pipeline, 0, (uint32_t)std::size(sets), sets);
				}
			}


			const auto &imageDesc = registry.getImage(data.m_resultImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;
			uint32_t d = imageDesc.m_depth;

			cmdList->dispatch((w + 3) / 4, (h + 3) / 4, (d + 3) / 4);
		});
}
