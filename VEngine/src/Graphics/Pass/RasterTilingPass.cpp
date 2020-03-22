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
	using namespace glm;
#include "../../../../Application/Resources/Shaders/rasterTiling_bindings.h"
}

void VEngine::RasterTilingPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_pointLightBitMaskBufferHandle), {gal::ResourceState::WRITE_STORAGE_BUFFER, PipelineStageFlagBits::FRAGMENT_SHADER_BIT}},
	};

	graph.addPass("Raster Tiling", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		GraphicsPipelineCreateInfo pipelineCreateInfo;
		GraphicsPipelineBuilder builder(pipelineCreateInfo);
		gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };
		builder.setVertexShader("Resources/Shaders/rasterTiling_vert.spv");
		builder.setFragmentShader("Resources/Shaders/rasterTiling_frag.spv");
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

			DescriptorBufferInfo pointLightMaskBufferInfo = registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle);

			DescriptorSetUpdate updates[] =
			{
				Initializers::storageBuffer(&pointLightMaskBufferInfo, POINT_LIGHT_MASK_BINDING),
			};

			descriptorSet->update(1, updates);

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
		uint32_t wordCount = static_cast<uint32_t>((data.m_lightData->m_pointLightData.size() + 31) / 32);

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, alignedDomainSizeX), sizeof(alignedDomainSizeX), &alignedDomainSizeX);
		cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, offsetof(PushConsts, wordCount), sizeof(wordCount), &wordCount);

		auto &renderResource = *data.m_passRecordContext->m_renderResources;
		const uint32_t pointLightProxyMeshIndexCount = renderResource.m_pointLightProxyMeshIndexCount;
		const uint32_t pointLightProxyMeshFirstIndex = renderResource.m_pointLightProxyMeshFirstIndex;
		const uint32_t pointLightProxyMeshVertexOffset = renderResource.m_pointLightProxyMeshVertexOffset;
		const uint32_t spotLightProxyMeshIndexCount = renderResource.m_spotLightProxyMeshIndexCount;
		const uint32_t spotLightProxyMeshFirstIndex = renderResource.m_spotLightProxyMeshFirstIndex;
		const uint32_t spotLightProxyMeshVertexOffset = renderResource.m_spotLightProxyMeshVertexOffset;

		for (size_t i = 0; i < data.m_lightData->m_pointLightData.size(); ++i)
		{
			const auto &item = data.m_lightData->m_pointLightData[i];

			PushConsts pushConsts;
			pushConsts.transform = data.m_passRecordContext->m_commonRenderData->m_projectionMatrix * glm::translate(glm::vec3(item.m_positionRadius)) * glm::scale(glm::vec3(item.m_positionRadius.w));
			pushConsts.index = static_cast<uint32_t>(i);

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, 0, offsetof(PushConsts, alignedDomainSizeX), &pushConsts);

			cmdList->drawIndexed(pointLightProxyMeshIndexCount, 1, pointLightProxyMeshFirstIndex, pointLightProxyMeshVertexOffset, 0);
		}

		cmdList->endRenderPass();
	});
}
