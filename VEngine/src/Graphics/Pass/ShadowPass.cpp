#include "ShadowPass.h"
#include "Graphics/LightData.h"
#include "Graphics/RenderData.h"
#include "Graphics/RenderResources.h"
#include "Graphics/Mesh.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "GlobalVar.h"
#include <glm/vec3.hpp>
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	using namespace glm;
#include "../../../../Application/Resources/Shaders/shadows_bindings.h"
}

void VEngine::ShadowPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		//{ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::READ_INDIRECT_BUFFER},
		//{ResourceViewHandle(data.m_indicesBufferHandle), ResourceState::READ_INDEX_BUFFER},
		{rg::ResourceViewHandle(data.m_shadowImageHandle), {gal::ResourceState::READ_WRITE_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
	};

	graph.addPass("Shadows", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_shadowMapSize;
			const uint32_t height = data.m_shadowMapSize;

			// begin renderpass
			DepthStencilAttachmentDescription depthAttachDesc =
			{ registry.getImageView(data.m_shadowImageHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, { 1.0f, 0 } };
			cmdList->beginRenderPass(0, nullptr, &depthAttachDesc, { {}, {width, height} });

			for (int i = 0; i < 2; ++i)
			{
				const bool alphaMasked = i == 1;

				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);

				gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };

				builder.setVertexShader(alphaMasked ? "Resources/Shaders/shadows_alpha_mask_vert.spv" : "Resources/Shaders/shadows_vert.spv");
				if (alphaMasked) builder.setFragmentShader("Resources/Shaders/shadows_frag.spv");
				builder.setPolygonModeCullMode(PolygonMode::FILL, alphaMasked ? CullModeFlagBits::NONE : CullModeFlagBits::BACK_BIT, FrontFace::COUNTER_CLOCKWISE);
				builder.setDepthTest(true, true, CompareOp::LESS_OR_EQUAL);
				builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
				builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_shadowImageHandle)->getImage()->getDescription().m_format);

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				// update descriptor sets
				{
					Buffer *vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer;
					DescriptorBufferInfo positionsBufferInfo{ vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition) };
					DescriptorBufferInfo texCoordsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord) };

					DescriptorSetUpdate updates[] =
					{
						Initializers::storageBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),
						Initializers::storageBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),

						// if (data.m_alphaMasked)
						Initializers::storageBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
						Initializers::storageBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
					};

					descriptorSet->update(alphaMasked ? 4 : 2, updates);
				}

				DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, alphaMasked ? 2 : 1, descriptorSets);

				Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
				Rect scissor{ { 0, 0 }, { width, height } };

				cmdList->setViewport(0, 1, &viewport);
				cmdList->setScissor(0, 1, &scissor);

				cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_indexBuffer, 0, IndexType::UINT16);

				const uint32_t instanceDataCount = alphaMasked ? data.m_maskedInstanceDataCount : data.m_opaqueInstanceDataCount;
				const uint32_t instanceDataOffset = alphaMasked ? data.m_maskedInstanceDataOffset : data.m_opaqueInstanceDataOffset;

				for (uint32_t j = 0; j < instanceDataCount; ++j)
				{
					const auto &instanceData = data.m_instanceData[j + instanceDataOffset];

					PushConsts pushConsts;
					pushConsts.shadowMatrix = data.m_shadowMatrix;
					pushConsts.transformIndex = instanceData.m_transformIndex;
					pushConsts.materialIndex = instanceData.m_materialIndex;

					cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

					const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

					cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, subMeshInfo.m_vertexOffset, 0);
				}

				//vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(data.m_shadowMatrix), &data.m_shadowMatrix);
				//
				//vkCmdDrawIndexedIndirect(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), 0, 1, sizeof(VkDrawIndexedIndirectCommand));
			}

			cmdList->endRenderPass();
		});
}