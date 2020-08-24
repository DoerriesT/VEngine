#include "VolumetricFogFilterPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include <glm/mat4x4.hpp>
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"
#include "Graphics/MappableBufferBlock.h"

using namespace VEngine::gal;

extern bool g_fogClamping;
extern bool g_fogPrevFrameCombine;
extern bool g_fogHistoryCombine;
extern float g_fogHistoryAlpha;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/volumetricFogFilter.hlsli"
}

void VEngine::VolumetricFogFilterPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ImageDescription desc = {};
	desc.m_name = "Volumetric Fog Tmp In-Scattering Image";
	desc.m_clear = false;
	desc.m_clearValue.m_imageClearValue = {};
	desc.m_width = (data.m_passRecordContext->m_commonRenderData->m_width + 7) / 8;
	desc.m_height = (data.m_passRecordContext->m_commonRenderData->m_height + 7) / 8;
	desc.m_depth = 64;
	desc.m_layers = 1;
	desc.m_levels = 1;
	desc.m_samples = SampleCount::_1;
	desc.m_imageType = ImageType::_3D;
	desc.m_format = Format::R16G16B16A16_SFLOAT;

	
	rg::ImageViewHandle tmpImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), { gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(tmpImageViewHandle), { gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }, true, { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_historyImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_prevImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_inputImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		{rg::ResourceViewHandle(data.m_exposureDataBufferHandle), {gal::ResourceState::READ_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *uboBuffer = data.m_passRecordContext->m_renderResources->m_mappableUBOBlock[commonData->m_curResIdx].get();

	DescriptorBufferInfo uboBufferInfo{ nullptr, 0, sizeof(Constants) };
	uint8_t *uboDataPtr = nullptr;
	uboBuffer->allocate(uboBufferInfo.m_range, uboBufferInfo.m_offset, uboBufferInfo.m_buffer, uboDataPtr);

	Constants consts;
	consts.prevViewMatrix = commonData->m_prevViewMatrix;
	consts.prevProjMatrix = commonData->m_prevProjectionMatrix;
	consts.reprojectedTexCoordScaleBias = *(glm::vec4 *)data.m_reprojectedTexCoordScaleBias;
	consts.frustumCornerTL = { data.m_frustumCorners[0][0], data.m_frustumCorners[0][1], data.m_frustumCorners[0][2] };
	consts.frustumCornerTR = { data.m_frustumCorners[1][0], data.m_frustumCorners[1][1], data.m_frustumCorners[1][2] };
	consts.frustumCornerBL = { data.m_frustumCorners[2][0], data.m_frustumCorners[2][1], data.m_frustumCorners[2][2] };
	consts.frustumCornerBR = { data.m_frustumCorners[3][0], data.m_frustumCorners[3][1], data.m_frustumCorners[3][2] };
	consts.cameraPos = commonData->m_cameraPosition;
	consts.ignoreHistory = data.m_ignoreHistory;


	memcpy(uboDataPtr, &consts, sizeof(consts));

	graph.addPass("Volumetric Fog Filter", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/volumetricFogFilter_cs");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			const auto &imageDesc = registry.getImage(data.m_resultImageViewHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;
			uint32_t d = imageDesc.m_depth;

			// combine previous frame's inscattering with current
			{
				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *resultImageView = registry.getImageView(tmpImageViewHandle);
					ImageView *historyImageView = registry.getImageView(data.m_prevImageViewHandle);
					ImageView *inputImageView = registry.getImageView(data.m_inputImageViewHandle);
					DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
						Initializers::texture(&historyImageView, HISTORY_IMAGE_BINDING),
						Initializers::texture(&inputImageView, INPUT_IMAGE_BINDING),
						Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
						Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);

					DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
				}

				PushConsts pushConsts;
				pushConsts.alpha = g_fogPrevFrameCombine ? 0.5f : 1.0f;
				pushConsts.neighborHoodClamping = 0;
				pushConsts.advancedFilter = 0;
				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				cmdList->dispatch((w + 7) / 8, (h + 7) / 8, d);
			}

			// transition temp image
			{
				Barrier barrier = Initializers::imageBarrier(registry.getImage(tmpImageViewHandle),
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					gal::ResourceState::WRITE_RW_IMAGE,
					gal::ResourceState::READ_TEXTURE,
					{ 0, 1, 0, 1 });

				cmdList->barrier(1, &barrier);
			}

			// combine temp inscattering with exponential history
			{
				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);
					ImageView *historyImageView = registry.getImageView(data.m_historyImageViewHandle);
					ImageView *inputImageView = registry.getImageView(tmpImageViewHandle);
					DescriptorBufferInfo exposureDataBufferInfo = registry.getBufferInfo(data.m_exposureDataBufferHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
						Initializers::texture(&historyImageView, HISTORY_IMAGE_BINDING),
						Initializers::texture(&inputImageView, INPUT_IMAGE_BINDING),
						Initializers::constantBuffer(&uboBufferInfo, CONSTANT_BUFFER_BINDING),
						Initializers::byteBuffer(&exposureDataBufferInfo, EXPOSURE_DATA_BUFFER_BINDING),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);

					DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
				}

				PushConsts pushConsts;
				pushConsts.alpha = g_fogHistoryCombine ? g_fogHistoryAlpha : 1.0f;
				pushConsts.neighborHoodClamping = g_fogClamping;
				pushConsts.advancedFilter = 1;
				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				cmdList->dispatch((w + 7) / 8, (h + 7) / 8, d);
			}
		});
}
