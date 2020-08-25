#include "ProbeGBufferPass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/probeGBuffer.hlsli"
}


void VEngine::ProbeGBufferPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint64_t alignment = graph.getGraphicsDevice()->getBufferAlignment(DescriptorType2::CONSTANT_BUFFER, sizeof(Constants));
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(alignment, uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);
	memcpy(uboDataPtr, &data.m_viewProjectionMatrices, sizeof(Constants));


	graph.addPass("Probe G-Buffer", rg::QueueType::GRAPHICS, 0, nullptr, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// transition images to correct state for writing in this pass
			{
				gal::Barrier barriers[3];
				{
					barriers[0] = Initializers::imageBarrier(data.m_depthImageViews[0]->getImage(),
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT,
						ResourceState::READ_TEXTURE,
						ResourceState::READ_WRITE_DEPTH_STENCIL,
						{ 0, 1, data.m_depthImageViews[0]->getDescription().m_baseArrayLayer, 6 });

					barriers[1] = Initializers::imageBarrier(data.m_albedoRoughnessImageViews[0]->getImage(),
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT,
						ResourceState::READ_TEXTURE,
						ResourceState::WRITE_ATTACHMENT,
						{ 0, 1, data.m_albedoRoughnessImageViews[0]->getDescription().m_baseArrayLayer, 6 });

					barriers[2] = Initializers::imageBarrier(data.m_normalImageViews[0]->getImage(),
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT,
						ResourceState::READ_TEXTURE,
						ResourceState::WRITE_ATTACHMENT,
						{ 0, 1, data.m_normalImageViews[0]->getDescription().m_baseArrayLayer, 6 });
				}
				cmdList->barrier(3, barriers);
			}

			const uint32_t width = RendererConsts::REFLECTION_PROBE_RES;;
			const uint32_t height = RendererConsts::REFLECTION_PROBE_RES;;

			Format depthAttachmentFormat = data.m_depthImageViews[0]->getImage()->getDescription().m_format;
			Format colorAttachmentFormats[]
			{
				data.m_albedoRoughnessImageViews[0]->getImage()->getDescription().m_format,
				data.m_normalImageViews[0]->getImage()->getDescription().m_format,
			};
			PipelineColorBlendAttachmentState colorBlendAttachments[]
			{
				GraphicsPipelineBuilder::s_defaultBlendAttachment,
				GraphicsPipelineBuilder::s_defaultBlendAttachment,
			};

			for (int face = 0; face < 6; ++face)
			{
				// begin renderpass
				DepthStencilAttachmentDescription depthAttachmentDesc{ data.m_depthImageViews[face] };
				depthAttachmentDesc.m_loadOp = AttachmentLoadOp::CLEAR;
				depthAttachmentDesc.m_storeOp = AttachmentStoreOp::STORE;
				depthAttachmentDesc.m_stencilLoadOp = AttachmentLoadOp::DONT_CARE;
				depthAttachmentDesc.m_stencilStoreOp = AttachmentStoreOp::DONT_CARE;
				depthAttachmentDesc.m_clearValue = { 1.0f, 0 };
				depthAttachmentDesc.m_readOnly = false;

				ColorAttachmentDescription albedoRoughAttachmentDesc{ data.m_albedoRoughnessImageViews[face], AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} };
				ColorAttachmentDescription normalAttachmentDesc{ data.m_normalImageViews[face], AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} };
				ColorAttachmentDescription colorAttachmentDescs[] = { albedoRoughAttachmentDesc, normalAttachmentDesc };
				cmdList->beginRenderPass(2, colorAttachmentDescs, &depthAttachmentDesc, { {}, {width, height} });

				for (int i = 0; i < 2; ++i)
				{
					const bool alphaMasked = i == 1;

					// create pipeline description
					GraphicsPipelineCreateInfo pipelineCreateInfo;
					GraphicsPipelineBuilder builder(pipelineCreateInfo);
					builder.setVertexShader("Resources/Shaders/hlsl/probeGBuffer_vs");
					builder.setFragmentShader(alphaMasked ? "Resources/Shaders/hlsl/probeGBuffer_ALPHA_MASK_ENABLED_ps" : "Resources/Shaders/hlsl/probeGBuffer_ps");
					builder.setPolygonModeCullMode(PolygonMode::FILL, alphaMasked ? CullModeFlagBits::NONE : CullModeFlagBits::BACK_BIT, FrontFace::CLOCKWISE);
					builder.setDepthTest(true, true, CompareOp::LESS_OR_EQUAL);
					builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
					builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
					builder.setDepthStencilAttachmentFormat(depthAttachmentFormat);
					builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

					auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

					cmdList->bindPipeline(pipeline);

					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					// update descriptor sets
					{
						DescriptorBufferInfo positionsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), 4u };
						DescriptorBufferInfo tangentsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * sizeof(VertexQTangent), 4u };
						DescriptorBufferInfo texCoordsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexQTangent)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord), 4u };

						DescriptorSetUpdate2 updates[] =
						{
							Initializers::structuredBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),
							Initializers::structuredBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
							Initializers::structuredBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
							Initializers::structuredBuffer(&tangentsBufferInfo, VERTEX_QTANGENTS_BINDING),
							Initializers::structuredBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
							Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
						};

						descriptorSet->update((uint32_t)std::size(updates), updates);
					}

					DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet, data.m_passRecordContext->m_renderResources->m_samplerDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 3, descriptorSets);

					Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
					Rect scissor{ { 0, 0 }, { width, height } };

					cmdList->setViewport(0, 1, &viewport);
					cmdList->setScissor(0, 1, &scissor);

					cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_indexBuffer, 0, IndexType::UINT16);

					const uint32_t instanceDataCount = alphaMasked ? data.m_maskedInstanceDataCount[face] : data.m_opaqueInstanceDataCount[face];
					const uint32_t instanceDataOffset = alphaMasked ? data.m_maskedInstanceDataOffset[face] : data.m_opaqueInstanceDataOffset[face];

					for (uint32_t i = 0; i < instanceDataCount; ++i)
					{
						const auto &instanceData = data.m_instanceData[i + instanceDataOffset];
						const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

						PushConsts pushConsts;
						pushConsts.texCoordScale = float2(data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 0], data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 1]);
						pushConsts.texCoordBias = float2(data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 2], data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 3]);
						pushConsts.transformIndex = instanceData.m_transformIndex;
						pushConsts.materialIndex = instanceData.m_materialIndex;
						pushConsts.face = face;
						pushConsts.vertexOffset = subMeshInfo.m_vertexOffset;

						cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

						cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, 0, 0);
					}
				}

				cmdList->endRenderPass();
			}

			// transition images to correct state for reading in lighting pass
			{
				gal::Barrier barriers[3];
				{
					barriers[0] = Initializers::imageBarrier(data.m_depthImageViews[0]->getImage(),
						PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT,
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						ResourceState::READ_WRITE_DEPTH_STENCIL,
						ResourceState::READ_TEXTURE,
						{ 0, 1, data.m_depthImageViews[0]->getDescription().m_baseArrayLayer, 6 });

					barriers[1] = Initializers::imageBarrier(data.m_albedoRoughnessImageViews[0]->getImage(),
						PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT,
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						ResourceState::WRITE_ATTACHMENT,
						ResourceState::READ_TEXTURE,
						{ 0, 1, data.m_albedoRoughnessImageViews[0]->getDescription().m_baseArrayLayer, 6 });

					barriers[2] = Initializers::imageBarrier(data.m_normalImageViews[0]->getImage(),
						PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT,
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						ResourceState::WRITE_ATTACHMENT,
						ResourceState::READ_TEXTURE,
						{ 0, 1, data.m_normalImageViews[0]->getDescription().m_baseArrayLayer, 6 });
				}
				cmdList->barrier(3, barriers);
			}
		}, true);
}
