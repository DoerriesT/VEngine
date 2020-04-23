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
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/forward.hlsli"
}

void VEngine::ForwardLightingPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	Constants consts;
	consts.jitteredViewProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredViewProjectionMatrix;
	consts.invViewMatrix = commonData->m_invViewMatrix;
	consts.viewMatrix = data.m_passRecordContext->m_commonRenderData->m_viewMatrix;
	consts.directionalLightCount = commonData->m_directionalLightCount;
	consts.directionalLightShadowedCount = commonData->m_directionalLightShadowedCount;
	consts.punctualLightCount = commonData->m_punctualLightCount;
	consts.punctualLightShadowedCount = commonData->m_punctualLightShadowedCount;
	consts.ambientOcclusion = data.m_ssao;
	consts.width = commonData->m_width;

	memcpy(uboDataPtr, &consts, sizeof(consts));


	rg::ResourceUsageDescription passUsages[]
	{
		//{rg::ResourceViewHandle(data.m_indicesBufferHandle), {gal::ResourceState::READ_INDEX_BUFFER, PipelineStageFlagBits::VERTEX_INPUT_BIT}},
		//{rg::ResourceViewHandle(data.m_indirectBufferHandle), {gal::ResourceState::READ_INDIRECT_BUFFER, PipelineStageFlagBits::DRAW_INDIRECT_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {gal::ResourceState::READ_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT }},
		{rg::ResourceViewHandle(data.m_normalImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_specularRoughnessImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT} },
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_deferredShadowImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		//{rg::ResourceViewHandle(data.m_volumetricFogImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_shadowAtlasImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		//{rg::ResourceViewHandle(data.m_probeImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
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

				//DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));
				//
				//// update descriptor sets
				//{
				//	ImageView *probeImageViewHandle = registry.getImageView(data.m_probeImageViewHandle);
				//
				//	DescriptorSetUpdate updates[] =
				//	{
				//		Initializers::sampledImage(&probeImageViewHandle, 0),
				//		Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX], 1),
				//	};
				//
				//	descriptorSet->update(static_cast<uint32_t>(sizeof(updates) / sizeof(updates[0])), updates);
				//}
				//
				//cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);

				Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
				Rect scissor{ { 0, 0 }, { width, height } };

				cmdList->setViewport(0, 1, &viewport);
				cmdList->setScissor(0, 1, &scissor);

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(glm::mat4), &data.m_passRecordContext->m_commonRenderData->m_invViewProjectionMatrix);

				cmdList->draw(3, 1, 0, 0);
			}

			// forward lighting
			{
				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);
				builder.setVertexShader("Resources/Shaders/hlsl/forward_vs.spv");
				builder.setFragmentShader("Resources/Shaders/hlsl/forward_ps.spv");
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
					//ImageView *volumetricFogImageView = registry.getImageView(data.m_volumetricFogImageViewHandle);
					ImageView *ssaoImageViewHandle = registry.getImageView(data.m_ssaoImageViewHandle);
					ImageView *shadowAtlasImageViewHandle = registry.getImageView(data.m_shadowAtlasImageViewHandle);
					DescriptorBufferInfo punctualLightsMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsBitMaskBufferHandle);
					DescriptorBufferInfo punctualLightsShadowedMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsShadowedBitMaskBufferHandle);
					DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

					DescriptorSetUpdate updates[] =
					{
						Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
						Initializers::storageBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
						Initializers::storageBuffer(&normalsBufferInfo, VERTEX_NORMALS_BINDING),
						Initializers::storageBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
						//Initializers::storageBuffer(&data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING),
						Initializers::storageBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),
						//Initializers::storageBuffer(&data.m_subMeshInfoBufferInfo, SUB_MESH_DATA_BINDING),
						Initializers::storageBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
						Initializers::sampledImage(&shadowImageView, DEFERRED_SHADOW_IMAGE_BINDING),
						//Initializers::sampledImage(&volumetricFogImageView, VOLUMETRIC_FOG_IMAGE_BINDING),
						Initializers::sampledImage(&ssaoImageViewHandle, SSAO_IMAGE_BINDING),
						Initializers::sampledImage(&shadowAtlasImageViewHandle, SHADOW_ATLAS_IMAGE_BINDING),
						Initializers::storageBuffer(&data.m_directionalLightsBufferInfo, DIRECTIONAL_LIGHTS_BINDING),
						Initializers::storageBuffer(&data.m_directionalLightsShadowedBufferInfo, DIRECTIONAL_LIGHTS_SHADOWED_BINDING),
						Initializers::storageBuffer(&data.m_punctualLightsBufferInfo, PUNCTUAL_LIGHTS_BINDING),
						Initializers::storageBuffer(&data.m_punctualLightsZBinsBufferInfo, PUNCTUAL_LIGHTS_Z_BINS_BINDING),
						Initializers::storageBuffer(&punctualLightsMaskBufferInfo, PUNCTUAL_LIGHTS_BIT_MASK_BINDING),
						Initializers::storageBuffer(&data.m_punctualLightsShadowedBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BINDING),
						Initializers::storageBuffer(&data.m_punctualLightsShadowedZBinsBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING),
						Initializers::storageBuffer(&punctualLightsShadowedMaskBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING),
						Initializers::storageBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
						Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_shadowSampler, SHADOW_SAMPLER_BINDING),
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

				for (uint32_t i = 0; i < data.m_instanceDataCount; ++i)
				{
					const auto &instanceData = data.m_instanceData[i + data.m_instanceDataOffset];

					PushConsts pushConsts;
					pushConsts.transformIndex = instanceData.m_transformIndex;
					pushConsts.materialIndex = instanceData.m_materialIndex;

					cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

					const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

					cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, subMeshInfo.m_vertexOffset, 0);
				}
			}

			cmdList->endRenderPass();
		});
}
