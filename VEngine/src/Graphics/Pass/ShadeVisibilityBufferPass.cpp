#include "ShadeVisibilityBufferPass.h"
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

void VEngine::ShadeVisibilityBufferPass::addToGraph(rg::RenderGraph &graph, const Data &data)
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
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_normalRoughnessImageViewHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_albedoMetalnessImageViewHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_deferredShadowImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_shadowAtlasImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_fomImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_triangleImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Shade Visibility Buffer", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/shadeVisibilityBuffer_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				Buffer *vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer;
				DescriptorBufferInfo positionsBufferInfo{ vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition) };
				DescriptorBufferInfo normalsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * sizeof(VertexQTangent) };
				DescriptorBufferInfo texCoordsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexQTangent)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord) };
				ImageView *shadowImageView = registry.getImageView(data.m_deferredShadowImageViewHandle);
				//ImageView *volumetricFogImageView = registry.getImageView(data.m_volumetricFogImageViewHandle);
				//ImageView *ssaoImageViewHandle = registry.getImageView(data.m_ssaoImageViewHandle);
				ImageView *shadowAtlasImageViewHandle = registry.getImageView(data.m_shadowAtlasImageViewHandle);
				ImageView *fomImageViewHandle = registry.getImageView(data.m_fomImageViewHandle);
				ImageView *resultImageViewHandle = registry.getImageView(data.m_resultImageViewHandle);
				ImageView *albedoImageViewHandle = registry.getImageView(data.m_albedoMetalnessImageViewHandle);
				ImageView *normalImageViewHandle = registry.getImageView(data.m_normalRoughnessImageViewHandle);
				ImageView *triangleImageViewHandle = registry.getImageView(data.m_triangleImageViewHandle);
				DescriptorBufferInfo punctualLightsMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsBitMaskBufferHandle);
				DescriptorBufferInfo punctualLightsShadowedMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsShadowedBitMaskBufferHandle);
				DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageViewHandle, RESULT_IMAGE_BINDING),
					Initializers::storageImage(&albedoImageViewHandle, ALBEDO_METALNESS_IMAGE_BINDING),
					Initializers::storageImage(&normalImageViewHandle, NORMAL_ROUGHNESS_IMAGE_BINDING),
					Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::storageBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
					Initializers::storageBuffer(&normalsBufferInfo, VERTEX_NORMALS_BINDING),
					Initializers::storageBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
					Initializers::storageBuffer(&data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING),
					Initializers::storageBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),
					Initializers::storageBuffer(&data.m_subMeshInfoBufferInfo, SUB_MESH_DATA_BINDING),
					Initializers::storageBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
					Initializers::sampledImage(&shadowImageView, DEFERRED_SHADOW_IMAGE_BINDING),
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
					Initializers::sampledImage(&fomImageViewHandle, FOM_IMAGE_BINDING),
					Initializers::sampledImage(&triangleImageViewHandle, TRIANGLE_IMAGE_BINDING),
					Initializers::storageBuffer(&data.m_indexBufferInfo, INDEX_BUFFER_BINDING),
				};

				descriptorSet->update(static_cast<uint32_t>(sizeof(updates) / sizeof(updates[0])), updates);
			}

			DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet };
			cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
			
		}, true);
}
