#include "VolumetricFogVBufferPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"
#include <glm/gtc/type_ptr.hpp>

bool g_albedoExtinctionMode = true;
float g_fogScattering[3] = { 1.0f, 1.0f, 1.0f };
float g_fogAbsorption[3] = { 1.0f, 1.0f, 1.0f };
float g_fogAlbedo[3] = { 0.01f, 0.01f, 0.01f };
float g_fogExtinction = 0.01f;
float g_fogEmissiveColor[3] = {};
float g_fogEmissiveIntensity = 0.0f;
float g_fogPhase = 0.0f;

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogVBuffer.hlsli"
}

void VEngine::VolumetricFogVBufferPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_scatteringExtinctionImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_emissivePhaseImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
	};

	graph.addPass("Volumetric Fog VBuffer", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricFogVBuffer_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *scatteringExtinctionImageView = registry.getImageView(data.m_scatteringExtinctionImageViewHandle);
				ImageView *emissivePhaseImageView = registry.getImageView(data.m_emissivePhaseImageViewHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&scatteringExtinctionImageView, SCATTERING_EXTINCTION_IMAGE_BINDING),
					Initializers::storageImage(&emissivePhaseImageView, EMISSIVE_PHASE_IMAGE_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			const auto *commonData = data.m_passRecordContext->m_commonRenderData;

			PushConsts pushConsts;
			pushConsts.scatteringExtinction = glm::vec4(glm::make_vec3(g_fogScattering), g_fogExtinction);
			pushConsts.emissivePhase = glm::vec4(glm::make_vec3(g_fogEmissiveColor) * g_fogEmissiveIntensity, g_fogPhase);
			pushConsts.frustumCornerTL = { data.m_frustumCorners[0][0], data.m_frustumCorners[0][1], data.m_frustumCorners[0][2] };
			pushConsts.jitterX = data.m_jitter[0];
			pushConsts.frustumCornerTR = { data.m_frustumCorners[1][0], data.m_frustumCorners[1][1], data.m_frustumCorners[1][2] };
			pushConsts.jitterY = data.m_jitter[1];
			pushConsts.frustumCornerBL = { data.m_frustumCorners[2][0], data.m_frustumCorners[2][1], data.m_frustumCorners[2][2] };
			pushConsts.jitterZ = data.m_jitter[2];
			pushConsts.frustumCornerBR = { data.m_frustumCorners[3][0], data.m_frustumCorners[3][1], data.m_frustumCorners[3][2] };
			pushConsts.cameraPos = commonData->m_cameraPosition;


			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			const auto &imageDesc = registry.getImage(data.m_scatteringExtinctionImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;
			uint32_t d = imageDesc.m_depth;

			cmdList->dispatch((w + 7) / 8, (h + 7) / 8, d);
		});
}
