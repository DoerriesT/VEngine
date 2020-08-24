#include "ParticlesPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"
#include "Graphics/ParticleDrawData.h"

using namespace VEngine::gal;

extern int g_volumetricShadow;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/particles.hlsli"
}

void VEngine::ParticlesPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	uint32_t totalParticleCount = 0;
	for (size_t i = 0; i < data.m_listCount; ++i)
	{
		totalParticleCount += data.m_listSizes[i];
	}

	if (!totalParticleCount)
	{
		return;
	}

	const auto *commonData = data.m_passRecordContext->m_commonRenderData;

	// constant buffer
	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants), sizeof(Constants) };
	{
		auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

		uint8_t *uboDataPtr = nullptr;
		uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

		Constants consts;
		consts.viewMatrix = commonData->m_viewMatrix;
		consts.viewProjectionMatrix = commonData->m_jitteredViewProjectionMatrix;
		consts.cameraPosition = commonData->m_cameraPosition;
		consts.cameraUp = glm::vec3(commonData->m_viewMatrix[0][1], commonData->m_viewMatrix[1][1], commonData->m_viewMatrix[2][1]);
		consts.width = commonData->m_width;
		consts.height = commonData->m_height;
		consts.volumetricShadow = g_volumetricShadow;
		consts.directionalLightCount = commonData->m_directionalLightCount;
		consts.directionalLightShadowedCount = commonData->m_directionalLightShadowedCount;
		consts.punctualLightCount = commonData->m_punctualLightCount;
		consts.punctualLightShadowedCount = commonData->m_punctualLightShadowedCount;
		consts.frame = data.m_passRecordContext->m_commonRenderData->m_frame;

		memcpy(uboDataPtr, &consts, sizeof(consts));
	}

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_shadowImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowAtlasImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_volumetricFogImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_fomImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_directionalLightFOMImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_directionalLightFOMDepthRangeImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {gal::ResourceState::READ_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT }},
	};

	graph.addPass("Particles", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// begin renderpass
			DepthStencilAttachmentDescription depthAttachDesc =
			{ registry.getImageView(data.m_depthImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {}, true };
			ColorAttachmentDescription colorAttachDescs[]
			{
				{registry.getImageView(data.m_resultImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, {} },
			};
			cmdList->beginRenderPass(sizeof(colorAttachDescs) / sizeof(colorAttachDescs[0]), colorAttachDescs, &depthAttachDesc, { {}, {width, height} });

			PipelineColorBlendAttachmentState blendState{};
			blendState.m_blendEnable = true;
			blendState.m_srcColorBlendFactor = BlendFactor::SRC_ALPHA;
			blendState.m_dstColorBlendFactor = BlendFactor::ONE_MINUS_SRC_ALPHA;
			blendState.m_colorBlendOp = BlendOp::ADD;
			blendState.m_srcAlphaBlendFactor = BlendFactor::SRC_ALPHA;
			blendState.m_dstAlphaBlendFactor = BlendFactor::ONE_MINUS_SRC_ALPHA;
			blendState.m_alphaBlendOp = BlendOp::ADD;
			blendState.m_colorWriteMask = ColorComponentFlagBits::ALL_BITS;

			Format colorAttachmentFormats[]
			{
				registry.getImageView(data.m_resultImageViewHandle)->getImage()->getDescription().m_format,
			};

			// create pipeline description
			GraphicsPipelineCreateInfo pipelineCreateInfo;
			GraphicsPipelineBuilder builder(pipelineCreateInfo);
			builder.setVertexShader("Resources/Shaders/hlsl/particles_vs");
			builder.setFragmentShader("Resources/Shaders/hlsl/particles_ps");
			builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
			builder.setDepthTest(true, false, CompareOp::GREATER_OR_EQUAL);
			builder.setColorBlendAttachment(blendState);
			builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
			builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageViewHandle)->getImage()->getDescription().m_format);
			builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);
				ImageView *shadowSpaceImageView = registry.getImageView(data.m_shadowImageViewHandle);
				ImageView *shadowAtlasImageViewHandle = registry.getImageView(data.m_shadowAtlasImageViewHandle);
				ImageView *fomImageViewHandle = registry.getImageView(data.m_fomImageViewHandle);
				ImageView *fomDirectionalImageView = registry.getImageView(data.m_directionalLightFOMImageViewHandle);
				ImageView *fomDirectionalDepthRangeImageView = registry.getImageView(data.m_directionalLightFOMDepthRangeImageViewHandle);
				ImageView *volumetricFogImageView = registry.getImageView(data.m_volumetricFogImageViewHandle);
				DescriptorBufferInfo punctualLightsMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsBitMaskBufferHandle);
				DescriptorBufferInfo punctualLightsShadowedMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsShadowedBitMaskBufferHandle);
				DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

				DescriptorSetUpdate2 updates[] =
				{
					Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::structuredBuffer(&data.m_particleBufferInfo, PARTICLES_BINDING),
					Initializers::texture(&volumetricFogImageView, VOLUMETRIC_FOG_IMAGE_BINDING),
					Initializers::texture(&shadowSpaceImageView, SHADOW_IMAGE_BINDING),
					Initializers::texture(&shadowAtlasImageViewHandle, SHADOW_ATLAS_IMAGE_BINDING),
					Initializers::texture(&fomImageViewHandle, FOM_IMAGE_BINDING),
					Initializers::texture(&fomDirectionalImageView, FOM_DIRECTIONAL_IMAGE_BINDING),
					Initializers::texture(&fomDirectionalDepthRangeImageView, FOM_DIRECTIONAL_DEPTH_RANGE_IMAGE_BINDING),
					Initializers::structuredBuffer(&data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING),
					Initializers::structuredBuffer(&data.m_directionalLightsBufferInfo, DIRECTIONAL_LIGHTS_BINDING),
					Initializers::structuredBuffer(&data.m_directionalLightsShadowedBufferInfo, DIRECTIONAL_LIGHTS_SHADOWED_BINDING),
					Initializers::structuredBuffer(&data.m_punctualLightsBufferInfo, PUNCTUAL_LIGHTS_BINDING),
					Initializers::byteBuffer(&data.m_punctualLightsZBinsBufferInfo, PUNCTUAL_LIGHTS_Z_BINS_BINDING),
					Initializers::byteBuffer(&punctualLightsMaskBufferInfo, PUNCTUAL_LIGHTS_BIT_MASK_BINDING),
					Initializers::structuredBuffer(&data.m_punctualLightsShadowedBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BINDING),
					Initializers::byteBuffer(&data.m_punctualLightsShadowedZBinsBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING),
					Initializers::byteBuffer(&punctualLightsShadowedMaskBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING),
					Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
					Initializers::texture(&data.m_blueNoiseImageView, BLUE_NOISE_IMAGE_BINDING),
				};

				descriptorSet->update((uint32_t)std::size(updates), updates);
			}

			DescriptorSet *sets[]
			{
				descriptorSet,
				data.m_passRecordContext->m_renderResources->m_textureDescriptorSet,
				data.m_passRecordContext->m_renderResources->m_samplerDescriptorSet,
				data.m_passRecordContext->m_renderResources->m_shadowSamplerDescriptorSet
			};
			cmdList->bindDescriptorSets(pipeline, 0, 4, sets);

			Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
			Rect scissor{ { 0, 0 }, { width, height } };

			cmdList->setViewport(0, 1, &viewport);
			cmdList->setScissor(0, 1, &scissor);

			uint32_t particleOffset = 0;
			for (size_t i = 0; i < data.m_listCount; ++i)
			{
				if (data.m_listSizes[i])
				{
					cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(particleOffset), &particleOffset);
					cmdList->draw(6 * data.m_listSizes[i], 1, 0, 0);
					particleOffset += data.m_listSizes[i];
				}
			}

			cmdList->endRenderPass();
		});
}
