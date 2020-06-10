#include "ParticlesPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"
#include "Graphics/ParticleDrawData.h"

using namespace VEngine::gal;

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
	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	{
		auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

		uint8_t *uboDataPtr = nullptr;
		uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

		Constants consts;
		consts.viewProjectionMatrix = commonData->m_jitteredViewProjectionMatrix;
		consts.cameraPosition = commonData->m_cameraPosition;
		consts.cameraUp = glm::vec3(commonData->m_viewMatrix[0][1], commonData->m_viewMatrix[1][1], commonData->m_viewMatrix[2][1]);

		memcpy(uboDataPtr, &consts, sizeof(consts));
	}
	
	// particle buffer
	DescriptorBufferInfo particleBufferInfo{ nullptr, 0, sizeof(ParticleDrawData) * totalParticleCount };
	{
		auto *storageBuffer = data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[commonData->m_curResIdx].get();

		uint8_t *particleDataPtr = nullptr;
		storageBuffer->allocate(particleBufferInfo.m_range, particleBufferInfo.m_offset, particleBufferInfo.m_buffer, particleDataPtr);

		for (size_t i = 0; i < data.m_listCount; ++i)
		{
			memcpy(particleDataPtr, data.m_particleLists[i], data.m_listSizes[i] * sizeof(ParticleDrawData));
			particleDataPtr += data.m_listSizes[i] * sizeof(ParticleDrawData);
		}
	}

	rg::ResourceUsageDescription passUsages[]
	{
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

			gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };
			
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
			builder.setVertexShader("Resources/Shaders/hlsl/particles_vs.spv");
			builder.setFragmentShader("Resources/Shaders/hlsl/particles_ps.spv");
			builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
			builder.setDepthTest(true, false, CompareOp::GREATER_OR_EQUAL);
			builder.setColorBlendAttachment(blendState);
			builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
			builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageViewHandle)->getImage()->getDescription().m_format);
			builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				DescriptorSetUpdate updates[] =
				{
					Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::storageBuffer(&particleBufferInfo, PARTICLES_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);
			}

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);

			Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
			Rect scissor{ { 0, 0 }, { width, height } };

			cmdList->setViewport(0, 1, &viewport);
			cmdList->setScissor(0, 1, &scissor);

			uint32_t particleOffset = 0;
			for (size_t i = 0; i < data.m_listCount; ++i)
			{
				cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, 4, &particleOffset);
				cmdList->draw(6 * data.m_listSizes[i], 1, 0, 0);
				particleOffset += data.m_listSizes[i];
			}

			cmdList->endRenderPass();
		});
}
