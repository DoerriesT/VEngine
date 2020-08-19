#include "VolumetricFogApplyPass2.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogApply2.hlsli"
}

void VEngine::VolumetricFogApplyPass2::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_downsampledDepthImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_raymarchedVolumetricsImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_volumetricFogImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
	};

	graph.addPass("Apply Volumetric Fog", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;


			PipelineColorBlendAttachmentState blendState{};
			blendState.m_blendEnable = true;
			blendState.m_srcColorBlendFactor = BlendFactor::ONE;
			blendState.m_dstColorBlendFactor = BlendFactor::SRC_ALPHA;
			blendState.m_colorBlendOp = BlendOp::ADD;
			blendState.m_srcAlphaBlendFactor = BlendFactor::ONE;
			blendState.m_dstAlphaBlendFactor = BlendFactor::SRC_ALPHA;
			blendState.m_alphaBlendOp = BlendOp::ADD;
			blendState.m_colorWriteMask = ColorComponentFlagBits::ALL_BITS;


			// create pipeline description
			GraphicsPipelineCreateInfo pipelineCreateInfo;
			GraphicsPipelineBuilder builder(pipelineCreateInfo);
			builder.setVertexShader("Resources/Shaders/hlsl/fullscreenTriangle_vs.spv");
			builder.setFragmentShader("Resources/Shaders/hlsl/volumetricFogApply_ps.spv");
			builder.setColorBlendAttachment(blendState);
			builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
			builder.setColorAttachmentFormat(registry.getImageView(data.m_resultImageHandle)->getImage()->getDescription().m_format);

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			// begin renderpass
			ColorAttachmentDescription colorAttachDesc{ registry.getImageView(data.m_resultImageHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, {} };
			cmdList->beginRenderPass(1, &colorAttachDesc, nullptr, { {}, {width, height} });

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *depthImageView = registry.getImageView(data.m_depthImageViewHandle);
				ImageView *downsampledDepthImageView = registry.getImageView(data.m_downsampledDepthImageViewHandle);
				ImageView *raymarchedVolumetricsImageView = registry.getImageView(data.m_raymarchedVolumetricsImageViewHandle);
				ImageView *volumetricFogImageView = registry.getImageView(data.m_volumetricFogImageViewHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&downsampledDepthImageView, RAYMARCHED_VOLUMETRICS_DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&volumetricFogImageView, VOLUMETRIC_FOG_IMAGE_BINDING),
					Initializers::sampledImage(&raymarchedVolumetricsImageView, RAYMARCHED_VOLUMETRICS_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
					Initializers::sampledImage(&data.m_blueNoiseImageView, BLUE_NOISE_IMAGE_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;

			PushConsts pushConsts;
			pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
			pushConsts.frame = data.m_passRecordContext->m_commonRenderData->m_frame;
			pushConsts.texelWidth = 1.0f / width;
			pushConsts.texelHeight = 1.0f / height;
			pushConsts.raymarchedFog = g_raymarchedFog;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->draw(3, 1, 0, 0);

			cmdList->endRenderPass();
		});
}
