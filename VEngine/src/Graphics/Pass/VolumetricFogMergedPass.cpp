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
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint64_t alignment = graph.getGraphicsDevice()->getBufferAlignment(DescriptorType2::CONSTANT_BUFFER, sizeof(Constants));
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(alignment, uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	Constants consts;
	consts.viewMatrixDepthRow = glm::vec4(commonData->m_viewMatrix[0][2], commonData->m_viewMatrix[1][2], commonData->m_viewMatrix[2][2], commonData->m_viewMatrix[3][2]);
	consts.volumeResResultRes = glm::vec4(160.0f, 90.0f, commonData->m_width, commonData->m_height);
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

	memcpy(uboDataPtr, &consts, sizeof(consts));

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), { gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowAtlasImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_fomImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_directionalLightFOMImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_directionalLightFOMDepthRangeImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_localMediaBitMaskBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Volumetric Fog Merged", rg::QueueType::GRAPHICS, (uint32_t)std::size(passUsages), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricFogMerged_cs");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);
				ImageView *shadowImageView = registry.getImageView(data.m_shadowImageViewHandle);
				ImageView *shadowAtlasImageViewHandle = registry.getImageView(data.m_shadowAtlasImageViewHandle);
				ImageView *fomImageViewHandle = registry.getImageView(data.m_fomImageViewHandle);
				ImageView *fomDirectionalImageView = registry.getImageView(data.m_directionalLightFOMImageViewHandle);
				ImageView *fomDirectionalDepthRangeImageView = registry.getImageView(data.m_directionalLightFOMDepthRangeImageViewHandle);
				DescriptorBufferInfo punctualLightsMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsBitMaskBufferHandle);
				DescriptorBufferInfo punctualLightsShadowedMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsShadowedBitMaskBufferHandle);
				DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);
				DescriptorBufferInfo localMediaMaskBufferInfo = registry.getBufferInfo(data.m_localMediaBitMaskBufferHandle);

				DescriptorSetUpdate2 updates[] =
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
					Initializers::byteBuffer(&punctualLightsMaskBufferInfo, PUNCTUAL_LIGHTS_BIT_MASK_BINDING),
					Initializers::structuredBuffer(&data.m_punctualLightsShadowedBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BINDING),
					Initializers::byteBuffer(&data.m_punctualLightsShadowedZBinsBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING),
					Initializers::byteBuffer(&punctualLightsShadowedMaskBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING),
					Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
					Initializers::structuredBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
					Initializers::structuredBuffer(&data.m_localMediaBufferInfo, LOCAL_MEDIA_BINDING),
					Initializers::byteBuffer(&data.m_localMediaZBinsBufferInfo, LOCAL_MEDIA_Z_BINS_BINDING),
					Initializers::byteBuffer(&localMediaMaskBufferInfo, LOCAL_MEDIA_BIT_MASK_BINDING),
				};

				descriptorSet->update((uint32_t)std::size(updates), updates);

				DescriptorSet *sets[]
				{ 
					descriptorSet,
					data.m_passRecordContext->m_renderResources->m_computeTexture3DDescriptorSet, 
					data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet, 
					data.m_passRecordContext->m_renderResources->m_computeShadowSamplerDescriptorSet 
				};
				cmdList->bindDescriptorSets(pipeline, 0, (uint32_t)std::size(sets), sets);
			}


			const auto &imageDesc = registry.getImage(data.m_resultImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;
			uint32_t d = imageDesc.m_depth;

			cmdList->dispatch((w + 3) / 4, (h + 3) / 4, (d + 3) / 4);
		});
}
