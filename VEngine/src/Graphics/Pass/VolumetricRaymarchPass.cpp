#include "VolumetricRaymarchPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace VEngine::gal;


namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricRaymarch.hlsli"
}

void VEngine::VolumetricRaymarchPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);


	auto proj = glm::perspective(commonData->m_fovy, commonData->m_width / float(commonData->m_height), 0.5f, commonData->m_farPlane);
	auto invViewProj = glm::inverse(proj * commonData->m_viewMatrix);

	glm::vec4 frustumCorners[4];
	frustumCorners[0] = invViewProj * glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f); // top left
	frustumCorners[1] = invViewProj * glm::vec4(1.0f, 1.0f, 0.0f, 1.0f); // top right
	frustumCorners[2] = invViewProj * glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f); // bottom left
	frustumCorners[3] = invViewProj * glm::vec4(1.0f, -1.0f, 0.0f, 1.0f); // bottom right

	for (auto &c : frustumCorners)
	{
		c /= c.w;
	}

	const auto &invProjMatrix = commonData->m_invJitteredProjectionMatrix;

	Constants consts;
	consts.viewMatrix = commonData->m_viewMatrix;
	consts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
	consts.frustumCornerTL = frustumCorners[0];
	consts.frustumCornerTR = frustumCorners[1];
	consts.frustumCornerBL = frustumCorners[2];
	consts.frustumCornerBR = frustumCorners[3];
	consts.width = commonData->m_width;
	consts.height = commonData->m_height;
	consts.texelSize = 1.0f / glm::vec2(commonData->m_width, commonData->m_height);
	consts.rayOriginFactor = 0.5f / commonData->m_farPlane;
	consts.rayOriginDist = 0.5f;
	consts.cameraPos = commonData->m_cameraPosition;
	consts.globalMediaCount = commonData->m_globalParticipatingMediaCount;
	consts.directionalLightCount = commonData->m_directionalLightCount;
	consts.directionalLightShadowedCount = commonData->m_directionalLightShadowedCount;

	memcpy(uboDataPtr, &consts, sizeof(consts));


	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_shadowImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Volumetric Raymarch", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricRaymarch_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);
				ImageView *depthImageView = registry.getImageView(data.m_depthImageViewHandle);
				ImageView *shadowImageView = registry.getImageView(data.m_shadowImageViewHandle);
				DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&shadowImageView, SHADOW_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_shadowSampler, SHADOW_SAMPLER_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
					Initializers::storageBuffer(&data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING),
					Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::storageBuffer(&data.m_directionalLightsBufferInfo, DIRECTIONAL_LIGHTS_BINDING),
					Initializers::storageBuffer(&data.m_directionalLightsShadowedBufferInfo, DIRECTIONAL_LIGHTS_SHADOWED_BINDING),
					Initializers::storageBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
					Initializers::storageBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTexture3DDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
			}

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);;
		}, true);
}
