#include "ShadeVisibilityBufferPassPS.h"
#include "Graphics/RenderResources.h"
#include "Graphics/Mesh.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"
#include <glm/ext.hpp>

using namespace VEngine::gal;

extern glm::vec3 g_sunDir;
extern int g_volumetricShadow;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/shadeVisibilityBuffer.hlsli"
}

void VEngine::ShadeVisibilityBufferPassPS::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	glm::vec4 frustumDirTL = commonData->m_invJitteredViewProjectionMatrix * glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f);
	frustumDirTL /= frustumDirTL.w;
	frustumDirTL -= commonData->m_cameraPosition;
	glm::vec4 frustumDirTR = commonData->m_invJitteredViewProjectionMatrix * glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
	frustumDirTR /= frustumDirTR.w;
	frustumDirTR -= commonData->m_cameraPosition;
	glm::vec4 frustumDirBL = commonData->m_invJitteredViewProjectionMatrix * glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f);
	frustumDirBL /= frustumDirBL.w;
	frustumDirBL -= commonData->m_cameraPosition;

	Constants consts;
	consts.jitteredViewProjectionMatrix = commonData->m_jitteredViewProjectionMatrix;
	consts.invViewMatrix = commonData->m_invViewMatrix;
	consts.viewMatrix = commonData->m_viewMatrix;
	consts.frustumDirTL = glm::vec3(frustumDirTL);
	consts.frustumDirDeltaX = (glm::vec3(frustumDirTR) - glm::vec3(frustumDirTL)) / (float)commonData->m_width;
	consts.frustumDirDeltaY = (glm::vec3(frustumDirBL) - glm::vec3(frustumDirTL)) / (float)commonData->m_height;
	consts.cameraPosWSX = commonData->m_cameraPosition.x;
	consts.cameraPosWSY = commonData->m_cameraPosition.y;
	consts.cameraPosWSZ = commonData->m_cameraPosition.z;
	consts.directionalLightCount = commonData->m_directionalLightCount;
	consts.directionalLightShadowedCount = commonData->m_directionalLightShadowedCount;
	consts.punctualLightCount = commonData->m_punctualLightCount;
	consts.punctualLightShadowedCount = commonData->m_punctualLightShadowedCount;
	consts.width = commonData->m_width;
	consts.height = commonData->m_height;
	consts.coordScale = 4.0f;
	consts.coordBias = (glm::vec3(64.0f) * 0.5f + 0.5f);// *4.0f;
	consts.extinctionVolumeTexelSize = 1.0f / 64.0f;
	consts.volumetricShadow = g_volumetricShadow;

	memcpy(uboDataPtr, &consts, sizeof(consts));


	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT }},
		{rg::ResourceViewHandle(data.m_normalRoughnessImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_albedoMetalnessImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT} },
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_deferredShadowImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {gal::ResourceState::READ_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
		{rg::ResourceViewHandle(data.m_shadowAtlasImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_fomImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_triangleImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
	};

	graph.addPass("Shade Visibility Buffer PS", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
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
			cmdList->beginRenderPass(sizeof(colorAttachDescs) / sizeof(colorAttachDescs[0]), colorAttachDescs, &depthAttachDesc, { {}, {width, height} });


			// create pipeline description
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

			GraphicsPipelineCreateInfo pipelineCreateInfo;
			GraphicsPipelineBuilder builder(pipelineCreateInfo);
			builder.setVertexShader("Resources/Shaders/hlsl/fullscreenTriangle_vs.spv");
			builder.setFragmentShader("Resources/Shaders/hlsl/shadeVisibilityBuffer_ps.spv");
			builder.setColorBlendAttachments(3, colorBlendAttachments);
			builder.setDepthTest(true, false, CompareOp::NOT_EQUAL);
			builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
			builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageViewHandle)->getImage()->getDescription().m_format);
			builder.setColorAttachmentFormats(3, colorAttachmentFormats);

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);
			
			cmdList->bindPipeline(pipeline);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				Buffer *vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer;
				DescriptorBufferInfo positionsBufferInfo{ vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), 4u };
				DescriptorBufferInfo tangentsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * sizeof(VertexQTangent), 4u };
				DescriptorBufferInfo texCoordsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexQTangent)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord), 4u };
				ImageView *shadowImageView = registry.getImageView(data.m_deferredShadowImageViewHandle);
				//ImageView *volumetricFogImageView = registry.getImageView(data.m_volumetricFogImageViewHandle);
				//ImageView *ssaoImageViewHandle = registry.getImageView(data.m_ssaoImageViewHandle);
				ImageView *shadowAtlasImageViewHandle = registry.getImageView(data.m_shadowAtlasImageViewHandle);
				ImageView *fomImageViewHandle = registry.getImageView(data.m_fomImageViewHandle);
				ImageView *triangleImageViewHandle = registry.getImageView(data.m_triangleImageViewHandle);
				DescriptorBufferInfo punctualLightsMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsBitMaskBufferHandle);
				DescriptorBufferInfo punctualLightsShadowedMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsShadowedBitMaskBufferHandle);
				DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);
				DescriptorBufferInfo texCoordScaleBiasBufferInfo{ data.m_passRecordContext->m_renderResources->m_subMeshTexCoordScaleBiasBuffer, 0, RendererConsts::MAX_SUB_MESHES * sizeof(float) * 4 };

				DescriptorSetUpdate2 updates[] =
				{
					Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::structuredBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
					Initializers::structuredBuffer(&tangentsBufferInfo, VERTEX_QTANGENTS_BINDING),
					Initializers::structuredBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
					Initializers::structuredBuffer(&data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING),
					Initializers::structuredBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),
					Initializers::structuredBuffer(&data.m_subMeshInfoBufferInfo, SUB_MESH_DATA_BINDING),
					Initializers::structuredBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
					Initializers::texture(&shadowImageView, DEFERRED_SHADOW_IMAGE_BINDING),
					Initializers::texture(&shadowAtlasImageViewHandle, SHADOW_ATLAS_IMAGE_BINDING),
					Initializers::structuredBuffer(&data.m_directionalLightsBufferInfo, DIRECTIONAL_LIGHTS_BINDING),
					Initializers::structuredBuffer(&data.m_directionalLightsShadowedBufferInfo, DIRECTIONAL_LIGHTS_SHADOWED_BINDING),
					Initializers::structuredBuffer(&data.m_punctualLightsBufferInfo, PUNCTUAL_LIGHTS_BINDING),
					Initializers::byteBuffer(&data.m_punctualLightsZBinsBufferInfo, PUNCTUAL_LIGHTS_Z_BINS_BINDING),
					Initializers::byteBuffer(&punctualLightsMaskBufferInfo, PUNCTUAL_LIGHTS_BIT_MASK_BINDING),
					Initializers::structuredBuffer(&data.m_punctualLightsShadowedBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BINDING),
					Initializers::byteBuffer(&data.m_punctualLightsShadowedZBinsBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING),
					Initializers::byteBuffer(&punctualLightsShadowedMaskBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING),
					Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
					Initializers::sampler(&data.m_passRecordContext->m_renderResources->m_shadowSampler, SHADOW_SAMPLER_BINDING),
					Initializers::texture(&fomImageViewHandle, FOM_IMAGE_BINDING),
					Initializers::texture(&triangleImageViewHandle, TRIANGLE_IMAGE_BINDING),
					Initializers::structuredBuffer(&data.m_indexBufferInfo, INDEX_BUFFER_BINDING),
					Initializers::structuredBuffer(&texCoordScaleBiasBufferInfo, TEXCOORD_SCALE_BIAS_BINDING),
				};

				descriptorSet->update(static_cast<uint32_t>(sizeof(updates) / sizeof(updates[0])), updates);
			}

			DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
			cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);

			Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
			Rect scissor{ { 0, 0 }, { width, height } };

			cmdList->setViewport(0, 1, &viewport);
			cmdList->setScissor(0, 1, &scissor);

			cmdList->draw(3, 1, 0, 0);

			cmdList->endRenderPass();

		}, true);
}
