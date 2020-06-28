#include "VolumetricFogMergedPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

extern bool g_fogDithering;
extern bool g_fogDoubleSample;
extern bool g_fogJittering;
extern int g_volumetricShadow;
extern float g_fogHistoryAlpha;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogMerged.hlsli"
}

void VEngine::VolumetricFogMergedPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_historyImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowAtlasImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_fomImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_localMediaBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	Constants consts;
	consts.viewMatrix = commonData->m_viewMatrix;
	consts.prevViewMatrix = commonData->m_prevViewMatrix;
	consts.prevProjMatrix = commonData->m_prevProjectionMatrix;
	consts.reprojectedTexCoordScaleBias = *(glm::vec4 *)data.m_reprojectedTexCoordScaleBias;
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
	consts.useDithering = g_fogDithering;
	consts.sampleCount = g_fogDoubleSample ? 2 : 1;
	consts.volumetricShadow = g_volumetricShadow;
	consts.globalMediaCount = commonData->m_globalParticipatingMediaCount;
	consts.localMediaCount = commonData->m_localParticipatingMediaCount;
	consts.ignoreHistory = data.m_ignoreHistory;
	consts.alpha = g_fogHistoryAlpha;

	memcpy(uboDataPtr, &consts, sizeof(consts));

	graph.addPass("Volumetric Fog Merged", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricFogMerged_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);
				ImageView *historyImageView = registry.getImageView(data.m_historyImageViewHandle);
				ImageView *shadowImageView = registry.getImageView(data.m_shadowImageViewHandle);
				ImageView *shadowAtlasImageViewHandle = registry.getImageView(data.m_shadowAtlasImageViewHandle);
				ImageView *fomImageViewHandle = registry.getImageView(data.m_fomImageViewHandle);
				DescriptorBufferInfo punctualLightsMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsBitMaskBufferHandle);
				DescriptorBufferInfo punctualLightsShadowedMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsShadowedBitMaskBufferHandle);
				DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);
				DescriptorBufferInfo localMediaMaskBufferInfo = registry.getBufferInfo(data.m_localMediaBitMaskBufferHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::sampledImage(&historyImageView, HISTORY_IMAGE_BINDING),
					Initializers::sampledImage(&shadowImageView, SHADOW_IMAGE_BINDING),
					Initializers::sampledImage(&shadowAtlasImageViewHandle, SHADOW_ATLAS_IMAGE_BINDING),
					Initializers::sampledImage(&fomImageViewHandle, FOM_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_shadowSampler, SHADOW_SAMPLER_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
					Initializers::storageBuffer(&data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING),
					Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::storageBuffer(&data.m_directionalLightsBufferInfo, DIRECTIONAL_LIGHTS_BINDING),
					Initializers::storageBuffer(&data.m_directionalLightsShadowedBufferInfo, DIRECTIONAL_LIGHTS_SHADOWED_BINDING),
					Initializers::storageBuffer(&data.m_punctualLightsBufferInfo, PUNCTUAL_LIGHTS_BINDING),
					Initializers::storageBuffer(&data.m_punctualLightsZBinsBufferInfo, PUNCTUAL_LIGHTS_Z_BINS_BINDING),
					Initializers::storageBuffer(&punctualLightsMaskBufferInfo, PUNCTUAL_LIGHTS_BIT_MASK_BINDING),
					Initializers::storageBuffer(&data.m_punctualLightsShadowedBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BINDING),
					Initializers::storageBuffer(&data.m_punctualLightsShadowedZBinsBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING),
					Initializers::storageBuffer(&punctualLightsShadowedMaskBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING),
					Initializers::storageBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
					Initializers::storageBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
					Initializers::storageBuffer(&data.m_localMediaBufferInfo, LOCAL_MEDIA_BINDING),
					Initializers::storageBuffer(&data.m_localMediaZBinsBufferInfo, LOCAL_MEDIA_Z_BINS_BINDING),
					Initializers::storageBuffer(&localMediaMaskBufferInfo, LOCAL_MEDIA_BIT_MASK_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTexture3DDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
			}


			const auto &imageDesc = registry.getImage(data.m_resultImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;
			uint32_t d = imageDesc.m_depth;

			cmdList->dispatch((w + 3) / 4, (h + 3) / 4, (d + 3) / 4);
		});
}