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
	assert(data.m_lightData->m_directionalLightsShadowed.size() <= 2);

	rg::ResourceUsageDescription passUsages[2 * DirectionalLightComponent::MAX_CASCADES * 2 + 1];
	rg::ImageViewHandle fomViewHandles[2 * DirectionalLightComponent::MAX_CASCADES * 2];
	uint32_t viewHandleCount = 0;

	for (size_t i = 0; i < data.m_lightData->m_directionalLightsShadowed.size(); ++i)
	{
		for (size_t j = 0; j < data.m_lightData->m_directionalLightsShadowed[i].m_shadowCount; ++j)
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
	DescriptorBufferInfo dirBufferInfo{ nullptr, 0, sizeof(glm::vec4) * 2 * data.m_lightData->m_directionalLightsShadowed.size(), sizeof(glm::vec4) };
	uint64_t alignment = graph.getGraphicsDevice()->getBufferAlignment(DescriptorType2::STRUCTURED_BUFFER, sizeof(glm::vec4));
	uint8_t *dirDataPtr = nullptr;
	ssboBuffer->allocate(alignment, dirBufferInfo.m_range, dirBufferInfo.m_offset, dirBufferInfo.m_buffer, dirDataPtr);

	for (size_t i = 0; i < data.m_lightData->m_directionalLightsShadowed.size(); ++i)
	{
		const auto &light = data.m_lightData->m_directionalLightsShadowed[i];

		glm::vec3 upDir(0.0f, 1.0f, 0.0f);

		// choose different up vector if light direction would be linearly dependent otherwise
		if (abs(light.m_direction.x) < 0.001f && abs(light.m_direction.z) < 0.001f)
		{
			upDir = glm::vec3(1.0f, 1.0f, 0.0f);
		}

		((glm::vec4 *)dirDataPtr)[i * 2] = glm::vec4(light.m_direction, 0.0f);
		((glm::vec4 *)dirDataPtr)[i * 2 + 1] = glm::vec4(glm::normalize(glm::cross(light.m_direction, glm::normalize(upDir))), 0.0f);
	}

	// transform buffer info
	DescriptorBufferInfo volumeTransformBufferInfo{ nullptr, 0, sizeof(glm::vec4) * 3 * glm::max(commonData->m_localParticipatingMediaCount, 1u), sizeof(glm::vec4) };
	uint8_t *transformDataPtr = nullptr;
	ssboBuffer->allocate(alignment, volumeTransformBufferInfo.m_range, volumeTransformBufferInfo.m_offset, volumeTransformBufferInfo.m_buffer, transformDataPtr);

	for (size_t i = 0; i < data.m_lightData->m_localParticipatingMedia.size(); ++i)
	{
		glm::mat4 transform = glm::transpose(data.m_lightData->m_localMediaTransforms[data.m_lightData->m_localMediaOrder[i]]);
		((glm::vec4 *)transformDataPtr)[0] = transform[0];
		((glm::vec4 *)transformDataPtr)[1] = transform[1];
		((glm::vec4 *)transformDataPtr)[2] = transform[2];
		transformDataPtr += sizeof(glm::vec4) * 3;
	}


	graph.addPass("FOM Directional Lights", rg::QueueType::GRAPHICS, viewHandleCount, passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GraphicsPipeline *particlePipeline;
			DescriptorSet *particleDescriptorSet;
			GraphicsPipeline *volumePipeline;
			DescriptorSet *volumeDescriptorSet;

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

			// particle pipeline
			{
				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);
				builder.setVertexShader("Resources/Shaders/hlsl/fourierOpacityParticleDirectional_vs");
				builder.setFragmentShader("Resources/Shaders/hlsl/fourierOpacityParticleDirectional_ps");
				builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
				builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
				builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
				builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

				particlePipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				particleDescriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(particlePipeline->getDescriptorSetLayout(0));

				// update descriptor sets
				{
					ImageView *fomDepthRangeImageView = registry.getImageView(data.m_fomDepthRangeImageViewHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::structuredBuffer(&data.m_particleBufferInfo, PARTICLES_BINDING),
						Initializers::structuredBuffer(&data.m_shadowMatrixBufferInfo, MATRIX_BUFFER_BINDING),
						Initializers::structuredBuffer(&dirBufferInfo, LIGHT_DIR_BUFFER_BINDING),
						Initializers::texture(&fomDepthRangeImageView, DEPTH_RANGE_IMAGE_BINDING),
					};

					particleDescriptorSet->update((uint32_t)std::size(updates), updates);
				}
			}

			// volume pipeline
			{
				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);
				builder.setVertexShader("Resources/Shaders/hlsl/fourierOpacityVolumeDirectional_vs");
				builder.setFragmentShader("Resources/Shaders/hlsl/fourierOpacityVolumeDirectional_ps");
				builder.setVertexBindingDescription({ 0, sizeof(float) * 3, VertexInputRate::VERTEX });
				builder.setVertexAttributeDescription({ "POSITION", 0, 0, Format::R32G32B32_SFLOAT, 0 });
				builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::FRONT_BIT, FrontFace::COUNTER_CLOCKWISE);
				builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
				builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
				builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

				volumePipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				volumeDescriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(volumePipeline->getDescriptorSetLayout(0));

				// update descriptor sets
				{
					ImageView *fomDepthRangeImageView = registry.getImageView(data.m_fomDepthRangeImageViewHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::structuredBuffer(&volumeTransformBufferInfo, 0),
						Initializers::structuredBuffer(&data.m_shadowMatrixBufferInfo, 1),
						Initializers::structuredBuffer(&data.m_localMediaBufferInfo, 2),
						Initializers::texture(&fomDepthRangeImageView, 3),
					};

					volumeDescriptorSet->update((uint32_t)std::size(updates), updates);
				}
			}


			const auto &imageDesc = registry.getImage(data.m_directionalLightFomImageHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;

			const uint32_t pointLightProxyMeshIndexCount = data.m_passRecordContext->m_renderResources->m_pointLightProxyMeshIndexCount;
			const uint32_t pointLightProxyMeshFirstIndex = data.m_passRecordContext->m_renderResources->m_pointLightProxyMeshFirstIndex;
			const uint32_t pointLightProxyMeshVertexOffset = data.m_passRecordContext->m_renderResources->m_pointLightProxyMeshVertexOffset;
			const uint32_t boxProxyMeshIndexCount = data.m_passRecordContext->m_renderResources->m_boxProxyMeshIndexCount;
			const uint32_t boxProxyMeshFirstIndex = data.m_passRecordContext->m_renderResources->m_boxProxyMeshFirstIndex;
			const uint32_t boxProxyMeshVertexOffset = data.m_passRecordContext->m_renderResources->m_boxProxyMeshVertexOffset;


			for (size_t lightIdx = 0; lightIdx < data.m_lightData->m_directionalLightsShadowed.size(); ++lightIdx)
			{
				const auto &light = data.m_lightData->m_directionalLightsShadowed[lightIdx];
				size_t currentLayerOffset = 0;
				for (size_t cascadeIdx = 0; cascadeIdx < light.m_shadowCount; ++cascadeIdx)
				{
					// begin renderpass
					ColorAttachmentDescription colorAttachDescs[]
					{
						{registry.getImageView(fomViewHandles[currentLayerOffset]), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, {} },
						{registry.getImageView(fomViewHandles[currentLayerOffset + 1]), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, {} },
					};
					cmdList->beginRenderPass(sizeof(colorAttachDescs) / sizeof(colorAttachDescs[0]), colorAttachDescs, nullptr, { {}, {w, h} }, false);
					{
						Viewport viewport{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
						Rect scissor{ { 0, 0 }, { w, h } };

						cmdList->setViewport(0, 1, &viewport);
						cmdList->setScissor(0, 1, &scissor);

						// particles
						{
							cmdList->bindPipeline(particlePipeline);

							DescriptorSet *descriptorSets[] = 
							{ 
								particleDescriptorSet, 
								data.m_passRecordContext->m_renderResources->m_textureDescriptorSet, 
								data.m_passRecordContext->m_renderResources->m_samplerDescriptorSet 
							};
							cmdList->bindDescriptorSets(particlePipeline, 0, 3, descriptorSets);

							uint32_t particleOffset = 0;
							for (size_t particleListIdx = 0; particleListIdx < data.m_listCount; ++particleListIdx)
							{
								if (data.m_listSizes[particleListIdx])
								{
									PushConsts pushConsts;
									pushConsts.shadowMatrixIndex = light.m_shadowOffset + (uint32_t)cascadeIdx;
									pushConsts.directionIndex = (uint32_t)lightIdx;
									pushConsts.particleOffset = particleOffset;

									cmdList->pushConstants(particlePipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);
									cmdList->draw(6 * data.m_listSizes[particleListIdx], 1, 0, 0);
									particleOffset += data.m_listSizes[particleListIdx];
								}
							}
						}

						// volumes
						{
							cmdList->bindPipeline(volumePipeline);

							DescriptorSet *descriptorSets[] = 
							{ 
								volumeDescriptorSet, 
								data.m_passRecordContext->m_renderResources->m_texture3DDescriptorSet,
								data.m_passRecordContext->m_renderResources->m_samplerDescriptorSet
							};
							cmdList->bindDescriptorSets(volumePipeline, 0, 3, descriptorSets);

							cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_lightProxyIndexBuffer, 0, IndexType::UINT16);

							uint64_t vertexOffset = 0;
							cmdList->bindVertexBuffers(0, 1, &data.m_passRecordContext->m_renderResources->m_lightProxyVertexBuffer, &vertexOffset);

							struct PushConsts2
							{
								float3 rayDir;
								float invLightRange;
								float3 topLeft;
								uint volumeIndex;
								float3 deltaX;
								uint shadowMatrixIndex;
								float3 deltaY;
							};

							

							glm::mat4 invShadowMatrix = glm::inverse(data.m_shadowMatrices[light.m_shadowOffset + (uint32_t)cascadeIdx]);

							PushConsts2 pushConsts;
							pushConsts.rayDir = -glm::normalize(glm::vec3(light.m_direction));
							pushConsts.invLightRange = 1.0f / 300.0f;
							pushConsts.topLeft = invShadowMatrix * glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f);
							pushConsts.deltaX = (glm::vec3(invShadowMatrix * glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)) - pushConsts.topLeft) / w;
							pushConsts.deltaY = (glm::vec3(invShadowMatrix * glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f)) - pushConsts.topLeft) / h;

							for (size_t mediaIdx = 0; mediaIdx < data.m_lightData->m_localParticipatingMedia.size(); ++mediaIdx)
							{
								pushConsts.shadowMatrixIndex = light.m_shadowOffset + (uint32_t)cascadeIdx;
								pushConsts.volumeIndex = data.m_lightData->m_localMediaOrder[mediaIdx];

								cmdList->pushConstants(volumePipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

								bool spherical = data.m_lightData->m_localParticipatingMedia[data.m_lightData->m_localMediaOrder[mediaIdx]].m_spherical;

								uint32_t indexCount = spherical ? pointLightProxyMeshIndexCount : boxProxyMeshIndexCount;
								uint32_t firstIndex = spherical ? pointLightProxyMeshFirstIndex : boxProxyMeshFirstIndex;
								uint32_t vertexOffset = spherical ? pointLightProxyMeshVertexOffset : boxProxyMeshVertexOffset;

								cmdList->drawIndexed(indexCount, 1, firstIndex, vertexOffset, 0);
							}
						}
					}
					cmdList->endRenderPass();

					currentLayerOffset += 2;
				}
			}
		}, true);
}
