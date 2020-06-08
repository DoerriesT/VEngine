#include "VolumetricFogVBufferPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"
#include <glm/gtc/type_ptr.hpp>

using namespace VEngine::gal;

extern bool g_fogDithering;
extern bool g_fogDoubleSample;
extern bool g_fogJittering;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogVBuffer.hlsli"
}

void VEngine::VolumetricFogVBufferPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	Constants consts;
	consts.viewMatrix = commonData->m_viewMatrix;
	consts.frustumCornerTL = { data.m_frustumCorners[0][0], data.m_frustumCorners[0][1], data.m_frustumCorners[0][2] };
	consts.jitterX = g_fogJittering ? data.m_jitter[0] : 0.0f;
	consts.frustumCornerTR = { data.m_frustumCorners[1][0], data.m_frustumCorners[1][1], data.m_frustumCorners[1][2] };
	consts.jitterY = g_fogJittering ? data.m_jitter[1] : 0.0f;
	consts.frustumCornerBL = { data.m_frustumCorners[2][0], data.m_frustumCorners[2][1], data.m_frustumCorners[2][2] };
	consts.jitterZ = g_fogJittering ? data.m_jitter[2] : 0.0f;
	consts.frustumCornerBR = { data.m_frustumCorners[3][0], data.m_frustumCorners[3][1], data.m_frustumCorners[3][2] };
	consts.globalMediaCount = commonData->m_globalParticipatingMediaCount;
	consts.cameraPos = commonData->m_cameraPosition;
	consts.localMediaCount = commonData->m_localParticipatingMediaCount;
	consts.jitter1 = g_fogJittering ? glm::vec3{ data.m_jitter[3], data.m_jitter[4], data.m_jitter[5] } : glm::vec3(0.5f);
	consts.useDithering = g_fogDithering;
	consts.sampleCount = g_fogDoubleSample ? 2 : 1;


	memcpy(uboDataPtr, &consts, sizeof(consts));

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_scatteringExtinctionImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_emissivePhaseImageViewHandle), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_localMediaBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
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
				DescriptorBufferInfo localMediaMaskBufferInfo = registry.getBufferInfo(data.m_localMediaBitMaskBufferHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&scatteringExtinctionImageView, SCATTERING_EXTINCTION_IMAGE_BINDING),
					Initializers::storageImage(&emissivePhaseImageView, EMISSIVE_PHASE_IMAGE_BINDING),
					Initializers::storageBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
					Initializers::storageBuffer(&data.m_localMediaBufferInfo, LOCAL_MEDIA_BINDING),
					Initializers::storageBuffer(&data.m_localMediaZBinsBufferInfo, LOCAL_MEDIA_Z_BINS_BINDING),
					Initializers::storageBuffer(&localMediaMaskBufferInfo, LOCAL_MEDIA_BIT_MASK_BINDING),
					Initializers::uniformBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX], LINEAR_SAMPLER_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTexture3DDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
			}

			const auto &imageDesc = registry.getImage(data.m_scatteringExtinctionImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;
			uint32_t d = imageDesc.m_depth;

			cmdList->dispatch((w + 1) / 2, (h + 1) / 2, (d + 15) / 16);
		});
}
