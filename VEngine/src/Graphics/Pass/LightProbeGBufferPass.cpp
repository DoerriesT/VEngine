#include "LightProbeGBufferPass.h"
#include <glm/vec2.hpp>
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "GlobalVar.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"
#include <glm/ext.hpp>

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/lightProbeGBuffer.hlsli"
}

void VEngine::LightProbeGBufferPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	const glm::mat4 vulkanCorrection =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.5f, 0.0f },
		{ 0.0f, 0.0f, 0.5f, 1.0f }
	};

	glm::mat4 projection = vulkanCorrection * glm::perspectiveLH(glm::radians(90.0f), 1.0f, data.m_probeNearPlane, data.m_probeFarPlane);

	Constants consts;
	consts.viewMatrix = data.m_passRecordContext->m_commonRenderData->m_viewMatrix;
	consts.invViewMatrix = data.m_passRecordContext->m_commonRenderData->m_invViewMatrix;
	consts.probeFaceToViewSpace[0] = consts.viewMatrix * glm::inverse(projection * glm::lookAtLH(data.m_probePosition, data.m_probePosition + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	consts.probeFaceToViewSpace[1] = consts.viewMatrix * glm::inverse(projection * glm::lookAtLH(data.m_probePosition, data.m_probePosition + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	consts.probeFaceToViewSpace[2] = consts.viewMatrix * glm::inverse(projection * glm::lookAtLH(data.m_probePosition, data.m_probePosition + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
	consts.probeFaceToViewSpace[3] = consts.viewMatrix * glm::inverse(projection * glm::lookAtLH(data.m_probePosition, data.m_probePosition + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
	consts.probeFaceToViewSpace[4] = consts.viewMatrix * glm::inverse(projection * glm::lookAtLH(data.m_probePosition, data.m_probePosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	consts.probeFaceToViewSpace[5] = consts.viewMatrix * glm::inverse(projection * glm::lookAtLH(data.m_probePosition, data.m_probePosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	consts.texelSize = 1.0f / RendererConsts::REFLECTION_PROBE_RES;
	consts.width = RendererConsts::REFLECTION_PROBE_RES;
	consts.directionalLightCount = data.m_passRecordContext->m_commonRenderData->m_directionalLightCount;
	consts.directionalLightShadowedCount = data.m_passRecordContext->m_commonRenderData->m_directionalLightShadowedProbeCount;
	consts.arrayTextureOffset = data.m_probeIndex * 6;


	memcpy(uboDataPtr, &consts, sizeof(Constants));


	rg::ResourceUsageDescription passUsages[]
	{
		{ rg::ResourceViewHandle(data.m_directionalShadowImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{ rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
	};

	graph.addPass("Light Probe G-Buffer", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/lightProbeGBuffer_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				ImageView *shadowImageView = registry.getImageView(data.m_directionalShadowImageViewHandle);
				ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);
				
				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::sampledImage(&data.m_depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&data.m_albedoRoughnessImageView, ALBEDO_ROUGHNESS_IMAGE_BINDING),
					Initializers::sampledImage(&data.m_normalImageView, NORMAL_IMAGE_BINDING),
					Initializers::sampledImage(&shadowImageView, DIRECTIONAL_LIGHTS_SHADOW_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_shadowSampler, SHADOW_SAMPLER_BINDING),
					Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::storageBuffer(&data.m_directionalLightsBufferInfo, DIRECTIONAL_LIGHTS_BINDING),
					Initializers::storageBuffer(&data.m_directionalLightsShadowedProbeBufferInfo, DIRECTIONAL_LIGHTS_SHADOWED_BINDING),
					Initializers::storageBuffer(&data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);
			}

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);

			cmdList->dispatch((RendererConsts::REFLECTION_PROBE_RES + 7) / 8, (RendererConsts::REFLECTION_PROBE_RES + 7) / 8, 6);
		}, true);
}
