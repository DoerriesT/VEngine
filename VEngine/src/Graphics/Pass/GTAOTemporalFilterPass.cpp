#include "GTAOTemporalFilterPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "GlobalVar.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/gtaoTemporalFilter.hlsli"
}

void VEngine::GTAOTemporalFilterPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants), sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	Constants consts;
	consts.invViewProjection = data.m_passRecordContext->m_commonRenderData->m_invViewProjectionMatrix;
	consts.prevInvViewProjection = data.m_passRecordContext->m_commonRenderData->m_prevInvViewProjectionMatrix;
	consts.width = commonData->m_width;
	consts.height = commonData->m_height;
	consts.texelSize = 1.0f / glm::vec2(consts.width, consts.height);
	consts.nearPlane = commonData->m_nearPlane;
	consts.farPlane = commonData->m_farPlane;
	consts.ignoreHistory = data.m_ignoreHistory;
	
	memcpy(uboDataPtr, &consts, sizeof(consts));

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_inputImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_velocityImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_previousImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("GTAO Temporal Filter", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/hlsl/gtaoTemporalFilter_cs");
		
		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		cmdList->bindPipeline(pipeline);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *inputImageView = registry.getImageView(data.m_inputImageHandle);
			ImageView *velocityImageView = registry.getImageView(data.m_velocityImageHandle);
			ImageView *prevImageView = registry.getImageView(data.m_previousImageHandle);
			ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);

			DescriptorSetUpdate2 updates[] =
			{
				Initializers::texture(&inputImageView, INPUT_IMAGE_BINDING),
				Initializers::texture(&velocityImageView, VELOCITY_IMAGE_BINDING),
				Initializers::texture(&prevImageView, HISTORY_IMAGE_BINDING),
				Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
				Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
			};

			descriptorSet->update((uint32_t)std::size(updates), updates);

			DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
			cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
		}

		cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
	});
}
