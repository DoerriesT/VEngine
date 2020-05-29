#include "VolumetricFogExtinctionVolumePass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogExtinctionVolume.hlsli"
}

void VEngine::VolumetricFogExtinctionVolumePass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_extinctionVolumeImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
	};

	graph.addPass("Volumetric Fog Extinction Volume", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricFogExtinctionVolume_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *extinctionImageView = registry.getImageView(data.m_extinctionVolumeImageViewHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&extinctionImageView, EXTINCTION_IMAGE_BINDING),
					Initializers::storageBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
					Initializers::storageBuffer(&data.m_localMediaBufferInfo, LOCAL_MEDIA_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			const auto &imageDesc = registry.getImage(data.m_extinctionVolumeImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;
			uint32_t d = imageDesc.m_depth;

			PushConsts pushConsts;
			pushConsts.positionScale = 0.25f;
			pushConsts.positionBias = -(glm::vec3(w, h, d) * 0.5f + 0.5f) * 0.25f;
			pushConsts.globalMediaCount = data.m_passRecordContext->m_commonRenderData->m_globalParticipatingMediaCount;
			pushConsts.localMediaCount = data.m_passRecordContext->m_commonRenderData->m_localParticipatingMediaCount;
			pushConsts.dstOffset = 0;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((w + 7) / 8, (h + 7) / 8, d);
		}, true);
}
