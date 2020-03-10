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
	using namespace glm;
#include "../../../../Application/Resources/Shaders/depthPrepass_bindings.h"
}

void VEngine::DepthPrepassPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		//{rg::ResourceViewHandle(data.m_indicesBufferHandle), {gal::ResourceState::READ_INDEX_BUFFER, PipelineStageFlagBits::VERTEX_INPUT_BIT}},
		//{rg::ResourceViewHandle(data.m_indirectBufferHandle), {gal::ResourceState::READ_INDIRECT_BUFFER, PipelineStageFlagBits::DRAW_INDIRECT_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageHandle), {gal::ResourceState::READ_WRITE_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
	};

	graph.addPass(data.m_alphaMasked ? "Depth Prepass Alpha" : "Depth Prepass", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			GraphicsPipelineCreateInfo pipelineCreateInfo;
			GraphicsPipelineBuilder builder(pipelineCreateInfo);
			gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };
			builder.setVertexShader(data.m_alphaMasked ? "Resources/Shaders/depthPrepass_ALPHA_MASK_ENABLED_vert.spv" : "Resources/Shaders/depthPrepass_vert.spv");
			if (data.m_alphaMasked)
			{
				builder.setFragmentShader("Resources/Shaders/depthPrepass_frag.spv");
			}
			builder.setPolygonModeCullMode(PolygonMode::FILL, data.m_alphaMasked ? CullModeFlagBits::NONE : CullModeFlagBits::BACK_BIT, FrontFace::COUNTER_CLOCKWISE);
			builder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
			builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
			builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageHandle)->getImage()->getDescription().m_format);

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			// begin renderpass
			DepthStencilAttachmentDescription depthAttachmentDesc{ registry.getImageView(data.m_depthImageHandle) };
			depthAttachmentDesc.m_loadOp = data.m_alphaMasked ? AttachmentLoadOp::LOAD : AttachmentLoadOp::CLEAR;
			depthAttachmentDesc.m_storeOp = AttachmentStoreOp::STORE;
			depthAttachmentDesc.m_stencilLoadOp = AttachmentLoadOp::DONT_CARE;
			depthAttachmentDesc.m_stencilStoreOp = AttachmentStoreOp::DONT_CARE;
			depthAttachmentDesc.m_clearValue = { 0.0f, 0 };
			depthAttachmentDesc.m_readOnly = false;

			cmdList->beginRenderPass(0, nullptr, &depthAttachmentDesc, { {}, {width, height} });

			cmdList->bindPipeline(pipeline);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				DescriptorBufferInfo positionsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition) };
				DescriptorBufferInfo texCoordsBufferInfo{ data.m_passRecordContext->m_renderResources->m_vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord) };

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
					//Initializers::storageBuffer(&data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING),
					Initializers::storageBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),
					//Initializers::storageBuffer(&data.m_subMeshInfoBufferInfo, SUB_MESH_DATA_BINDING),

					// if (data.m_alphaMasked)
					Initializers::storageBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
					Initializers::storageBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
				};

				descriptorSet->update(data.m_alphaMasked ? 4 : 2, updates);
			}

			DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
			cmdList->bindDescriptorSets(pipeline, 0, data.m_alphaMasked ? 2 : 1, descriptorSets);

			Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
			Rect scissor{ { 0, 0 }, { width, height } };

			cmdList->setViewport(0, 1, &viewport);
			cmdList->setScissor(0, 1, &scissor);

			//cmdList->bindIndexBuffer(registry.getBuffer(data.m_indicesBufferHandle), 0, IndexType::UINT32);
			cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_indexBuffer, 0, IndexType::UINT16);

			PushConsts pushConsts;
			pushConsts.jitteredViewProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredViewProjectionMatrix;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, offsetof(PushConsts, transformIndex), &pushConsts);

			for (uint32_t i = 0; i < data.m_instanceDataCount; ++i)
			{
				const auto &instanceData = data.m_instanceData[i + data.m_instanceDataOffset];

				
				pushConsts.transformIndex = instanceData.m_transformIndex;
				pushConsts.materialIndex = instanceData.m_materialIndex;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, offsetof(PushConsts, transformIndex), sizeof(pushConsts) - offsetof(PushConsts, transformIndex), &pushConsts.transformIndex);

				const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

				cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, subMeshInfo.m_vertexOffset, 0);
			}

			cmdList->endRenderPass();
		});
}
