#include "VelocityInitializationPass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/velocityInitialization.hlsli"
}

void VEngine::VelocityInitializationPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_depthImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_velocityImageHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
	};

	graph.addPass("Velocity Initialization", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		GraphicsPipelineCreateInfo pipelineCreateInfo;
		GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader("Resources/Shaders/hlsl/fullscreenTriangle_vs.spv");
		builder.setFragmentShader("Resources/Shaders/hlsl/velocityInitialization_ps.spv");
		builder.setColorBlendAttachment(GraphicsPipelineBuilder::s_defaultBlendAttachment);
		builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
		builder.setColorAttachmentFormat(registry.getImageView(data.m_velocityImageHandle)->getImage()->getDescription().m_format);

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		// begin renderpass
		ColorAttachmentDescription colorAttachDesc{ registry.getImageView(data.m_velocityImageHandle), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} };
		cmdList->beginRenderPass(1, &colorAttachDesc, nullptr, { {}, {width, height} });

		cmdList->bindPipeline(pipeline);

		

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *depthImageView = registry.getImageView(data.m_depthImageHandle);

			DescriptorSetUpdate updates[] =
			{
				Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
			};

			descriptorSet->update(1, updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

		Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		Rect scissor{ { 0, 0 }, { width, height } };

		cmdList->setViewport(0, 1, &viewport);
		cmdList->setScissor(0, 1, &scissor);

		const glm::mat4 reprojectionMatrix = data.m_passRecordContext->m_commonRenderData->m_prevViewProjectionMatrix *data.m_passRecordContext->m_commonRenderData->m_invViewProjectionMatrix;

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::FRAGMENT_BIT, 0, sizeof(reprojectionMatrix), &reprojectionMatrix);
		
		cmdList->draw(3, 1, 0, 0);
		
		cmdList->endRenderPass();
	});
}
