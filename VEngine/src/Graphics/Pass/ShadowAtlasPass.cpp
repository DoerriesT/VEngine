#include "ShadowAtlasPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/Mesh.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"
#include "Graphics/ViewRenderList.h"
#include "GlobalVar.h"
#include <glm/vec3.hpp>
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/shadows.hlsli"
}

void VEngine::ShadowAtlasPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_shadowAtlasImageViewHandle), {gal::ResourceState::READ_WRITE_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
	};

	graph.addPass("Shadow Atlas", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_shadowMapSize;
			const uint32_t height = data.m_shadowMapSize;
			gal::ImageView *shadowAtlasImageView = registry.getImageView(data.m_shadowAtlasImageViewHandle);

			// get pipelines and update descriptor sets
			gal::GraphicsPipeline *pipelines[2];
			gal::DescriptorSet *sets[2];
			for (size_t i = 0; i < 2; ++i)
			{
				const bool alphaMasked = i == 1;

				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);

				gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };

				builder.setVertexShader(alphaMasked ? "Resources/Shaders/hlsl/shadows_ALPHA_MASK_ENABLED_vs.spv" : "Resources/Shaders/hlsl/shadows_vs.spv");
				if (alphaMasked) builder.setFragmentShader("Resources/Shaders/hlsl/shadows_ps.spv");
				builder.setPolygonModeCullMode(PolygonMode::FILL, alphaMasked ? CullModeFlagBits::NONE : CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
				builder.setDepthTest(true, true, CompareOp::LESS_OR_EQUAL);
				builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
				builder.setDepthStencilAttachmentFormat(shadowAtlasImageView->getImage()->getDescription().m_format);

				pipelines[i] = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				sets[i] = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelines[i]->getDescriptorSetLayout(0));

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

					sets[i]->update(alphaMasked ? 4 : 2, updates);
				}
			}

			cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_indexBuffer, 0, IndexType::UINT16);

			for (size_t i = 0; i < data.m_drawInfoCount; ++i)
			{
				const auto &drawInfo = data.m_shadowAtlasDrawInfo[i];
				const auto &renderList = data.m_renderLists[drawInfo.m_drawListIdx];

				Rect renderArea = { {static_cast<int32_t>(drawInfo.m_offsetX), static_cast<int32_t>(drawInfo.m_offsetY)}, {drawInfo.m_size, drawInfo.m_size} };

				// begin renderpass
				DepthStencilAttachmentDescription depthAttachDesc =
				{ shadowAtlasImageView, AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, { 1.0f, 0 } };
				cmdList->beginRenderPass(0, nullptr, &depthAttachDesc, renderArea);

				for (size_t j = 0; j < 2; ++j)
				{
					const bool alphaMasked = j == 1;

					cmdList->bindPipeline(pipelines[j]);

					DescriptorSet *descriptorSets[] = { sets[j], data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
					cmdList->bindDescriptorSets(pipelines[j], 0, alphaMasked ? 2 : 1, descriptorSets);

					Viewport viewport{ static_cast<float>(drawInfo.m_offsetX), static_cast<float>(drawInfo.m_offsetY), static_cast<float>(drawInfo.m_size), static_cast<float>(drawInfo.m_size), 0.0f, 1.0f };

					cmdList->setViewport(0, 1, &viewport);
					cmdList->setScissor(0, 1, &renderArea);

					
					const uint32_t instanceDataCount = alphaMasked ? renderList.m_maskedCount : renderList.m_opaqueCount;
					const uint32_t instanceDataOffset = alphaMasked ? renderList.m_maskedOffset : renderList.m_opaqueOffset;

					for (uint32_t k = 0; k < instanceDataCount; ++k)
					{
						const auto &instanceData = data.m_instanceData[k + instanceDataOffset];

						PushConsts pushConsts;
						pushConsts.shadowMatrix = data.m_shadowMatrices[drawInfo.m_shadowMatrixIdx];
						pushConsts.transformIndex = instanceData.m_transformIndex;
						pushConsts.materialIndex = instanceData.m_materialIndex;

						cmdList->pushConstants(pipelines[j], ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

						const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

						cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, subMeshInfo.m_vertexOffset, 0);
					}
				}

				cmdList->endRenderPass();
			}
		});
}
