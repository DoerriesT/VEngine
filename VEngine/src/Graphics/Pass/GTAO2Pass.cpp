#include "GTAO2Pass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "GlobalVar.h"
#include <glm/detail/func_trigonometric.hpp>
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/gtao2.hlsli"
}

void VEngine::GTAO2Pass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;

	Constants consts;
	consts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
	consts.width = data.m_passRecordContext->m_commonRenderData->m_width;
	consts.height = data.m_passRecordContext->m_commonRenderData->m_height;
	consts.texelSize = 1.0f / glm::vec2(consts.width, consts.height);
	consts.radiusScale = 0.5f * g_gtaoRadius * (1.0f / tanf(glm::radians(data.m_passRecordContext->m_commonRenderData->m_fovy) * 0.5f) * (consts.height / static_cast<float>(consts.width)));
	consts.falloffScale = 1.0f / g_gtaoRadius;
	consts.falloffBias = -0.75f;
	consts.sampleCount = g_gtaoSteps;
	consts.frame = data.m_passRecordContext->m_commonRenderData->m_frame;
	consts.maxTexelRadius = static_cast<float>(g_gtaoMaxRadiusPixels.get());

	memcpy(uboDataPtr, &consts, sizeof(consts));

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_normalImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		//{rg::ResourceViewHandle(data.m_tangentSpaceImageHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
	};

	graph.addPass("GTAO", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/gtao2_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *depthImageView = registry.getImageView(data.m_depthImageHandle);
				ImageView *normalImageView = registry.getImageView(data.m_normalImageViewHandle);
				ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&normalImageView, NORMAL_IMAGE_BINDING),
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		});
}
