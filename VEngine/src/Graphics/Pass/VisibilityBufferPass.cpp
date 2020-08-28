#include "VisibilityBufferPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/Mesh.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/visibilityBuffer.hlsli"
}

void VEngine::VisibilityBufferPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants), sizeof(Constants) };
	uint64_t alignment = graph.getGraphicsDevice()->getBufferAlignment(DescriptorType2::CONSTANT_BUFFER, sizeof(Constants));
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(alignment, uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	Constants consts;
	consts.jitteredViewProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredViewProjectionMatrix;

	memcpy(uboDataPtr, &consts, sizeof(consts));


	rg::ResourceUsageDescription passUsages[]
	{
		//{rg::ResourceViewHandle(data.m_indicesBufferHandle), {gal::ResourceState::READ_INDEX_BUFFER, PipelineStageFlagBits::VERTEX_INPUT_BIT}},
		//{rg::ResourceViewHandle(data.m_indirectBufferHandle), {gal::ResourceState::READ_INDIRECT_BUFFER, PipelineStageFlagBits::DRAW_INDIRECT_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageHandle), {gal::ResourceState::READ_WRITE_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
		{rg::ResourceViewHandle(data.m_triangleImageHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
	};

	graph.addPass("Visibility Buffer", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// begin renderpass
			DepthStencilAttachmentDescription depthAttachDesc =
			{ registry.getImageView(data.m_depthImageHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {} };
			ColorAttachmentDescription colorAttachDesc = { registry.getImageView(data.m_triangleImageHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, { {1.0f, 1.0f, 1.0f, 1.0f} } };

			cmdList->beginRenderPass(1, &colorAttachDesc, &depthAttachDesc, { {}, {width, height} }, false);

			for (int i = 0; i < 2; ++i)
			{
				const bool alphaMasked = i == 1;
				const uint32_t instanceDataCount = alphaMasked ? data.m_maskedInstanceDataCount : data.m_opaqueInstanceDataCount;
				const uint32_t instanceDataOffset = alphaMasked ? data.m_maskedInstanceDataOffset : data.m_opaqueInstanceDataOffset;

				if (instanceDataCount == 0)
				{
					continue;
				}

				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);
				builder.setVertexShader(alphaMasked ? "Resources/Shaders/hlsl/visibilityBuffer_ALPHA_MASK_ENABLED_vs" : "Resources/Shaders/hlsl/visibilityBuffer_vs");
				builder.setFragmentShader(alphaMasked ? "Resources/Shaders/hlsl/visibilityBuffer_ALPHA_MASK_ENABLED_ps" : "Resources/Shaders/hlsl/visibilityBuffer_ps");
				builder.setPolygonModeCullMode(PolygonMode::FILL, alphaMasked ? CullModeFlagBits::NONE : CullModeFlagBits::BACK_BIT, FrontFace::COUNTER_CLOCKWISE);
				builder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
				builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
				builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageHandle)->getImage()->getDescription().m_format);
				builder.setColorBlendAttachment(GraphicsPipelineBuilder::s_defaultBlendAttachment);
				builder.setColorAttachmentFormat(registry.getImageViewDescription(data.m_triangleImageHandle).m_format);
				//builder.setMultisampleState(SampleCount::_4, false, 0.0f, 0xFFFFFFFF, false, false);

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				// update descriptor sets
				{
					DescriptorBufferInfo positionsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), 4u };
					DescriptorBufferInfo texCoordsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexQTangent)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord), 4u };

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
						Initializers::structuredBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
						Initializers::structuredBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),

						// if (data.m_alphaMasked)
						Initializers::structuredBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
						Initializers::structuredBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
					};

					descriptorSet->update(alphaMasked ? 5 : 3, updates);
				}

				DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet, data.m_passRecordContext->m_renderResources->m_samplerDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, alphaMasked ? 3 : 1, descriptorSets);

				Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
				Rect scissor{ { 0, 0 }, { width, height } };

				cmdList->setViewport(0, 1, &viewport);
				cmdList->setScissor(0, 1, &scissor);

				//cmdList->bindIndexBuffer(registry.getBuffer(data.m_indicesBufferHandle), 0, IndexType::UINT32);
				cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_indexBuffer, 0, IndexType::UINT16);

				
				for (uint32_t j = 0; j < instanceDataCount; ++j)
				{
					const auto &instanceData = data.m_instanceData[j + instanceDataOffset];
					const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

					PushConsts pushConsts;
					pushConsts.transformIndex = instanceData.m_transformIndex;
					pushConsts.materialIndex = instanceData.m_materialIndex;
					pushConsts.instanceID = j + instanceDataOffset;
					pushConsts.vertexOffset = subMeshInfo.m_vertexOffset;

					if (i == 0)
					{
						cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, offsetof(PushConsts, transformIndex), sizeof(pushConsts) - offsetof(PushConsts, transformIndex), &pushConsts.transformIndex);
					}
					else
					{
						pushConsts.texCoordScale = float2(data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 0], data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 1]);
						pushConsts.texCoordBias = float2(data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 2], data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 3]);

						cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT,0, sizeof(pushConsts), &pushConsts);
					}

					cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, 0, 0);
				}
			}

			cmdList->endRenderPass();
		}, true);
}
