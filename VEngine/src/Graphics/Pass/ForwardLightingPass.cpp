#include "ForwardLightingPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/Mesh.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"
#include "GlobalVar.h"

using namespace VEngine::gal;

extern glm::vec3 g_sunDir;
extern int g_volumetricShadow;

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
	uint64_t alignment = graph.getGraphicsDevice()->getBufferAlignment(DescriptorType2::CONSTANT_BUFFER, sizeof(Constants));
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(alignment, uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	Constants consts;
	consts.jitteredViewProjectionMatrix = commonData->m_jitteredViewProjectionMatrix;
	consts.viewMatrixDepthRow = glm::vec4(commonData->m_viewMatrix[0][2], commonData->m_viewMatrix[1][2], commonData->m_viewMatrix[2][2], commonData->m_viewMatrix[3][2]);
	consts.cameraPos = commonData->m_cameraPosition;
	consts.directionalLightCount = commonData->m_directionalLightCount;
	consts.directionalLightShadowedCount = commonData->m_directionalLightShadowedCount;
	consts.punctualLightCount = commonData->m_punctualLightCount;
	consts.punctualLightShadowedCount = commonData->m_punctualLightShadowedCount;
	consts.ambientOcclusion = 0;
	consts.width = commonData->m_width;
	consts.volumetricShadow = g_volumetricShadow;
	consts.lodBias = g_TAAMipBias;

	memcpy(uboDataPtr, &consts, sizeof(consts));


	rg::ResourceUsageDescription passUsages[]
	{
		//{rg::ResourceViewHandle(data.m_indicesBufferHandle), {gal::ResourceState::READ_INDEX_BUFFER, PipelineStageFlagBits::VERTEX_INPUT_BIT}},
		//{rg::ResourceViewHandle(data.m_indirectBufferHandle), {gal::ResourceState::READ_INDIRECT_BUFFER, PipelineStageFlagBits::DRAW_INDIRECT_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {gal::ResourceState::READ_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT }},
		{rg::ResourceViewHandle(data.m_normalRoughnessImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_albedoMetalnessImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT} },
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_deferredShadowImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_shadowAtlasImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_fomImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_atmosphereScatteringImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_atmosphereTransmittanceImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
	};

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
				{registry.getImageView(data.m_normalRoughnessImageViewHandle), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} },
				{registry.getImageView(data.m_albedoMetalnessImageViewHandle), AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} },
			};
			cmdList->beginRenderPass(sizeof(colorAttachDescs) / sizeof(colorAttachDescs[0]), colorAttachDescs, &depthAttachDesc, { {}, {width, height} }, false);

			PipelineColorBlendAttachmentState colorBlendAttachments[]
			{
				GraphicsPipelineBuilder::s_defaultBlendAttachment,
				GraphicsPipelineBuilder::s_defaultBlendAttachment,
				GraphicsPipelineBuilder::s_defaultBlendAttachment,
			};
			Format colorAttachmentFormats[]
			{
				registry.getImageView(data.m_resultImageViewHandle)->getImage()->getDescription().m_format,
				registry.getImageView(data.m_normalRoughnessImageViewHandle)->getImage()->getDescription().m_format,
				registry.getImageView(data.m_albedoMetalnessImageViewHandle)->getImage()->getDescription().m_format,
			};

			// sky
			{
				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);
				builder.setVertexShader("Resources/Shaders/hlsl/sky_vs");
				builder.setFragmentShader("Resources/Shaders/hlsl/sky_ps");
				builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
				builder.setDepthTest(true, false, CompareOp::EQUAL);
				builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
				builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
				builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageViewHandle)->getImage()->getDescription().m_format);
				builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));
				
				// update descriptor sets
				{
					ImageView *atmosphereScatteringImageView = registry.getImageView(data.m_atmosphereScatteringImageViewHandle);
					ImageView *atmosphereTransmittanceImageView = registry.getImageView(data.m_atmosphereTransmittanceImageViewHandle);
					DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::texture(&data.m_probeImageView, 0),
						Initializers::constantBuffer(&data.m_atmosphereConstantBufferInfo, 1),
						Initializers::texture(&atmosphereTransmittanceImageView, 2),
						Initializers::texture(&atmosphereScatteringImageView, 3),
						Initializers::byteBuffer(&exposureDataBufferInfo, 4),
					};
				
					descriptorSet->update((uint32_t)std::size(updates), updates);
				}
				
				DescriptorSet *sets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_samplerDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, sets);

				Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
				Rect scissor{ { 0, 0 }, { width, height } };

				cmdList->setViewport(0, 1, &viewport);
				cmdList->setScissor(0, 1, &scissor);

				struct SkyPushConsts
				{
					float4x4 invModelViewProjection;
					float3 sunDir;
					float cameraHeight;
				};

				SkyPushConsts pushconsts;
				pushconsts.invModelViewProjection = glm::inverse(data.m_passRecordContext->m_commonRenderData->m_jitteredProjectionMatrix * glm::mat4(glm::mat3(data.m_passRecordContext->m_commonRenderData->m_viewMatrix)));
				pushconsts.sunDir = g_sunDir;
				pushconsts.cameraHeight = data.m_passRecordContext->m_commonRenderData->m_cameraPosition.y;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, 0, sizeof(pushconsts), &pushconsts);

				cmdList->draw(3, 1, 0, 0);
			}

			// forward lighting
			{
				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);
				builder.setVertexShader("Resources/Shaders/hlsl/forward_vs");
				builder.setFragmentShader("Resources/Shaders/hlsl/forward_ps");
				builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
				builder.setDepthTest(true, false, CompareOp::EQUAL);
				builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
				builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
				builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageViewHandle)->getImage()->getDescription().m_format);
				builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				// update descriptor sets
				{
					Buffer *vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer;
					DescriptorBufferInfo positionsBufferInfo{ vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), 4u };
					//DescriptorBufferInfo normalsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * sizeof(VertexNormal) };
					DescriptorBufferInfo tangentsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition)), RendererConsts::MAX_VERTICES * sizeof(VertexQTangent), 4u };
					DescriptorBufferInfo texCoordsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexQTangent)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord), 4u };
					ImageView *shadowImageView = registry.getImageView(data.m_deferredShadowImageViewHandle);
					//ImageView *volumetricFogImageView = registry.getImageView(data.m_volumetricFogImageViewHandle);
					//ImageView *ssaoImageViewHandle = registry.getImageView(data.m_ssaoImageViewHandle);
					ImageView *shadowAtlasImageViewHandle = registry.getImageView(data.m_shadowAtlasImageViewHandle);
					ImageView *fomImageViewHandle = registry.getImageView(data.m_fomImageViewHandle);
					ImageView *punctualLightsMaskImageView = registry.getImageView(data.m_punctualLightsBitMaskImageViewHandle);
					ImageView *punctualLightsShadowedMaskImageView = registry.getImageView(data.m_punctualLightsShadowedBitMaskImageViewHandle);
					DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
						Initializers::structuredBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
						//Initializers::structuredBuffer(&normalsBufferInfo, VERTEX_NORMALS_BINDING),
						Initializers::structuredBuffer(&tangentsBufferInfo, VERTEX_QTANGENTS_BINDING),
						Initializers::structuredBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
						//Initializers::structuredBuffer(&data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING),
						Initializers::structuredBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),
						//Initializers::structuredBuffer(&data.m_subMeshInfoBufferInfo, SUB_MESH_DATA_BINDING),
						Initializers::structuredBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
						Initializers::texture(&shadowImageView, DEFERRED_SHADOW_IMAGE_BINDING),
						//Initializers::texture(&volumetricFogImageView, VOLUMETRIC_FOG_IMAGE_BINDING),
						//Initializers::texture(&ssaoImageViewHandle, SSAO_IMAGE_BINDING),
						Initializers::texture(&shadowAtlasImageViewHandle, SHADOW_ATLAS_IMAGE_BINDING),
						Initializers::structuredBuffer(&data.m_directionalLightsBufferInfo, DIRECTIONAL_LIGHTS_BINDING),
						Initializers::structuredBuffer(&data.m_directionalLightsShadowedBufferInfo, DIRECTIONAL_LIGHTS_SHADOWED_BINDING),
						Initializers::structuredBuffer(&data.m_punctualLightsBufferInfo, PUNCTUAL_LIGHTS_BINDING),
						Initializers::byteBuffer(&data.m_punctualLightsZBinsBufferInfo, PUNCTUAL_LIGHTS_Z_BINS_BINDING),
						Initializers::texture(&punctualLightsMaskImageView, PUNCTUAL_LIGHTS_BIT_MASK_BINDING),
						Initializers::structuredBuffer(&data.m_punctualLightsShadowedBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BINDING),
						Initializers::byteBuffer(&data.m_punctualLightsShadowedZBinsBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING),
						Initializers::texture(&punctualLightsShadowedMaskImageView, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING),
						Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
						Initializers::texture(&fomImageViewHandle, FOM_IMAGE_BINDING),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);
				}

				DescriptorSet *descriptorSets[] = 
				{ 
					descriptorSet, 
					data.m_passRecordContext->m_renderResources->m_textureDescriptorSet, 
					data.m_passRecordContext->m_renderResources->m_samplerDescriptorSet,
					data.m_passRecordContext->m_renderResources->m_shadowSamplerDescriptorSet
				};
				cmdList->bindDescriptorSets(pipeline, 0, 4, descriptorSets);

				Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
				Rect scissor{ { 0, 0 }, { width, height } };

				cmdList->setViewport(0, 1, &viewport);
				cmdList->setScissor(0, 1, &scissor);

				//cmdList->bindIndexBuffer(registry.getBuffer(data.m_indicesBufferHandle), 0, IndexType::UINT32);
				cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_indexBuffer, 0, IndexType::UINT16);

				for (uint32_t i = 0; i < data.m_instanceDataCount; ++i)
				{
					const auto &instanceData = data.m_instanceData[i + data.m_instanceDataOffset];
					const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

					PushConsts pushConsts;
					pushConsts.texCoordScale = float2(data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 0], data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 1]);
					pushConsts.texCoordBias = float2(data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 2], data.m_texCoordScaleBias[instanceData.m_subMeshIndex * 4 + 3]);
					pushConsts.transformIndex = instanceData.m_transformIndex;
					pushConsts.materialIndex = instanceData.m_materialIndex;
					pushConsts.vertexOffset = subMeshInfo.m_vertexOffset;

					cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

					

					cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, 0, 0);
				}
			}

			cmdList->endRenderPass();
		});
}
