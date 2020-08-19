#include "VolumetricFogIntegratePass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/gal/Initializers.h"
#include "Graphics/RenderData.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogIntegrate.hlsli"
}

void VEngine::VolumetricFogIntegratePass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	Constants consts;
	consts.frustumCornerTL = { data.m_frustumCorners[0][0], data.m_frustumCorners[0][1], data.m_frustumCorners[0][2] };
	consts.frustumCornerTR = { data.m_frustumCorners[1][0], data.m_frustumCorners[1][1], data.m_frustumCorners[1][2] };
	consts.frustumCornerBL = { data.m_frustumCorners[2][0], data.m_frustumCorners[2][1], data.m_frustumCorners[2][2] };
	consts.frustumCornerBR = { data.m_frustumCorners[3][0], data.m_frustumCorners[3][1], data.m_frustumCorners[3][2] };
	consts.cameraPos = commonData->m_cameraPosition;


	memcpy(uboDataPtr, &consts, sizeof(consts));

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_inputImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
	};

	graph.addPass("Volumetric Fog Integrate", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricFogIntegrate_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);
				ImageView *inputImageView = registry.getImageView(data.m_inputImageViewHandle);

				DescriptorSetUpdate2 updates[] =
				{
					Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::texture(&inputImageView, INPUT_IMAGE_BINDING),
					Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					//Initializers::sampler(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
				};

				descriptorSet->update(std::size(updates), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			const auto &imageDesc = registry.getImage(data.m_resultImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;

			cmdList->dispatch((w + 7) / 8, (h + 7) / 8, 1);
		});
}
