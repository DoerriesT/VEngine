#include "DeferredShadowsPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include <glm/mat4x4.hpp>
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

extern int g_volumetricShadow;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/deferredShadows.hlsli"
}

void VEngine::DeferredShadowsPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	for (size_t i = 0; i < data.m_lightDataCount; ++i)
	{
		DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
		uint64_t alignment = graph.getGraphicsDevice()->getBufferAlignment(DescriptorType2::CONSTANT_BUFFER, sizeof(Constants));
		uint8_t *uboDataPtr = nullptr;
		uboBuffer->allocate(alignment, uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

		const glm::mat4 rowMajorViewMatrix = glm::transpose(data.m_passRecordContext->m_commonRenderData->m_viewMatrix);


		const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
		const auto &lightData = data.m_lightData[i];

		Constants consts;
		consts.invViewMatrix = data.m_passRecordContext->m_commonRenderData->m_invViewMatrix;
		consts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
		consts.direction = glm::vec3(data.m_passRecordContext->m_commonRenderData->m_viewMatrix * glm::vec4(lightData.m_direction, 0.0f));
		consts.cascadeDataOffset = static_cast<int32_t>(lightData.m_shadowOffset);
		consts.cascadeCount = static_cast<int32_t>(lightData.m_shadowCount);
		consts.width = commonData->m_width;
		consts.height = commonData->m_height;
		consts.texelWidth = 1.0f / consts.width;
		consts.texelHeight = 1.0f / consts.height;
		consts.frame = commonData->m_frame & 63u;
		consts.volumetricShadow = g_volumetricShadow;

		memcpy(uboDataPtr, &consts, sizeof(consts));

		rg::ImageViewHandle resultImageViewHandle;

		rg::ImageViewDescription viewDesc{};
		viewDesc.m_name = "Deferred Shadows Layer";
		viewDesc.m_imageHandle = data.m_resultImageHandle;
		viewDesc.m_subresourceRange = { 0, 1, static_cast<uint32_t>(i), 1 };
		resultImageViewHandle = graph.createImageView(viewDesc);

		rg::ResourceUsageDescription passUsages[]
		{
			{rg::ResourceViewHandle(resultImageViewHandle), { gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
			{rg::ResourceViewHandle(data.m_depthImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
			//{rg::ResourceViewHandle(data.m_tangentSpaceImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
			{rg::ResourceViewHandle(data.m_shadowImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
			{rg::ResourceViewHandle(data.m_directionalLightFOMImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
			{rg::ResourceViewHandle(data.m_directionalLightFOMDepthRangeImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		};

		graph.addPass("Deferred Shadows", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
				const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

				// create pipeline description
				ComputePipelineCreateInfo pipelineCreateInfo;
				ComputePipelineBuilder builder(pipelineCreateInfo);
				builder.setComputeShader("Resources/Shaders/hlsl/deferredShadows_cs");

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *resultImageView = registry.getImageView(resultImageViewHandle);
					ImageView *depthImageView = registry.getImageView(data.m_depthImageViewHandle);
					//ImageView *tangentSpaceImageView = registry.getImageView(data.m_tangentSpaceImageViewHandle);
					ImageView *shadowSpaceImageView = registry.getImageView(data.m_shadowImageViewHandle);
					ImageView *fomImageView = registry.getImageView(data.m_directionalLightFOMImageViewHandle);
					ImageView *fomDepthRangeImageView = registry.getImageView(data.m_directionalLightFOMDepthRangeImageViewHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
						Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
						Initializers::texture(&depthImageView, DEPTH_IMAGE_BINDING),
						//Initializers::texture(&tangentSpaceImageView, TANGENT_SPACE_IMAGE_BINDING),
						Initializers::texture(&shadowSpaceImageView, SHADOW_IMAGE_BINDING),
						Initializers::structuredBuffer(&data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING),
						Initializers::structuredBuffer(&data.m_cascadeParamsBufferInfo, CASCADE_PARAMS_BUFFER_BINDING),
						Initializers::texture(&data.m_blueNoiseImageView, BLUE_NOISE_IMAGE_BINDING),
						Initializers::texture(&fomImageView, FOM_IMAGE_BINDING),
						Initializers::texture(&fomDepthRangeImageView, FOM_DEPTH_RANGE_IMAGE_BINDING),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);

					DescriptorSet *sets[] = 
					{ 
						descriptorSet, 
						data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet,
						data.m_passRecordContext->m_renderResources->m_computeShadowSamplerDescriptorSet 
					};
					cmdList->bindDescriptorSets(pipeline, 0, 3, sets);
				}

				cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
			});
	}
}
