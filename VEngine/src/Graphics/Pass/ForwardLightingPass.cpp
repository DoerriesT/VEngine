#include "ForwardLightingPass.h"
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
#include "../../../../Application/Resources/Shaders/forward_bindings.h"
}

void VEngine::ForwardLightingPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		//{rg::ResourceViewHandle(data.m_indicesBufferHandle), {gal::ResourceState::READ_INDEX_BUFFER, PipelineStageFlagBits::VERTEX_INPUT_BIT}},
		//{rg::ResourceViewHandle(data.m_indirectBufferHandle), {gal::ResourceState::READ_INDIRECT_BUFFER, PipelineStageFlagBits::DRAW_INDIRECT_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {gal::ResourceState::READ_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT }},
		{rg::ResourceViewHandle(data.m_normalImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_specularRoughnessImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT} },
		{rg::ResourceViewHandle(data.m_pointLightBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_spotLightBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_deferredShadowImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_volumetricFogImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_ssaoImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
	};

	uint32_t usageCount = data.m_ssao ? sizeof(passUsages) / sizeof(passUsages[0]) : sizeof(passUsages) / sizeof(passUsages[0]) - 1;

	graph.addPass("Forward Lighting", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// begin renderpass
			DepthStencilAttachmentDescription depthAttachDesc =
			{ registry.getImageView(data.m_depthImageViewHandle), AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, {}, true };
			ColorAttachmentDescription colorAttachDescs[]
			{
				{registry.getImageView(data.m_resultImageViewHandle), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} },
				{registry.getImageView(data.m_normalImageViewHandle), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} },
				{registry.getImageView(data.m_specularRoughnessImageViewHandle), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} },
			};
			cmdList->beginRenderPass(sizeof(colorAttachDescs) / sizeof(colorAttachDescs[0]), colorAttachDescs, &depthAttachDesc, { {}, {width, height} });

			gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };
			PipelineColorBlendAttachmentState colorBlendAttachments[]
			{
				GraphicsPipelineBuilder::s_defaultBlendAttachment,
				GraphicsPipelineBuilder::s_defaultBlendAttachment,
				GraphicsPipelineBuilder::s_defaultBlendAttachment,
			};
			Format colorAttachmentFormats[]
			{
				registry.getImageView(data.m_resultImageViewHandle)->getImage()->getDescription().m_format,
				registry.getImageView(data.m_normalImageViewHandle)->getImage()->getDescription().m_format,
				registry.getImageView(data.m_specularRoughnessImageViewHandle)->getImage()->getDescription().m_format,
			};

			// sky
			{
				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);
				builder.setVertexShader("Resources/Shaders/hlsl/sky_vs.spv");
				builder.setFragmentShader("Resources/Shaders/hlsl/sky_ps.spv");
				builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
				builder.setDepthTest(true, false, CompareOp::EQUAL);
				builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
				builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
				builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageViewHandle)->getImage()->getDescription().m_format);
				builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
				Rect scissor{ { 0, 0 }, { width, height } };

				cmdList->setViewport(0, 1, &viewport);
				cmdList->setScissor(0, 1, &scissor);

				cmdList->draw(3, 1, 0, 0);
			}

			// forward lighting
			{
				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);
				builder.setVertexShader("Resources/Shaders/forward_vert.spv");
				builder.setFragmentShader("Resources/Shaders/forward_frag.spv");
				builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
				builder.setDepthTest(true, false, CompareOp::EQUAL);
				builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
				builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
				builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageViewHandle)->getImage()->getDescription().m_format);
				builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				// update descriptor sets
				{
					Buffer *vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer;
					DescriptorBufferInfo positionsBufferInfo{ vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition) };
					DescriptorBufferInfo normalsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * sizeof(VertexNormal) };
					DescriptorBufferInfo texCoordsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord) };
					ImageView *shadowImageView = registry.getImageView(data.m_deferredShadowImageViewHandle);
					ImageView *volumetricFogImageView = registry.getImageView(data.m_volumetricFogImageViewHandle);
					ImageView *ssaoImageViewHandle = registry.getImageView(data.m_ssaoImageViewHandle);
					DescriptorBufferInfo pointLightMaskBufferInfo = registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle);
					DescriptorBufferInfo spotLightMaskBufferInfo = registry.getBufferInfo(data.m_spotLightBitMaskBufferHandle);

					DescriptorSetUpdate updates[] =
					{
						Initializers::storageBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
						Initializers::storageBuffer(&normalsBufferInfo, VERTEX_NORMALS_BINDING),
						Initializers::storageBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
						//Initializers::storageBuffer(&data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING),
						Initializers::storageBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),
						//Initializers::storageBuffer(&data.m_subMeshInfoBufferInfo, SUB_MESH_DATA_BINDING),
						Initializers::storageBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
						Initializers::sampledImage(&shadowImageView, DEFERRED_SHADOW_IMAGE_BINDING),
						Initializers::sampledImage(&volumetricFogImageView, VOLUMETRIC_FOG_IMAGE_BINDING),
						Initializers::sampledImage(&ssaoImageViewHandle, SSAO_IMAGE_BINDING),
						Initializers::storageBuffer(&data.m_directionalLightDataBufferInfo, DIRECTIONAL_LIGHT_DATA_BINDING),
						Initializers::storageBuffer(&data.m_pointLightDataBufferInfo, POINT_LIGHT_DATA_BINDING),
						Initializers::storageBuffer(&data.m_pointLightZBinsBufferInfo, POINT_LIGHT_Z_BINS_BINDING),
						Initializers::storageBuffer(&pointLightMaskBufferInfo, POINT_LIGHT_MASK_BINDING),
						Initializers::storageBuffer(&data.m_spotLightDataBufferInfo, SPOT_LIGHT_DATA_BINDING),
						Initializers::storageBuffer(&data.m_spotLightZBinsBufferInfo, SPOT_LIGHT_Z_BINS_BINDING),
						Initializers::storageBuffer(&spotLightMaskBufferInfo, SPOT_LIGHT_MASK_BINDING),
						Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
					};

					descriptorSet->update(static_cast<uint32_t>(sizeof(updates) / sizeof(updates[0])), updates);
				}

				DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);

				Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
				Rect scissor{ { 0, 0 }, { width, height } };

				cmdList->setViewport(0, 1, &viewport);
				cmdList->setScissor(0, 1, &scissor);

				//cmdList->bindIndexBuffer(registry.getBuffer(data.m_indicesBufferHandle), 0, IndexType::UINT32);
				cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_indexBuffer, 0, IndexType::UINT16);

				const glm::mat4 rowMajorViewMatrix = glm::transpose(data.m_passRecordContext->m_commonRenderData->m_viewMatrix);

				PushConsts pushConsts;
				pushConsts.jitteredViewProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredViewProjectionMatrix;
				pushConsts.viewMatrixRow0 = rowMajorViewMatrix[0];
				pushConsts.viewMatrixRow1 = rowMajorViewMatrix[1];
				pushConsts.viewMatrixRow2 = rowMajorViewMatrix[2];
				pushConsts.pointLightCount = (data.m_passRecordContext->m_commonRenderData->m_pointLightCount & 0xFFFF) | ((data.m_passRecordContext->m_commonRenderData->m_spotLightCount & 0xFFFF) << 16);
				pushConsts.ambientOcclusion = data.m_ssao;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, 0, offsetof(PushConsts, transformIndex), &pushConsts);

				for (uint32_t i = 0; i < data.m_instanceDataCount; ++i)
				{
					const auto &instanceData = data.m_instanceData[i + data.m_instanceDataOffset];


					pushConsts.transformIndex = instanceData.m_transformIndex;
					pushConsts.materialIndex = instanceData.m_materialIndex;

					cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, transformIndex), sizeof(pushConsts) - offsetof(PushConsts, transformIndex), &pushConsts.transformIndex);

					const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

					cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, subMeshInfo.m_vertexOffset, 0);
				}
			}

			cmdList->endRenderPass();
		});
}
