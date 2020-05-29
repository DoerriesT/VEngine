#include "VolumetricFogExtinctionVolumeDebugPass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogExtinctionVolumeDebug.hlsli"
}

void VEngine::VolumetricFogExtinctionVolumeDebugPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), { gal::ResourceState::READ_WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_extinctionVolumeImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
	};

	graph.addPass("Volumetric Fog Extinction Volume Debug", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricFogExtinctionVolumeDebug_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *extinctionImageView = registry.getImageView(data.m_extinctionVolumeImageViewHandle);
				ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::sampledImage(&extinctionImageView, EXTINCTION_IMAGE_BINDING),
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			const auto &imageDesc = registry.getImage(data.m_extinctionVolumeImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;
			uint32_t d = imageDesc.m_depth;

			PushConsts pushConsts;
			pushConsts.invViewProjection = data.m_passRecordContext->m_commonRenderData->m_invJitteredViewProjectionMatrix;
			pushConsts.coordScale = 4.0f;
			pushConsts.coordBias = (glm::vec3(w, h, d) * 0.5f + 0.5f);// *4.0f;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		}, true);
}
