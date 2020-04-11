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
		{rg::ResourceViewHandle(data.m_punctualLightsBitMaskBufferHandle), {gal::ResourceState::WRITE_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_punctualLightsShadowedBitMaskBufferHandle), {gal::ResourceState::WRITE_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
	};

	graph.addPass("Raster Tiling", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		GraphicsPipelineCreateInfo pipelineCreateInfo;
		GraphicsPipelineBuilder builder(pipelineCreateInfo);
		gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };
		builder.setVertexShader("Resources/Shaders/hlsl/rasterTiling_vs.spv");
		builder.setFragmentShader("Resources/Shaders/hlsl/rasterTiling_ps.spv");
		builder.setVertexBindingDescription({ 0, sizeof(float) * 3, VertexInputRate::VERTEX });
		builder.setVertexAttributeDescription({ 0, 0, Format::R32G32B32_SFLOAT, 0 });
		builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::FRONT_BIT, FrontFace::COUNTER_CLOCKWISE);
		builder.setMultisampleState(SampleCount::_4, false, 0.0f, 0xFFFFFFFF, false, false);
		builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		// begin renderpass
		cmdList->beginRenderPass(0, nullptr, nullptr, { {}, {width / 2, height / 2} });

		cmdList->bindPipeline(pipeline);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			DescriptorBufferInfo punctualLightsMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsBitMaskBufferHandle);
			DescriptorBufferInfo punctualLightsShadowedMaskBufferInfo = registry.getBufferInfo(data.m_punctualLightsShadowedBitMaskBufferHandle);

			DescriptorSetUpdate updates[] =
			{
				Initializers::storageBuffer(&punctualLightsMaskBufferInfo, PUNCTUAL_LIGHTS_BIT_MASK_BINDING),
				Initializers::storageBuffer(&punctualLightsShadowedMaskBufferInfo, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING),
			};

			descriptorSet->update(2, updates);

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

		cmdList->endRenderPass();
	});
}
