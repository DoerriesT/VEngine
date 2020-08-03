#include "DepthPrepassPass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/depthPrepass.hlsli"
}

void VEngine::DepthPrepassPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		//{rg::ResourceViewHandle(data.m_indicesBufferHandle), {gal::ResourceState::READ_INDEX_BUFFER, PipelineStageFlagBits::VERTEX_INPUT_BIT}},
		//{rg::ResourceViewHandle(data.m_indirectBufferHandle), {gal::ResourceState::READ_INDIRECT_BUFFER, PipelineStageFlagBits::DRAW_INDIRECT_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageHandle), {gal::ResourceState::READ_WRITE_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
	};

	graph.addPass("Depth Prepass", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// begin renderpass
			DepthStencilAttachmentDescription depthAttachmentDesc{ registry.getImageView(data.m_depthImageHandle) };
			depthAttachmentDesc.m_loadOp = AttachmentLoadOp::CLEAR;
			depthAttachmentDesc.m_storeOp = AttachmentStoreOp::STORE;
			depthAttachmentDesc.m_stencilLoadOp = AttachmentLoadOp::DONT_CARE;
			depthAttachmentDesc.m_stencilStoreOp = AttachmentStoreOp::DONT_CARE;
			depthAttachmentDesc.m_clearValue = { 0.0f, 0 };
			depthAttachmentDesc.m_readOnly = false;

			cmdList->beginRenderPass(0, nullptr, &depthAttachmentDesc, { {}, {width, height} });

			for (int i = 0; i < 2; ++i)
			{
				const bool alphaMasked = i == 1;

				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);
				gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };
				builder.setVertexShader(alphaMasked ? "Resources/Shaders/hlsl/depthPrepass_ALPHA_MASK_ENABLED_vs.spv" : "Resources/Shaders/hlsl/depthPrepass_vs.spv");
				if (alphaMasked)
				{
					builder.setFragmentShader("Resources/Shaders/hlsl/depthPrepass_ps.spv");
				}
				builder.setPolygonModeCullMode(PolygonMode::FILL, alphaMasked ? CullModeFlagBits::NONE : CullModeFlagBits::BACK_BIT, FrontFace::COUNTER_CLOCKWISE);
				builder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
				builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
				builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageHandle)->getImage()->getDescription().m_format);

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				// update descriptor sets
				{
					DescriptorBufferInfo positionsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition) };
					DescriptorBufferInfo texCoordsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexQTangent)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord) };

					DescriptorSetUpdate updates[] =
					{
						Initializers::storageBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
						Initializers::storageBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),

						// if (data.m_alphaMasked)
						Initializers::storageBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
						Initializers::storageBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
					};

					descriptorSet->update(alphaMasked ? 4 : 2, updates);
				}

				DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, alphaMasked ? 2 : 1, descriptorSets);

				Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
				Rect scissor{ { 0, 0 }, { width, height } };

				cmdList->setViewport(0, 1, &viewport);
				cmdList->setScissor(0, 1, &scissor);

				//cmdList->bindIndexBuffer(registry.getBuffer(data.m_indicesBufferHandle), 0, IndexType::UINT32);
				cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_indexBuffer, 0, IndexType::UINT16);

				PushConsts pushConsts;
				pushConsts.jitteredViewProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredViewProjectionMatrix;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, offsetof(PushConsts, transformIndex), &pushConsts);

				const uint32_t instanceDataCount = alphaMasked ? data.m_maskedInstanceDataCount : data.m_opaqueInstanceDataCount;
				const uint32_t instanceDataOffset = alphaMasked ? data.m_maskedInstanceDataOffset : data.m_opaqueInstanceDataOffset;

				for (uint32_t j = 0; j < instanceDataCount; ++j)
				{
					const auto &instanceData = data.m_instanceData[j + instanceDataOffset];

					pushConsts.transformIndex = instanceData.m_transformIndex;
					pushConsts.materialIndex = instanceData.m_materialIndex;

					if (i == 0)
					{
						cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, offsetof(PushConsts, transformIndex), sizeof(pushConsts) - offsetof(PushConsts, transformIndex), &pushConsts.transformIndex);
					}
					else
					{
						pushConsts.texCoordScale = float2(data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 0], data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 1]);
						pushConsts.texCoordBias = float2(data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 2], data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 3]);

						cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, offsetof(PushConsts, texCoordScale), sizeof(pushConsts) - offsetof(PushConsts, texCoordScale), (uint8_t *)&pushConsts + offsetof(PushConsts, texCoordScale));
					}

					const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

					cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, subMeshInfo.m_vertexOffset, 0);
				}
			}

			cmdList->endRenderPass();
		});
}
