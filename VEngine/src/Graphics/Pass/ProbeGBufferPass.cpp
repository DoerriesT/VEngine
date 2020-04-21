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
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);
	memcpy(uboDataPtr, &data.m_viewProjectionMatrices, sizeof(Constants));


	rg::ResourceUsageDescription passUsages[3 * 6];
	for (size_t i = 0; i < 6; ++i)
	{
		passUsages[i * 3 + 0] = { rg::ResourceViewHandle(data.m_depthImageHandles[i]), {gal::ResourceState::READ_WRITE_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT} };
		passUsages[i * 3 + 1] = { rg::ResourceViewHandle(data.m_albedoRoughnessImageHandles[i]), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT} };
		passUsages[i * 3 + 2] = { rg::ResourceViewHandle(data.m_normalImageHandles[i]), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT} };
	}

	graph.addPass("Probe G-Buffer", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = 256;
			const uint32_t height = 256;

			Format depthAttachmentFormat = registry.getImageView(data.m_depthImageHandles[0])->getImage()->getDescription().m_format;
			Format colorAttachmentFormats[]
			{
				registry.getImageView(data.m_albedoRoughnessImageHandles[0])->getImage()->getDescription().m_format,
				registry.getImageView(data.m_normalImageHandles[0])->getImage()->getDescription().m_format,
			};
			PipelineColorBlendAttachmentState colorBlendAttachments[]
			{
				GraphicsPipelineBuilder::s_defaultBlendAttachment,
				GraphicsPipelineBuilder::s_defaultBlendAttachment,
			};

			for (int face = 0; face < 6; ++face)
			{
				// begin renderpass
				DepthStencilAttachmentDescription depthAttachmentDesc{ registry.getImageView(data.m_depthImageHandles[face]) };
				depthAttachmentDesc.m_loadOp = AttachmentLoadOp::CLEAR;
				depthAttachmentDesc.m_storeOp = AttachmentStoreOp::STORE;
				depthAttachmentDesc.m_stencilLoadOp = AttachmentLoadOp::DONT_CARE;
				depthAttachmentDesc.m_stencilStoreOp = AttachmentStoreOp::DONT_CARE;
				depthAttachmentDesc.m_clearValue = { 1.0f, 0 };
				depthAttachmentDesc.m_readOnly = false;

				ColorAttachmentDescription albedoRoughAttachmentDesc{ registry.getImageView(data.m_albedoRoughnessImageHandles[face]), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} };
				ColorAttachmentDescription normalAttachmentDesc{ registry.getImageView(data.m_normalImageHandles[face]), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} };
				ColorAttachmentDescription colorAttachmentDescs[] = { albedoRoughAttachmentDesc, normalAttachmentDesc };
				cmdList->beginRenderPass(2, colorAttachmentDescs, &depthAttachmentDesc, { {}, {width, height} });

				for (int i = 0; i < 2; ++i)
				{
					const bool alphaMasked = i == 1;

					// create pipeline description
					GraphicsPipelineCreateInfo pipelineCreateInfo;
					GraphicsPipelineBuilder builder(pipelineCreateInfo);
					gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };
					builder.setVertexShader("Resources/Shaders/hlsl/probeGBuffer_vs.spv");
					builder.setFragmentShader(alphaMasked ? "Resources/Shaders/hlsl/probeGBuffer_ALPHA_MASK_ENABLED_ps.spv" : "Resources/Shaders/hlsl/probeGBuffer_ps.spv");
					builder.setPolygonModeCullMode(PolygonMode::FILL, alphaMasked ? CullModeFlagBits::NONE : CullModeFlagBits::BACK_BIT, FrontFace::CLOCKWISE);
					builder.setDepthTest(true, true, CompareOp::LESS_OR_EQUAL);
					builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
					builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
					builder.setDepthStencilAttachmentFormat(depthAttachmentFormat);
					builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

					auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

					cmdList->bindPipeline(pipeline);

					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					// update descriptor sets
					{
						DescriptorBufferInfo positionsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition) };
						DescriptorBufferInfo normalsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * sizeof(VertexNormal) };
						DescriptorBufferInfo texCoordsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord) };

						DescriptorSetUpdate updates[] =
						{
							Initializers::storageBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),
							Initializers::storageBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
							Initializers::storageBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
							Initializers::storageBuffer(&normalsBufferInfo, VERTEX_NORMALS_BINDING),
							Initializers::storageBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
							Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
						};

						descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);
					}

					DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);

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

						PushConsts pushConsts;
						pushConsts.transformIndex = instanceData.m_transformIndex;
						pushConsts.materialIndex = instanceData.m_materialIndex;
						pushConsts.face = face;

						cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

						const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

						cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, subMeshInfo.m_vertexOffset, 0);
					}
				}

				cmdList->endRenderPass();
			}
		}, true);
}
