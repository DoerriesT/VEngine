#include "GaussianDownsamplePass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/gaussianPyramid.hlsli"
}

void VEngine::GaussianDownsamplePass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	assert(data.m_levels < 15);
	assert(data.m_levels > 1);

	// create temp image
	rg::ImageDescription tempImageDesc = {};
	tempImageDesc.m_name = "Gaussian Pyramid Temp Image";
	tempImageDesc.m_clear = false;
	tempImageDesc.m_clearValue.m_imageClearValue = {};
	tempImageDesc.m_width = data.m_width / 2;
	tempImageDesc.m_height = data.m_height;
	tempImageDesc.m_layers = 1;
	tempImageDesc.m_levels = data.m_levels - 1;
	tempImageDesc.m_samples = SampleCount::_1;
	tempImageDesc.m_format = data.m_format;
	tempImageDesc.m_usageFlags = ImageUsageFlagBits::SAMPLED_BIT | ImageUsageFlagBits::STORAGE_BIT;

	rg::ImageHandle tempImageHandle = graph.createImage(tempImageDesc);

	// create image views for each mip
	rg::ImageViewHandle viewHandles[14];
	for (uint32_t i = 0; i < data.m_levels; ++i)
	{
		rg::ImageViewDescription viewDesc{};
		viewDesc.m_name = "Gaussian Pyramid Mip Level";
		viewDesc.m_imageHandle = data.m_resultImageHandle;
		viewDesc.m_subresourceRange = { i, 1, 0, 1 };
		viewHandles[i] = graph.createImageView(viewDesc);
	}

	rg::ImageViewHandle tmpImgViewHandles[13];
	for (uint32_t i = 0; i < data.m_levels - 1; ++i)
	{
		rg::ImageViewDescription viewDesc{};
		viewDesc.m_name = "Gaussian Pyramid Temp Mip Level";
		viewDesc.m_imageHandle = tempImageHandle;
		viewDesc.m_subresourceRange = { i, 1, 0, 1 };
		tmpImgViewHandles[i] = graph.createImageView(viewDesc);
	}

	rg::ResourceUsageDescription passUsages[14 + 13]; // 14 levels input image maximum + 13 levels temp image maximum
	uint32_t passUsageCount = 0;
	for (uint32_t i = 0; i < data.m_levels; ++i)
	{
		rg::ResourceViewHandle resViewHandle = rg::ResourceViewHandle(viewHandles[i]);

		// last level isnt read from in this pass
		if (i == (data.m_levels - 1))
		{
			passUsages[passUsageCount++] = { resViewHandle, {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} };
		}
		else if (i == 0)
		{
			passUsages[passUsageCount++] = { resViewHandle, { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
		}
		else
		{
			passUsages[passUsageCount++] = { resViewHandle, {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}, true, {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} };
		}
		
		if (i > 0)
		{
			passUsages[passUsageCount++] = { rg::ResourceViewHandle(tmpImgViewHandles[i - 1]), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}, true, {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} };
		}
	}

	graph.addPass("Gaussian Downsample", rg::QueueType::GRAPHICS, passUsageCount, passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/gaussianPyramid_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			Sampler *linearSampler = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX];
			uint32_t mipWidth = data.m_passRecordContext->m_commonRenderData->m_width;
			uint32_t mipHeight = data.m_passRecordContext->m_commonRenderData->m_height;

			for (uint32_t i = 1; i < data.m_levels; ++i)
			{
				// source mip to temp image
				{
					// update descriptor set
					{
						DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

						ImageView *inputImageView = registry.getImageView(viewHandles[i - 1]);
						ImageView *tempImageView = registry.getImageView(tmpImgViewHandles[i - 1]);

						DescriptorSetUpdate updates[] =
						{
							Initializers::sampledImage(&inputImageView, INPUT_IMAGE_BINDING),
							Initializers::storageImage(&tempImageView, RESULT_IMAGE_BINDING),
							Initializers::samplerDescriptor(&linearSampler, LINEAR_SAMPLER_BINDING),
						};

						descriptorSet->update(3, updates);

						cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
					}

					PushConsts pushConsts;
					pushConsts.srcTexelSize = 1.0f / glm::vec2(mipWidth, mipHeight);

					mipWidth = glm::max(mipWidth / 2u, 1u);

					pushConsts.width = mipWidth;
					pushConsts.height = mipHeight;
					pushConsts.horizontal = true;

					cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

					cmdList->dispatch((mipWidth + 7) / 8, (mipHeight + 7) / 8, 1);
				}
				
				// transition temp image
				{
					Barrier barrier = Initializers::imageBarrier(registry.getImage(tmpImgViewHandles[0]),
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						gal::ResourceState::WRITE_STORAGE_IMAGE,
						gal::ResourceState::READ_TEXTURE,
						{ i - 1, 1, 0, 1 });

					cmdList->barrier(1, &barrier);
				}

				// temp image to dest mip
				{
					// update descriptor set
					{
						DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

						ImageView *tempImageView = registry.getImageView(tmpImgViewHandles[i - 1]);
						ImageView *resultImageView = registry.getImageView(viewHandles[i]);

						DescriptorSetUpdate updates[] =
						{
							Initializers::sampledImage(&tempImageView, INPUT_IMAGE_BINDING),
							Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
							Initializers::samplerDescriptor(&linearSampler, LINEAR_SAMPLER_BINDING),
						};

						descriptorSet->update(3, updates);

						cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
					}

					PushConsts pushConsts;
					pushConsts.srcTexelSize = 1.0f / glm::vec2(mipWidth, mipHeight);

					mipHeight = glm::max(mipHeight / 2u, 1u);

					pushConsts.width = mipWidth;
					pushConsts.height = mipHeight;
					pushConsts.horizontal = false;

					cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

					cmdList->dispatch((mipWidth + 7) / 8, (mipHeight + 7) / 8, 1);
				}
				// dont insert a barrier after the last iteration: we dont know the next state, so let the RenderGraph figure it out
				if (i < (data.m_levels - 1))
				{
					Barrier barrier = Initializers::imageBarrier(registry.getImage(viewHandles[i]),
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						gal::ResourceState::WRITE_STORAGE_IMAGE,
						gal::ResourceState::READ_TEXTURE,
						{ i, 1, 0, 1 });

					cmdList->barrier(1, &barrier);
				}
			}
		}, true);
}
