#include "DebugDrawPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

void VEngine::DebugDrawPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;

	uint32_t vertexCount = 0;
	for (size_t i = 0; i < 6; ++i)
	{
		vertexCount += data.m_debugDrawData->m_vertexCounts[i];
	}

	if (vertexCount == 0)
	{
		return;
	}

	// debug vertex buffer
	DescriptorBufferInfo vertexBufferInfo{ nullptr, 0, vertexCount * sizeof(DebugDrawVertex) };
	{
		auto *ssboBuffer = data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[commonData->m_curResIdx].get();

		uint8_t *dataPtr = nullptr;
		ssboBuffer->allocate(vertexBufferInfo.m_range, vertexBufferInfo.m_offset, vertexBufferInfo.m_buffer, dataPtr);

		size_t currentOffset = 0;
		for (size_t i = 0; i < 6; ++i)
		{
			if (data.m_debugDrawData->m_vertexCounts[i])
			{
				memcpy(dataPtr + currentOffset, data.m_debugDrawData->m_vertices[i], data.m_debugDrawData->m_vertexCounts[i] * sizeof(DebugDrawVertex));
				currentOffset += data.m_debugDrawData->m_vertexCounts[i] * sizeof(DebugDrawVertex);
			}
		}
	}

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {gal::ResourceState::READ_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT }},
	};

	graph.addPass("Debug Draw", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

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

			Format depthAttachmentFormat = registry.getImageView(data.m_depthImageViewHandle)->getImage()->getDescription().m_format;

			DepthStencilAttachmentDescription depthAttachDesc =
			{ registry.getImageView(data.m_depthImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {}, true };
			ColorAttachmentDescription colorAttachDescs[]
			{
				{registry.getImageView(data.m_resultImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, {} },
			};

			DescriptorSet *descriptorSet = nullptr;

			uint32_t currentVertexOffset = 0;

			for (size_t i = 0; i < 6; ++i)
			{
				if (data.m_debugDrawData->m_vertexCounts[i] == 0)
				{
					continue;
				}

				bool depthTestEnabled = i != 0 && i != 3;
				bool depthTestVisible = (i == 1 || i == 4);
				bool depthTestHidden = (i == 2 || i == 5);

				// begin renderpass
				cmdList->beginRenderPass(1, colorAttachDescs, depthTestEnabled ? &depthAttachDesc : nullptr, { {}, {width, height} });
				{
					// create pipeline description
					GraphicsPipelineCreateInfo pipelineCreateInfo;
					GraphicsPipelineBuilder builder(pipelineCreateInfo);
					builder.setVertexShader("Resources/Shaders/hlsl/debugDraw_vs.spv");
					builder.setFragmentShader("Resources/Shaders/hlsl/debugDraw_ps.spv");
					builder.setInputAssemblyState(i < 3 ? PrimitiveTopology::LINE_LIST : PrimitiveTopology::TRIANGLE_LIST, false);
					builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
					builder.setDepthTest(depthTestEnabled, false, depthTestVisible ? CompareOp::GREATER_OR_EQUAL : depthTestHidden ? CompareOp::LESS : CompareOp::ALWAYS);
					builder.setColorBlendAttachment(blendState);
					builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
					if (depthTestEnabled)
					{
						builder.setDepthStencilAttachmentFormat(depthAttachmentFormat);
					}
					builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

					auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

					cmdList->bindPipeline(pipeline);

					// retrieve and initialize descriptor set only once: all pipelines can share the same set
					if (!descriptorSet)
					{
						descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

						DescriptorSetUpdate updates[] =
						{
							Initializers::storageBuffer(&vertexBufferInfo, 0),
						};

						descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);
					}

					cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);

					Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
					Rect scissor{ { 0, 0 }, { width, height } };

					cmdList->setViewport(0, 1, &viewport);
					cmdList->setScissor(0, 1, &scissor);

					cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(glm::mat4), &data.m_passRecordContext->m_commonRenderData->m_viewProjectionMatrix);
					cmdList->draw(data.m_debugDrawData->m_vertexCounts[i], 1, currentVertexOffset, 0);
					currentVertexOffset += data.m_debugDrawData->m_vertexCounts[i];
				}
				cmdList->endRenderPass();
			}
		});
}
