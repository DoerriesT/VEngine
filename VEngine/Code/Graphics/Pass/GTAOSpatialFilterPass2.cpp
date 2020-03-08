#include "GTAOSpatialFilterPass2.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext2.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/gtaoSpatialFilter_bindings.h"
}


void VEngine::GTAOSpatialFilterPass2::addToGraph(rg::RenderGraph2 &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_inputImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
	};

	graph.addPass("GTAO Spatial Filter", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry2 &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/gtaoSpatialFilter_comp.spv");
		
		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		cmdList->bindPipeline(pipeline);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *inputImageView = registry.getImageView(data.m_inputImageHandle);
			ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);

			DescriptorSetUpdate updates[] =
			{
				Initializers::sampledImage(&inputImageView, INPUT_IMAGE_BINDING),
				Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
				Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
			};

			descriptorSet->update(3, updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

		cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
	});
}
