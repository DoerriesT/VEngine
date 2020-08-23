#include "ApplyIndirectLightingPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/applyIndirectLighting.hlsli"
}

void VEngine::ApplyIndirectLightingPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {ResourceState::READ_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT} },
		{rg::ResourceViewHandle(data.m_depthImageViewHandle2), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_ssaoImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
		//{rg::ResourceViewHandle(data.m_indirectSpecularLightImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_brdfLutImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_albedoMetalnessImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_normalRoughnessImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_reflectionProbeBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
	};

	graph.addPass("Apply Indirect Lighting", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;


			PipelineColorBlendAttachmentState blendState{};
			blendState.m_blendEnable = true;
			blendState.m_srcColorBlendFactor = BlendFactor::ONE;
			blendState.m_dstColorBlendFactor = BlendFactor::ONE;
			blendState.m_colorBlendOp = BlendOp::ADD;
			blendState.m_srcAlphaBlendFactor = BlendFactor::ONE;
			blendState.m_dstAlphaBlendFactor = BlendFactor::ONE;
			blendState.m_alphaBlendOp = BlendOp::ADD;
			blendState.m_colorWriteMask = ColorComponentFlagBits::ALL_BITS;


			// create pipeline description
			GraphicsPipelineCreateInfo pipelineCreateInfo;
			GraphicsPipelineBuilder builder(pipelineCreateInfo);
			builder.setVertexShader("Resources/Shaders/hlsl/fullscreenTriangle_vs.spv");
			builder.setFragmentShader("Resources/Shaders/hlsl/applyIndirectLighting_ps.spv");
			builder.setColorBlendAttachment(blendState);
			builder.setDepthTest(true, false, CompareOp::NOT_EQUAL);
			builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
			builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageViewHandle)->getImage()->getDescription().m_format);
			builder.setColorAttachmentFormat(registry.getImageView(data.m_resultImageHandle)->getImage()->getDescription().m_format);

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			// begin renderpass
			DepthStencilAttachmentDescription depthAttachDesc =
			{ registry.getImageView(data.m_depthImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {}, true };
			ColorAttachmentDescription colorAttachDesc{ registry.getImageView(data.m_resultImageHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, {} };
			cmdList->beginRenderPass(1, &colorAttachDesc, &depthAttachDesc, { {}, {width, height} });

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *depthImageView = registry.getImageView(data.m_depthImageViewHandle2);
				ImageView *ssaoImageView = registry.getImageView(data.m_ssaoImageViewHandle);
				//ImageView *indirectSpecularImageView = registry.getImageView(data.m_indirectSpecularLightImageViewHandle);
				ImageView *brdfLutImageView = registry.getImageView(data.m_brdfLutImageViewHandle);
				ImageView *albedoMetalnessImageView = registry.getImageView(data.m_albedoMetalnessImageViewHandle);
				ImageView *normalRoughnessImageView = registry.getImageView(data.m_normalRoughnessImageViewHandle);
				DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);
				DescriptorBufferInfo reflProbeMaskBufferInfo = registry.getBufferInfo(data.m_reflectionProbeBitMaskBufferHandle);

				DescriptorSetUpdate2 updates[] =
				{
					Initializers::texture(&depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::texture(&ssaoImageView, SSAO_IMAGE_BINDING),
					//Initializers::texture(&indirectSpecularImageView, INDIRECT_SPECULAR_LIGHT_IMAGE_BINDING),
					Initializers::texture(&brdfLutImageView, BRDF_LUT_IMAGE_BINDING),
					Initializers::texture(&albedoMetalnessImageView, ALBEDO_METALNESS_IMAGE_BINDING),
					Initializers::texture(&normalRoughnessImageView, NORMAL_ROUGHNESS_IMAGE_BINDING),
					Initializers::texture(&data.m_reflectionProbeImageView, REFLECTION_PROBE_IMAGE_BINDING),
					Initializers::structuredBuffer(&data.m_reflectionProbeDataBufferInfo, REFLECTION_PROBE_DATA_BINDING),
					Initializers::byteBuffer(&reflProbeMaskBufferInfo, REFLECTION_PROBE_BIT_MASK_BINDING),
					Initializers::byteBuffer(&data.m_reflectionProbeZBinsBufferInfo, REFLECTION_PROBE_Z_BINS_BINDING),
					Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
				};

				descriptorSet->update((uint32_t)std::size(updates), updates);

				DescriptorSet *sets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_samplerDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
			}

			Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
			Rect scissor{ { 0, 0 }, { width, height } };

			cmdList->setViewport(0, 1, &viewport);
			cmdList->setScissor(0, 1, &scissor);

			const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;

			PushConsts pushConsts;
			pushConsts.invViewMatrix = data.m_passRecordContext->m_commonRenderData->m_invViewMatrix;
			pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
			pushConsts.width = width;
			pushConsts.texelWidth = 1.0f / width;
			pushConsts.texelHeight = 1.0f / height;
			pushConsts.probeCount = data.m_passRecordContext->m_commonRenderData->m_reflectionProbeCount;
			pushConsts.ssao = data.m_ssao;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->draw(3, 1, 0, 0);

			cmdList->endRenderPass();
		});
}
