#include "DeferredShadowsPass2.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext2.h"
#include <glm/mat4x4.hpp>
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	using namespace glm;
#include "../../../../Application/Resources/Shaders/deferredShadows_bindings.h"
}

void VEngine::DeferredShadowsPass2::addToGraph(rg::RenderGraph2 &graph, const Data &data)
{
	for (size_t i = 0; i < data.m_lightDataCount; ++i)
	{
		rg::ImageViewHandle resultImageViewHandle;

		rg::ImageViewDescription viewDesc{};
		viewDesc.m_name = "Deferred Shadows Layer";
		viewDesc.m_imageHandle = data.m_resultImageHandle;
		viewDesc.m_subresourceRange = { 0, 1, static_cast<uint32_t>(i), 1 };
		resultImageViewHandle = graph.createImageView(viewDesc);

		rg::ResourceUsageDescription passUsages[]
		{
			{rg::ResourceViewHandle(resultImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
			{rg::ResourceViewHandle(data.m_depthImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
			{rg::ResourceViewHandle(data.m_tangentSpaceImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
			{rg::ResourceViewHandle(data.m_shadowImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		};

		graph.addPass("Deferred Shadows", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry2 &registry)
			{
				const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
				const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

				// create pipeline description
				ComputePipelineCreateInfo pipelineCreateInfo;
				ComputePipelineBuilder builder(pipelineCreateInfo);
				builder.setComputeShader("Resources/Shaders/deferredShadows_comp.spv");

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					Sampler *pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
					Sampler *shadowSampler = data.m_passRecordContext->m_renderResources->m_shadowSampler;
					ImageView *resultImageView = registry.getImageView(resultImageViewHandle);
					ImageView *depthImageView = registry.getImageView(data.m_depthImageViewHandle);
					ImageView *tangentSpaceImageView = registry.getImageView(data.m_tangentSpaceImageViewHandle);
					ImageView *shadowSpaceImageView = registry.getImageView(data.m_shadowImageViewHandle);

					DescriptorSetUpdate updates[] =
					{
						Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
						Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
						Initializers::sampledImage(&tangentSpaceImageView, TANGENT_SPACE_IMAGE_BINDING),
						Initializers::sampledImage(&shadowSpaceImageView, SHADOW_IMAGE_BINDING),
						Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_shadowSampler, SHADOW_SAMPLER_BINDING),
						Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
						Initializers::storageBuffer(&data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING),
						Initializers::storageBuffer(&data.m_cascadeParamsBufferInfo, CASCADE_PARAMS_BUFFER_BINDING),
					};

					descriptorSet->update(8, updates);

					cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
				}

				const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
				const auto &lightData = data.m_lightData[i];

				PushConsts pushConsts;
				pushConsts.invViewMatrix = data.m_passRecordContext->m_commonRenderData->m_invViewMatrix;
				pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
				pushConsts.direction = glm::vec3(lightData.m_direction);
				pushConsts.cascadeDataOffset = static_cast<int32_t>(lightData.m_shadowMatricesOffsetCount >> 16);
				pushConsts.cascadeCount = static_cast<int32_t>(lightData.m_shadowMatricesOffsetCount & 0xFFFF);

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);
				
				cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
			});
	}
}
