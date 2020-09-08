#include "RasterTilingPass.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/RenderResources.h"
#include "Graphics/LightData.h"
#include <glm/gtx/transform.hpp>
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <glm/vec3.hpp>
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/rasterTiling.hlsli"
}

void VEngine::RasterTilingPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskImageHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskImageHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_participatingMediaBitMaskImageHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_reflectionProbeBitMaskImageHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
	};

	graph.addPass("Raster Tiling", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		GraphicsPipelineCreateInfo pipelineCreateInfo;
		GraphicsPipelineBuilder builder(pipelineCreateInfo);
		builder.setVertexShader("Resources/Shaders/hlsl/rasterTiling_vs");
		builder.setFragmentShader("Resources/Shaders/hlsl/rasterTiling_ps");
		builder.setVertexBindingDescription({ 0, sizeof(float) * 3, VertexInputRate::VERTEX });
		builder.setVertexAttributeDescription({"POSITION", 0, 0, Format::R32G32B32_SFLOAT, 0 });
		builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::FRONT_BIT, FrontFace::COUNTER_CLOCKWISE);
		builder.setMultisampleState(SampleCount::_4, false, 0.0f, 0xFFFFFFFF, false, false);
		builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		// begin renderpass
		cmdList->beginRenderPass(0, nullptr, nullptr, { {}, {width / 2, height / 2} }, true);

		cmdList->bindPipeline(pipeline);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *punctualLightsMaskImageView = registry.getImageView(data.m_punctualLightsBitMaskImageHandle);
			ImageView *punctualLightsShadowedMaskImageView = registry.getImageView(data.m_punctualLightsShadowedBitMaskImageHandle);
			ImageView *participatingMediaMaskImageView = registry.getImageView(data.m_participatingMediaBitMaskImageHandle);
			ImageView *reflectionProbeMaskImageView = registry.getImageView(data.m_reflectionProbeBitMaskImageHandle);

			DescriptorSetUpdate2 updates[] =
			{
				Initializers::rwTexture(&punctualLightsMaskImageView, PUNCTUAL_LIGHTS_BIT_MASK_BINDING),
				Initializers::rwTexture(&punctualLightsShadowedMaskImageView, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING),
				Initializers::rwTexture(&participatingMediaMaskImageView, PARTICIPATING_MEDIA_BIT_MASK_BINDING),
				Initializers::rwTexture(&reflectionProbeMaskImageView, REFLECTION_PROBE_BIT_MASK_BINDING),
			};

			descriptorSet->update((uint32_t)std::size(updates), updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

		Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width / 2), static_cast<float>(height / 2), 0.0f, 1.0f };
		Rect scissor{ { 0, 0 }, { width / 2, height / 2 } };

		cmdList->setViewport(0, 1, &viewport);
		cmdList->setScissor(0, 1, &scissor);

		cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_lightProxyIndexBuffer, 0, IndexType::UINT16);

		uint64_t vertexOffset = 0;
		cmdList->bindVertexBuffers(0, 1, &data.m_passRecordContext->m_renderResources->m_lightProxyVertexBuffer, &vertexOffset);

		uint32_t alignedDomainSizeX = (width + RendererConsts::LIGHTING_TILE_SIZE - 1) / RendererConsts::LIGHTING_TILE_SIZE;
		

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, alignedDomainSizeX), sizeof(alignedDomainSizeX), &alignedDomainSizeX);

		auto &renderResource = *data.m_passRecordContext->m_renderResources;
		const uint32_t pointLightProxyMeshIndexCount = renderResource.m_pointLightProxyMeshIndexCount;
		const uint32_t pointLightProxyMeshFirstIndex = renderResource.m_pointLightProxyMeshFirstIndex;
		const uint32_t pointLightProxyMeshVertexOffset = renderResource.m_pointLightProxyMeshVertexOffset;
		const uint32_t spotLightProxyMeshIndexCount = renderResource.m_spotLightProxyMeshIndexCount;
		const uint32_t spotLightProxyMeshFirstIndex = renderResource.m_spotLightProxyMeshFirstIndex;
		const uint32_t spotLightProxyMeshVertexOffset = renderResource.m_spotLightProxyMeshVertexOffset;
		const uint32_t boxProxyMeshIndexCount = renderResource.m_boxProxyMeshIndexCount;
		const uint32_t boxProxyMeshFirstIndex = renderResource.m_boxProxyMeshFirstIndex;
		const uint32_t boxProxyMeshVertexOffset = renderResource.m_boxProxyMeshVertexOffset;
		
		auto &commonData = *data.m_passRecordContext->m_commonRenderData;
		auto &lightData = *data.m_lightData;

		// punctual lights
		uint32_t targetBuffer = 0;
		uint32_t wordCount = static_cast<uint32_t>((data.m_lightData->m_punctualLights.size() + 31) / 32);
		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, targetBuffer), sizeof(targetBuffer), &targetBuffer);
		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, wordCount), sizeof(wordCount), &wordCount);
		for (size_t i = 0; i < lightData.m_punctualLights.size(); ++i)
		{
			PushConsts pushConsts;
			pushConsts.transform = commonData.m_jitteredViewProjectionMatrix * lightData.m_punctualLightTransforms[lightData.m_punctualLightOrder[i]];
			pushConsts.index = static_cast<uint32_t>(i);

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, 0, offsetof(PushConsts, alignedDomainSizeX), &pushConsts);

			const bool spotLight = lightData.m_punctualLights[lightData.m_punctualLightOrder[i]].m_angleScale != -1.0f;
			uint32_t indexCount = spotLight ? spotLightProxyMeshIndexCount : pointLightProxyMeshIndexCount;
			uint32_t firstIndex = spotLight ? spotLightProxyMeshFirstIndex : pointLightProxyMeshFirstIndex;
			uint32_t vertexOffset = spotLight ? spotLightProxyMeshVertexOffset : pointLightProxyMeshVertexOffset;

			cmdList->drawIndexed(indexCount, 1, firstIndex, vertexOffset, 0);
		}

		// shadowed punctual lights
		targetBuffer = 1;
		wordCount = static_cast<uint32_t>((data.m_lightData->m_punctualLightsShadowed.size() + 31) / 32);
		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, targetBuffer), sizeof(targetBuffer), &targetBuffer);
		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, wordCount), sizeof(wordCount), &wordCount);
		for (size_t i = 0; i < lightData.m_punctualLightsShadowed.size(); ++i)
		{
			PushConsts pushConsts;
			pushConsts.transform = commonData.m_jitteredViewProjectionMatrix * lightData.m_punctualLightShadowedTransforms[lightData.m_punctualLightShadowedOrder[i]];
			pushConsts.index = static_cast<uint32_t>(i);

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, 0, offsetof(PushConsts, alignedDomainSizeX), &pushConsts);

			const bool spotLight = lightData.m_punctualLightsShadowed[lightData.m_punctualLightShadowedOrder[i]].m_light.m_angleScale != -1.0f;
			uint32_t indexCount = spotLight ? spotLightProxyMeshIndexCount : pointLightProxyMeshIndexCount;
			uint32_t firstIndex = spotLight ? spotLightProxyMeshFirstIndex : pointLightProxyMeshFirstIndex;
			uint32_t vertexOffset = spotLight ? spotLightProxyMeshVertexOffset : pointLightProxyMeshVertexOffset;

			cmdList->drawIndexed(indexCount, 1, firstIndex, vertexOffset, 0);
		}

		// participating media
		targetBuffer = 2;
		wordCount = static_cast<uint32_t>((data.m_lightData->m_localParticipatingMedia.size() + 31) / 32);
		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, targetBuffer), sizeof(targetBuffer), &targetBuffer);
		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, wordCount), sizeof(wordCount), &wordCount);
		for (size_t i = 0; i < lightData.m_localParticipatingMedia.size(); ++i)
		{
			PushConsts pushConsts;
			pushConsts.transform = commonData.m_jitteredViewProjectionMatrix * lightData.m_localMediaTransforms[lightData.m_localMediaOrder[i]];
			pushConsts.index = static_cast<uint32_t>(i);

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, 0, offsetof(PushConsts, alignedDomainSizeX), &pushConsts);

			bool spherical = lightData.m_localParticipatingMedia[lightData.m_localMediaOrder[i]].m_spherical;
			uint32_t indexCount = spherical ? pointLightProxyMeshIndexCount : boxProxyMeshIndexCount;
			uint32_t firstIndex = spherical ? pointLightProxyMeshFirstIndex : boxProxyMeshFirstIndex;
			uint32_t vertexOffset = spherical ? pointLightProxyMeshVertexOffset : boxProxyMeshVertexOffset;

			cmdList->drawIndexed(indexCount, 1, firstIndex, vertexOffset, 0);
		}

		// reflection probes
		targetBuffer = 3;
		wordCount = static_cast<uint32_t>((data.m_lightData->m_localReflectionProbes.size() + 31) / 32);
		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, targetBuffer), sizeof(targetBuffer), &targetBuffer);
		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, wordCount), sizeof(wordCount), &wordCount);
		for (size_t i = 0; i < lightData.m_localReflectionProbes.size(); ++i)
		{
			PushConsts pushConsts;
			pushConsts.transform = commonData.m_jitteredViewProjectionMatrix * lightData.m_localReflectionProbeTransforms[lightData.m_localReflectionProbeOrder[i]];
			pushConsts.index = static_cast<uint32_t>(i);

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, 0, offsetof(PushConsts, alignedDomainSizeX), &pushConsts);
			cmdList->drawIndexed(boxProxyMeshIndexCount, 1, boxProxyMeshFirstIndex, boxProxyMeshVertexOffset, 0);
		}

		cmdList->endRenderPass();
	});
}
