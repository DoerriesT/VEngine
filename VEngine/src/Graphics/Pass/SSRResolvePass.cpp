#include "SSRResolvePass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Utility/Utility.h"
#include <glm/detail/func_trigonometric.hpp>
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/ssrResolve.hlsli"
}

void VEngine::SSRResolvePass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
	const auto &projMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredProjectionMatrix;

	Constants consts;
	consts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
	consts.depthProjectParams = glm::vec2(projMatrix[2][2], projMatrix[3][2]);
	consts.width = data.m_passRecordContext->m_commonRenderData->m_width;
	consts.height = data.m_passRecordContext->m_commonRenderData->m_height;
	consts.texelWidth = 1.0f / consts.width;
	consts.texelHeight = 1.0f / consts.height;
	consts.diameterToScreen = 0.5f / tanf(glm::radians(data.m_passRecordContext->m_commonRenderData->m_fovy) * 0.5f) * glm::min(consts.width, consts.height);
	consts.bias = data.m_bias * 0.5f;
	consts.ignoreHistory = data.m_ignoreHistory;

	memcpy(uboDataPtr, &consts, sizeof(consts));

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_resultMaskImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_rayHitPDFImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_maskImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_normalRoughnessImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_albedoMetalnessImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_prevColorImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_velocityImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("SSR Resolve", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/ssrResolve_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);
				ImageView *resultMaskImageView = registry.getImageView(data.m_resultMaskImageHandle);
				ImageView *rayHitPdfImageView = registry.getImageView(data.m_rayHitPDFImageHandle);
				ImageView *maskImageView = registry.getImageView(data.m_maskImageHandle);
				ImageView *depthImageView = registry.getImageView(data.m_depthImageHandle);
				ImageView *normalRoughnessImageView = registry.getImageView(data.m_normalRoughnessImageHandle);
				ImageView *albedoMetalnessImageView = registry.getImageView(data.m_albedoMetalnessImageHandle);
				ImageView *prevColorImageView = registry.getImageView(data.m_prevColorImageHandle);
				ImageView *velocityImageView = registry.getImageView(data.m_velocityImageHandle);
				DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::storageImage(&resultMaskImageView, RESULT_MASK_IMAGE_BINDING),
					Initializers::sampledImage(&rayHitPdfImageView, RAY_HIT_PDF_IMAGE_BINDING),
					Initializers::sampledImage(&maskImageView, MASK_IMAGE_BINDING),
					Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&normalRoughnessImageView, NORMAL_ROUGHNESS_IMAGE_BINDING),
					//Initializers::sampledImage(&albedoMetalnessImageView, ALBEDO_METALNESS_IMAGE_BINDING),
					Initializers::sampledImage(&prevColorImageView, PREV_COLOR_IMAGE_BINDING),
					Initializers::sampledImage(&velocityImageView, VELOCITY_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
					Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::storageBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);
			}

			DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet };
			cmdList->bindDescriptorSets(pipeline, 0, 1, descriptorSets);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		});
}
