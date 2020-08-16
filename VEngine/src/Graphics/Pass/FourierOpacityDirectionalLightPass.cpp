#include "FourierOpacityDirectionalLightPass.h"
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
#include "Components/DirectionalLightComponent.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/fourierOpacityParticleDirectional.hlsli"
}

void VEngine::FourierOpacityDirectionalLightPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	assert(data.m_lightDataCount <= 2);

	rg::ResourceUsageDescription passUsages[2 * DirectionalLightComponent::MAX_CASCADES * 2 + 1];
	rg::ImageViewHandle fomViewHandles[2 * DirectionalLightComponent::MAX_CASCADES * 2];
	uint32_t viewHandleCount = 0;

	for (size_t i = 0; i < data.m_lightDataCount; ++i)
	{
		for (size_t j = 0; j < data.m_lightData[i].m_shadowCount; ++j)
		{
			for (size_t k = 0; k < 2; ++k)
			{
				fomViewHandles[viewHandleCount] = graph.createImageView({ "FOM Directional Light Layer", data.m_directionalLightFomImageHandle, { 0, 1, viewHandleCount, 1 }, ImageViewType::_2D });
				passUsages[viewHandleCount] = { rg::ResourceViewHandle(fomViewHandles[viewHandleCount]), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT} };
				++viewHandleCount;
			}
		}
	}

	passUsages[viewHandleCount++] = { rg::ResourceViewHandle(data.m_fomDepthRangeImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::FRAGMENT_SHADER_BIT} };

	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *ssboBuffer = data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[commonData->m_curResIdx].get();

	// direction info
	DescriptorBufferInfo dirBufferInfo{ nullptr, 0, sizeof(glm::vec4) * 2 * data.m_lightDataCount };
	uint8_t *dirDataPtr = nullptr;
	ssboBuffer->allocate(dirBufferInfo.m_range, dirBufferInfo.m_offset, dirBufferInfo.m_buffer, dirDataPtr);

	for (size_t i = 0; i < data.m_lightDataCount; ++i)
	{
		const auto &light = data.m_lightData[i];

		glm::vec3 worldSpaceDir = glm::normalize(glm::vec3(commonData->m_invViewMatrix * glm::vec4(light.m_direction, 0.0f)));

		glm::vec3 upDir(0.0f, 1.0f, 0.0f);

		// choose different up vector if light direction would be linearly dependent otherwise
		if (abs(worldSpaceDir.x) < 0.001f && abs(worldSpaceDir.z) < 0.001f)
		{
			upDir = glm::vec3(1.0f, 1.0f, 0.0f);
		}

		((glm::vec4 *)dirDataPtr)[i * 2] = glm::vec4(worldSpaceDir, 0.0f);
		((glm::vec4 *)dirDataPtr)[i * 2 + 1] = glm::vec4(glm::normalize(glm::cross(worldSpaceDir, glm::normalize(upDir))), 0.0f);
	}


	graph.addPass("Fourier Opacity Directional Light", rg::QueueType::GRAPHICS, viewHandleCount, passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };

			PipelineColorBlendAttachmentState additiveBlending{};
			additiveBlending.m_blendEnable = true;
			additiveBlending.m_srcColorBlendFactor = BlendFactor::ONE;
			additiveBlending.m_dstColorBlendFactor = BlendFactor::ONE;
			additiveBlending.m_colorBlendOp = BlendOp::ADD;
			additiveBlending.m_srcAlphaBlendFactor = BlendFactor::ONE;
			additiveBlending.m_dstAlphaBlendFactor = BlendFactor::ONE;
			additiveBlending.m_alphaBlendOp = BlendOp::ADD;
			additiveBlending.m_colorWriteMask = ColorComponentFlagBits::ALL_BITS;

			PipelineColorBlendAttachmentState colorBlendAttachments[]
			{
				additiveBlending, additiveBlending
			};

			Format colorAttachmentFormats[]
			{
				registry.getImageView(fomViewHandles[0])->getImage()->getDescription().m_format,
				registry.getImageView(fomViewHandles[1])->getImage()->getDescription().m_format,
			};

			// create pipeline description
			GraphicsPipelineCreateInfo pipelineCreateInfo;
			GraphicsPipelineBuilder builder(pipelineCreateInfo);
			builder.setVertexShader("Resources/Shaders/hlsl/fourierOpacityParticleDirectional_vs.spv");
			builder.setFragmentShader("Resources/Shaders/hlsl/fourierOpacityParticleDirectional_ps.spv");
			builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
			builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
			builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
			builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				ImageView *fomDepthRangeImageView = registry.getImageView(data.m_fomDepthRangeImageViewHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageBuffer(&data.m_particleBufferInfo, PARTICLES_BINDING),
					Initializers::storageBuffer(&data.m_shadowMatrixBufferInfo, MATRIX_BUFFER_BINDING),
					Initializers::storageBuffer(&dirBufferInfo, LIGHT_DIR_BUFFER_BINDING),
					Initializers::sampledImage(&fomDepthRangeImageView, DEPTH_RANGE_IMAGE_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);
			}

			const auto &imageDesc = registry.getImage(data.m_directionalLightFomImageHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;

			for (size_t i = 0; i < data.m_lightDataCount; ++i)
			{
				size_t currentLayerOffset = 0;
				for (size_t j = 0; j < data.m_lightData[i].m_shadowCount; ++j)
				{
					// begin renderpass
					ColorAttachmentDescription colorAttachDescs[]
					{
						{registry.getImageView(fomViewHandles[currentLayerOffset]), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, {} },
						{registry.getImageView(fomViewHandles[currentLayerOffset + 1]), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, {} },
					};
					cmdList->beginRenderPass(sizeof(colorAttachDescs) / sizeof(colorAttachDescs[0]), colorAttachDescs, nullptr, { {}, {w, h} });
					{
						cmdList->bindPipeline(pipeline);

						DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
						cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);

						Viewport viewport{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
						Rect scissor{ { 0, 0 }, { w, h } };

						cmdList->setViewport(0, 1, &viewport);
						cmdList->setScissor(0, 1, &scissor);

						uint32_t particleOffset = 0;
						for (size_t k = 0; k < data.m_listCount; ++k)
						{
							if (data.m_listSizes[k])
							{
								PushConsts pushConsts;
								pushConsts.shadowMatrixIndex = data.m_lightData[i].m_shadowOffset + (uint32_t)j;
								pushConsts.directionIndex = (uint32_t)i;
								pushConsts.particleOffset = particleOffset;

								cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);
								cmdList->draw(6 * data.m_listSizes[k], 1, 0, 0);
								particleOffset += data.m_listSizes[k];
							}
						}
					}
					cmdList->endRenderPass();

					currentLayerOffset += 2;
				}
			}
		}, true);
}
