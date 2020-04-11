#include "VolumetricFogApplyPass.h"
#include <glm/vec4.hpp>
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogApply.hlsli"
}

void VEngine::VolumetricFogApplyPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::READ_WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_volumetricFogImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_indirectSpecularLightImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_brdfLutImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_specularRoughnessImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		{rg::ResourceViewHandle(data.m_normalImageViewHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
	};

	graph.addPass("Volumetric Fog Apply", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricFogApply_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);
				ImageView *depthImageView = registry.getImageView(data.m_depthImageViewHandle);
				ImageView *volumetricFogImageView = registry.getImageView(data.m_volumetricFogImageViewHandle);
				ImageView *indirectSpecularImageView = registry.getImageView(data.m_indirectSpecularLightImageViewHandle);
				ImageView *brdfLutImageView = registry.getImageView(data.m_brdfLutImageViewHandle);
				ImageView *specularRoughnessImageView = registry.getImageView(data.m_specularRoughnessImageViewHandle);
				ImageView *normalImageView = registry.getImageView(data.m_normalImageViewHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&volumetricFogImageView, VOLUMETRIC_FOG_IMAGE_BINDING),
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
					Initializers::sampledImage(&indirectSpecularImageView, INDIRECT_SPECULAR_LIGHT_IMAGE_BINDING),
					Initializers::sampledImage(&brdfLutImageView, BRDF_LUT_IMAGE_BINDING),
					Initializers::sampledImage(&specularRoughnessImageView, SPEC_ROUGHNESS_IMAGE_BINDING),
					Initializers::sampledImage(&normalImageView, NORMAL_IMAGE_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;

			PushConsts pushConsts;
			pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
			pushConsts.width = width;
			pushConsts.height = height;
			pushConsts.texelWidth = 1.0f / width;
			pushConsts.texelHeight = 1.0f / height;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		});
}
