#include "ProbeCompressBCH6Pass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/probeCompressBCH6.hlsli"
}

void VEngine::ProbeCompressBCH6Pass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	graph.addPass("Reflection Probe BCH6 Compression", rg::QueueType::GRAPHICS, 0, nullptr, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// transition tmp result image from READ_IMAGE_TRANSFER to WRITE_STORAGE_IMAGE
			{
				gal::Barrier barrier = Initializers::imageBarrier(data.m_tmpResultImageViews[0]->getImage(),
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					ResourceState::READ_IMAGE_TRANSFER,
					ResourceState::WRITE_STORAGE_IMAGE,
					{ 0, RendererConsts::REFLECTION_PROBE_MIPS, data.m_tmpResultImageViews[0]->getDescription().m_baseArrayLayer, 6 });
				cmdList->barrier(1, &barrier);
			}

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/probeCompressBCH6_cs");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor set
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				DescriptorSetUpdate2 updates[] =
				{
					Initializers::rwTexture(data.m_tmpResultImageViews, RESULT_IMAGE_BINDING, 0, RendererConsts::REFLECTION_PROBE_MIPS),
					Initializers::texture(data.m_inputImageViews, INPUT_IMAGE_BINDING, 0, RendererConsts::REFLECTION_PROBE_MIPS),
				};

				descriptorSet->update((uint32_t)std::size(updates), updates);

				DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
			}

			uint32_t mipSize = RendererConsts::REFLECTION_PROBE_RES;
			for (uint32_t i = 0; i < RendererConsts::REFLECTION_PROBE_MIPS; ++i)
			{
				PushConsts pushConsts;
				pushConsts.mip = i;
				pushConsts.resultRes = mipSize > 4 ? mipSize / 4 : 1;
				pushConsts.texelSize = 1.0f / mipSize;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);
				cmdList->dispatch((pushConsts.resultRes + 7) / 8, (pushConsts.resultRes + 7) / 8, 6);

				mipSize /= 2;
			}

			// transition tmp result image from WRITE_STORAGE_IMAGE to READ_IMAGE_TRANSFER
			// transition actual result image from READ_TEXTURE to WRITE_IMAGE_TRANSFER
			{
				gal::Barrier barrier0 = Initializers::imageBarrier(data.m_tmpResultImageViews[0]->getImage(),
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					ResourceState::WRITE_STORAGE_IMAGE,
					ResourceState::READ_IMAGE_TRANSFER,
					{ 0, RendererConsts::REFLECTION_PROBE_MIPS, data.m_tmpResultImageViews[0]->getDescription().m_baseArrayLayer, 6 });

				gal::Barrier barrier1 = Initializers::imageBarrier(data.m_compressedCubeArrayImage,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					PipelineStageFlagBits::TRANSFER_BIT,
					ResourceState::READ_TEXTURE,
					ResourceState::WRITE_IMAGE_TRANSFER,
					{ 0, RendererConsts::REFLECTION_PROBE_MIPS, data.m_probeIndex * 6, 6 });

				gal::Barrier barriers[]{ barrier0, barrier1 };

				cmdList->barrier(2, barriers);
			}

			// copy from temp compressed image to final compressed image
			{
				ImageCopy regions[RendererConsts::REFLECTION_PROBE_MIPS];
				uint32_t mipWidth = RendererConsts::REFLECTION_PROBE_RES / 4;
				for (uint32_t i = 0; i < RendererConsts::REFLECTION_PROBE_MIPS; ++i)
				{
					auto &region = regions[i];
					region = {};
					region.m_srcMipLevel = i;
					region.m_srcBaseLayer = 0;
					region.m_srcLayerCount = 6;
					region.m_srcOffset = {};
					region.m_dstMipLevel = i;
					region.m_dstBaseLayer = data.m_probeIndex * 6;
					region.m_dstLayerCount = 6;
					region.m_dstOffset = {};
					region.m_extent.m_width = mipWidth;
					region.m_extent.m_height = mipWidth;
					region.m_extent.m_depth = 1;

					mipWidth /= 2;
				}
				cmdList->copyImage(data.m_tmpResultImageViews[0]->getImage(), data.m_compressedCubeArrayImage, RendererConsts::REFLECTION_PROBE_MIPS, regions);
			}
			
			// transition final compressed image back to READ_TEXTURE from WRITE_IMAGE_TRANSFER
			{
				gal::Barrier barrier = Initializers::imageBarrier(data.m_compressedCubeArrayImage,
					PipelineStageFlagBits::TRANSFER_BIT,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					ResourceState::WRITE_IMAGE_TRANSFER,
					ResourceState::READ_TEXTURE,
					{ 0, RendererConsts::REFLECTION_PROBE_MIPS, data.m_probeIndex * 6, 6 });

				cmdList->barrier(1, &barrier);
			}
		}, true);
}
