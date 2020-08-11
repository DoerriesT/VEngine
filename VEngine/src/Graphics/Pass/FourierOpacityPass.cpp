#include "FourierOpacityPass.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/RenderResources.h"
#include "Graphics/LightData.h"
#include <glm/gtx/transform.hpp>
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <glm/vec3.hpp>
#include <glm/ext.hpp>
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/fourierOpacityVolume.hlsli"
}

void VEngine::FourierOpacityPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *ssboBuffer = data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[commonData->m_curResIdx].get();

	// light info
	DescriptorBufferInfo lightBufferInfo{ nullptr, 0, sizeof(LightInfo) * data.m_drawCount };
	uint8_t *lightDataPtr = nullptr;
	ssboBuffer->allocate(lightBufferInfo.m_range, lightBufferInfo.m_offset, lightBufferInfo.m_buffer, lightDataPtr);

	for (size_t i = 0; i < data.m_drawCount; ++i)
	{
		const auto &info = data.m_drawInfo[i];

		uint32_t resolution = info.m_pointLight ? (info.m_size - 2) : info.m_size;

		LightInfo lightInfo;
		lightInfo.viewProjection = info.m_shadowMatrix;
		lightInfo.invViewProjection = glm::inverse(info.m_shadowMatrix);
		lightInfo.position = info.m_lightPosition;
		lightInfo.radius = info.m_lightRadius;
		lightInfo.texelSize = 1.0f / resolution;
		lightInfo.resolution = resolution;
		lightInfo.offsetX = info.m_offsetX + (info.m_pointLight ? 1 : 0);
		lightInfo.offsetY = info.m_offsetY + (info.m_pointLight ? 1 : 0);
		lightInfo.isPointLight = info.m_pointLight;

		((LightInfo *)lightDataPtr)[i] = lightInfo;
	}


	//rg::ImageViewHandle tempImageViewHandle;
	//{
	//	rg::ImageDescription desc = {};
	//	desc.m_name = "FOM temp Image";
	//	desc.m_clear = false;
	//	desc.m_clearValue.m_imageClearValue = {};
	//	desc.m_width = 256;
	//	desc.m_height = 256;
	//	desc.m_layers = 2;
	//	desc.m_levels = 1;
	//	desc.m_samples = SampleCount::_1;
	//	desc.m_format = Format::R16G16B16A16_SFLOAT;
	//
	//	tempImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 2 }, ImageViewType::_2D_ARRAY });
	//}

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_fomImageViewHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		//{rg::ResourceViewHandle(tempImageViewHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}, true, {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Fourier Opacity", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// render volume opacity
			{
				// create pipeline description
				ComputePipelineCreateInfo pipelineCreateInfo;
				ComputePipelineBuilder builder(pipelineCreateInfo);
				builder.setComputeShader("Resources/Shaders/hlsl/fourierOpacityVolume_cs.spv");

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *resultImageView = registry.getImageView(data.m_fomImageViewHandle);

					DescriptorSetUpdate updates[] =
					{
						Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
						Initializers::storageBuffer(&lightBufferInfo, LIGHT_INFO_BINDING),
						Initializers::storageBuffer(&data.m_localMediaBufferInfo, LOCAL_MEDIA_BINDING),
						Initializers::storageBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
						Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX], LINEAR_SAMPLER_BINDING),
					};

					descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

					DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTexture3DDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
				}

				PushConsts pushConsts;
				pushConsts.lightIndex = 0;
				pushConsts.globalMediaCount = commonData->m_globalParticipatingMediaCount;
				pushConsts.localVolumeCount = commonData->m_localParticipatingMediaCount;

				for (size_t i = 0; i < data.m_drawCount; ++i)
				{
					pushConsts.lightIndex = static_cast<uint32_t>(i);
					cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

					const auto &info = data.m_drawInfo[i];
					uint32_t res = info.m_pointLight ? (info.m_size - 2) : info.m_size;
					cmdList->dispatch((res + 3) / 4, (res + 3) / 4, 1);
				}
			}

			// rasterize particles
			if (data.m_particleCount)
			{
				// barrier on fom atlas to wait for volumetrics
				{
					Barrier barrier = Initializers::imageBarrier(registry.getImage(data.m_fomImageViewHandle),
						PipelineStageFlagBits::COMPUTE_SHADER_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						gal::ResourceState::WRITE_STORAGE_IMAGE, gal::ResourceState::READ_WRITE_STORAGE_IMAGE, { 0, 1, 0, 2 });

					cmdList->barrier(1, &barrier);
				}


				// create pipeline description
				ComputePipelineCreateInfo pipelineCreateInfo;
				ComputePipelineBuilder builder(pipelineCreateInfo);
				builder.setComputeShader("Resources/Shaders/hlsl/fourierOpacityParticle_cs.spv");

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *resultImageView = registry.getImageView(data.m_fomImageViewHandle);

					DescriptorSetUpdate updates[] =
					{
						Initializers::storageImage(&resultImageView, 0),
						Initializers::storageBuffer(&lightBufferInfo, 1),
						Initializers::storageBuffer(&data.m_particleBufferInfo, 2),
					};

					descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

					DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);
				}

				struct FOMParticlePushConsts
				{
					uint lightIndex;
					uint particleCount;
				};

				FOMParticlePushConsts pushConsts;
				pushConsts.lightIndex = 0;
				pushConsts.particleCount = data.m_particleCount;

				for (size_t i = 0; i < data.m_drawCount; ++i)
				{
					pushConsts.lightIndex = static_cast<uint32_t>(i);
					cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

					const auto &info = data.m_drawInfo[i];
					uint32_t res = info.m_pointLight ? (info.m_size - 2) : info.m_size;
					cmdList->dispatch((res + 7) / 8, (res + 7) / 8, 1);
				}
			}

			//// blur textures
			//if (g_blur)
			//{
			//	// create pipeline description
			//	ComputePipelineCreateInfo pipelineCreateInfo;
			//	ComputePipelineBuilder builder(pipelineCreateInfo);
			//	builder.setComputeShader("Resources/Shaders/hlsl/fourierOpacityBlur_cs.spv");
			//
			//	auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);
			//
			//	cmdList->bindPipeline(pipeline);
			//
			//	DescriptorSet *descriptorSet0 = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));
			//	DescriptorSet *descriptorSet1 = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));
			//
			//	Sampler *linearSampler = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX];
			//
			//	// update descriptor sets
			//	{
			//		ImageView *fomImageView = registry.getImageView(data.m_fomImageViewHandle);
			//		ImageView *tmpImageView = registry.getImageView(tempImageViewHandle);
			//
			//		DescriptorSetUpdate updates0[] =
			//		{
			//			Initializers::storageImage(&tmpImageView, 0),
			//			Initializers::sampledImage(&fomImageView, 1),
			//			Initializers::samplerDescriptor(&linearSampler, 2),
			//		};
			//
			//		DescriptorSetUpdate updates1[] =
			//		{
			//			Initializers::storageImage(&fomImageView, 0),
			//			Initializers::sampledImage(&tmpImageView, 1),
			//			Initializers::samplerDescriptor(&linearSampler, 2),
			//		};
			//
			//		descriptorSet0->update(sizeof(updates0) / sizeof(updates0[0]), updates0);
			//		descriptorSet1->update(sizeof(updates1) / sizeof(updates1[0]), updates1);
			//	}
			//
			//	struct BlurPushConsts
			//	{
			//		glm::vec2 offset;
			//		uint resolution;
			//		float texelSize;
			//		uint horizontal;
			//	};
			//
			//	// iterate over all drawn tiles and blur them
			//	for (size_t i = 0; i < data.m_drawCount; ++i)
			//	{
			//		const auto &info = data.m_drawInfo[i];
			//
			//		// transition images
			//		{
			//			// fom goes from write to read
			//			Barrier fom = Initializers::imageBarrier(registry.getImage(data.m_fomImageViewHandle),
			//				PipelineStageFlagBits::COMPUTE_SHADER_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT,
			//				gal::ResourceState::WRITE_STORAGE_IMAGE, gal::ResourceState::READ_TEXTURE, { 0, 1, 0, 2 });
			//
			//			// tmp goes from read to write
			//			Barrier tmp = Initializers::imageBarrier(registry.getImage(tempImageViewHandle),
			//				PipelineStageFlagBits::COMPUTE_SHADER_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT,
			//				gal::ResourceState::READ_TEXTURE, gal::ResourceState::WRITE_STORAGE_IMAGE, { 0, 1, 0, 2 });
			//
			//			// no need to transition temp images on first iteration as they are already in the correct state
			//			Barrier barriers[]{ fom, tmp };
			//			cmdList->barrier(i == 0 ? 1 : 2, barriers);
			//		}
			//
			//		// source to temp image
			//		{
			//			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet0);
			//
			//			BlurPushConsts pushConsts;
			//			pushConsts.texelSize = 1.0f / 2048.0f;
			//			pushConsts.resolution = info.m_size;
			//			pushConsts.offset = glm::vec2(info.m_offsetX, info.m_offsetY) / 2048.0f;
			//			pushConsts.horizontal = true;
			//
			//			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);
			//
			//			cmdList->dispatch((info.m_size + 7) / 8, (info.m_size + 7) / 8, 1);
			//		}
			//
			//		// transition images
			//		{
			//			// fom goes from read to write
			//			Barrier fom = Initializers::imageBarrier(registry.getImage(data.m_fomImageViewHandle),
			//				PipelineStageFlagBits::COMPUTE_SHADER_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT,
			//				gal::ResourceState::READ_TEXTURE, gal::ResourceState::WRITE_STORAGE_IMAGE, { 0, 1, 0, 2 });
			//
			//			// tmp goes from write to read
			//			Barrier tmp = Initializers::imageBarrier(registry.getImage(tempImageViewHandle),
			//				PipelineStageFlagBits::COMPUTE_SHADER_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT,
			//				gal::ResourceState::WRITE_STORAGE_IMAGE, gal::ResourceState::READ_TEXTURE, { 0, 1, 0, 2 });
			//
			//			Barrier barriers[]{ fom, tmp };
			//			cmdList->barrier(2, barriers);
			//		}
			//
			//		// temp image to dest mip
			//		{
			//			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet1);
			//
			//			BlurPushConsts pushConsts;
			//			pushConsts.texelSize = 1.0f / 256.0f;
			//			pushConsts.resolution = info.m_size;
			//			pushConsts.offset = glm::vec2(0.0f);
			//			pushConsts.horizontal = false;
			//
			//			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);
			//
			//			cmdList->dispatch((info.m_size + 7) / 8, (info.m_size + 7) / 8, 1);
			//		}
			//	}
			//}
		}, true);
}
