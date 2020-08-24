#include "TonemapPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "GlobalVar.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/tonemap.hlsli"
}

void VEngine::TonemapPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_avgLuminanceBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_srcImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_dstImageHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_bloomImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	uint32_t usageCount = data.m_bloomEnabled ? sizeof(passUsages) / sizeof(passUsages[0]) : sizeof(passUsages) / sizeof(passUsages[0]) - 1;

	graph.addPass("Tonemap", rg::QueueType::GRAPHICS, usageCount, passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/hlsl/tonemap_cs");

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		cmdList->bindPipeline(pipeline);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *resultImageView = registry.getImageView(data.m_dstImageHandle);
			ImageView *inputImageView = registry.getImageView(data.m_srcImageHandle);
			ImageView *bloomImageImageView = registry.getImageView(data.m_bloomEnabled ? data.m_bloomImageViewHandle : data.m_srcImageHandle); // need to bind a valid image if bloom is disabled
			DescriptorBufferInfo avgLumBufferInfo = registry.getBufferInfo(data.m_avgLuminanceBufferHandle);
			DescriptorBufferInfo exposureBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

			DescriptorSetUpdate2 updates[] =
			{
				Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
				Initializers::texture(&inputImageView, INPUT_IMAGE_BINDING),
				Initializers::texture(&bloomImageImageView, BLOOM_IMAGE_BINDING),
				//Initializers::storageBuffer(&avgLumBufferInfo, LUMINANCE_VALUES_BINDING),
				Initializers::byteBuffer(&exposureBufferInfo, EXPOSURE_DATA_BINDING),
			};

			descriptorSet->update((uint32_t)std::size(updates), updates);

			DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
			cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
		}

		PushConsts pushConsts;
		pushConsts.texelSize = 1.0f / glm::vec2(width, height);
		pushConsts.applyLinearToGamma = data.m_applyLinearToGamma;
		pushConsts.bloomEnabled = data.m_bloomEnabled;
		pushConsts.bloomStrength = data.m_bloomStrength;

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
	});
}
