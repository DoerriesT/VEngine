#include "FourierOpacityPSPass.h"
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

void VEngine::FourierOpacityPSPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *ssboBuffer = data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[commonData->m_curResIdx].get();

	// light info
	DescriptorBufferInfo lightBufferInfo{ nullptr, 0, sizeof(LightInfo) * data.m_drawCount, sizeof(LightInfo) };
	uint8_t *lightDataPtr = nullptr;
	ssboBuffer->allocate(lightBufferInfo.m_range, lightBufferInfo.m_offset, lightBufferInfo.m_buffer, lightDataPtr);

	for (size_t i = 0; i < data.m_drawCount; ++i)
	{
		const auto &info = data.m_drawInfo[i];

		LightInfo lightInfo;
		lightInfo.viewProjection = info.m_shadowMatrix;
		lightInfo.invViewProjection = glm::inverse(info.m_shadowMatrix);
		lightInfo.position = info.m_lightPosition;
		lightInfo.radius = info.m_lightRadius;
		lightInfo.texelSize = 1.0f / info.m_size;
		lightInfo.resolution = info.m_size;
		lightInfo.offsetX = info.m_offsetX;
		lightInfo.offsetY = info.m_offsetY;
		lightInfo.isPointLight = info.m_pointLight;

		((LightInfo *)lightDataPtr)[i] = lightInfo;
	}

	// volume transforms
	DescriptorBufferInfo volumeTransformBufferInfo{ nullptr, 0, sizeof(float4) * 3 * commonData->m_localParticipatingMediaCount };
	uint8_t *volumeTransformDataPtr = nullptr;
	ssboBuffer->allocate(volumeTransformBufferInfo.m_range, volumeTransformBufferInfo.m_offset, volumeTransformBufferInfo.m_buffer, volumeTransformDataPtr);

	for (size_t i = 0; i < commonData->m_localParticipatingMediaCount; ++i)
	{
		const auto &volume = data.m_lightData->m_localParticipatingMedia[data.m_lightData->m_localMediaOrder[i]];

		glm::mat4 worldToLocal = glm::transpose(glm::mat4(volume.m_worldToLocal0, volume.m_worldToLocal1, volume.m_worldToLocal2, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
		glm::mat4 localToWorldTranspose = glm::transpose(glm::inverse(worldToLocal));

		((float4*)volumeTransformDataPtr)[0] = localToWorldTranspose[0];
		((float4*)volumeTransformDataPtr)[1] = localToWorldTranspose[1];
		((float4*)volumeTransformDataPtr)[2] = localToWorldTranspose[2];

		volumeTransformDataPtr += 3 * sizeof(float4);
	}


	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_fom0ImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_fom1ImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
	};

	graph.addPass("Fourier Opacity PS", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipelines
			GraphicsPipeline *fomGlobalPipeline;
			DescriptorSet *fomGlobalDescriptorSet;
			GraphicsPipeline *fomVolumePipeline;
			DescriptorSet *fomVolumeDescriptorSet;
			{
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
					registry.getImageView(data.m_fom0ImageViewHandle)->getImage()->getDescription().m_format,
					registry.getImageView(data.m_fom1ImageViewHandle)->getImage()->getDescription().m_format,
				};

				// global pipeline
				{
					// create pipeline description
					GraphicsPipelineCreateInfo pipelineCreateInfo;
					GraphicsPipelineBuilder builder(pipelineCreateInfo);


					builder.setVertexShader("Resources/Shaders/hlsl/fullscreenTriangle_vs.spv");
					builder.setFragmentShader("Resources/Shaders/hlsl/fourierOpacityGlobal_ps.spv");
					builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::NONE, FrontFace::COUNTER_CLOCKWISE);
					builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
					builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
					builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

					fomGlobalPipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

					fomGlobalDescriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(fomGlobalPipeline->getDescriptorSetLayout(0));

					// update descriptor sets
					{
						DescriptorSetUpdate2 updates[] =
						{
							Initializers::structuredBuffer(&lightBufferInfo, LIGHT_INFO_BINDING),
							Initializers::structuredBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
						};

						fomGlobalDescriptorSet->update((uint32_t)std::size(updates), updates);
					}
				}

				// volume pipeline
				{
					// create pipeline description
					GraphicsPipelineCreateInfo pipelineCreateInfo;
					GraphicsPipelineBuilder builder(pipelineCreateInfo);


					builder.setVertexShader("Resources/Shaders/hlsl/fourierOpacityVolume_vs.spv");
					builder.setFragmentShader("Resources/Shaders/hlsl/fourierOpacityVolume_ps.spv");
					builder.setVertexBindingDescription({ 0, sizeof(float) * 3, VertexInputRate::VERTEX });
					builder.setVertexAttributeDescription({ 0, 0, Format::R32G32B32_SFLOAT, 0 });
					builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::FRONT_BIT, FrontFace::COUNTER_CLOCKWISE);
					builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
					builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
					builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

					fomVolumePipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

					fomVolumeDescriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(fomVolumePipeline->getDescriptorSetLayout(0));

					// update descriptor sets
					{
						DescriptorSetUpdate2 updates[] =
						{
							Initializers::structuredBuffer(&lightBufferInfo, LIGHT_INFO_BINDING),
							Initializers::structuredBuffer(&data.m_localMediaBufferInfo, LOCAL_MEDIA_BINDING),
							Initializers::structuredBuffer(&volumeTransformBufferInfo, VOLUME_TRANSFORMS_BINDING),
						};

						fomVolumeDescriptorSet->update((uint32_t)std::size(updates), updates);
					}
				}
			}

			auto &renderResources = *data.m_passRecordContext->m_renderResources;
			const uint32_t pointLightProxyMeshIndexCount = renderResources.m_pointLightProxyMeshIndexCount;
			const uint32_t pointLightProxyMeshFirstIndex = renderResources.m_pointLightProxyMeshFirstIndex;
			const uint32_t pointLightProxyMeshVertexOffset = renderResources.m_pointLightProxyMeshVertexOffset;
			const uint32_t spotLightProxyMeshIndexCount = renderResources.m_spotLightProxyMeshIndexCount;
			const uint32_t spotLightProxyMeshFirstIndex = renderResources.m_spotLightProxyMeshFirstIndex;
			const uint32_t spotLightProxyMeshVertexOffset = renderResources.m_spotLightProxyMeshVertexOffset;
			const uint32_t boxProxyMeshIndexCount = renderResources.m_boxProxyMeshIndexCount;
			const uint32_t boxProxyMeshFirstIndex = renderResources.m_boxProxyMeshFirstIndex;
			const uint32_t boxProxyMeshVertexOffset = renderResources.m_boxProxyMeshVertexOffset;

			// iterate over all atlas tiles that need to be drawn
			for (size_t i = 0; i < data.m_drawCount; ++i)
			{
				const auto &drawInfo = data.m_drawInfo[i];

				Rect renderArea = { {static_cast<int32_t>(drawInfo.m_offsetX), static_cast<int32_t>(drawInfo.m_offsetY)}, {drawInfo.m_size, drawInfo.m_size} };

				// begin renderpass
				ColorAttachmentDescription colorAttachDescs[]
				{
					{registry.getImageView(data.m_fom0ImageViewHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, {} },
					{registry.getImageView(data.m_fom1ImageViewHandle), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, {} },
				};
				cmdList->beginRenderPass(2, colorAttachDescs, nullptr, renderArea);
				{
					// render global media
					{
						cmdList->bindPipeline(fomGlobalPipeline);

						Viewport viewport{ static_cast<float>(drawInfo.m_offsetX), static_cast<float>(drawInfo.m_offsetY), static_cast<float>(drawInfo.m_size), static_cast<float>(drawInfo.m_size), 0.0f, 1.0f };
						cmdList->setViewport(0, 1, &viewport);
						cmdList->setScissor(0, 1, &renderArea);

						DescriptorSet *sets[]{ fomGlobalDescriptorSet, renderResources.m_texture3DDescriptorSet, renderResources.m_computeSamplerDescriptorSet };
						cmdList->bindDescriptorSets(fomGlobalPipeline, 0, 3, sets);

						PushConsts pushConsts;
						pushConsts.lightIndex = (uint32_t)i;
						pushConsts.globalMediaCount = commonData->m_globalParticipatingMediaCount;

						cmdList->pushConstants(fomGlobalPipeline, ShaderStageFlagBits::FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

						cmdList->draw(3, 1, 0, 0);
					}

					// render local media
					{
						cmdList->bindPipeline(fomVolumePipeline);

						Viewport viewport{ static_cast<float>(drawInfo.m_offsetX), static_cast<float>(drawInfo.m_offsetY), static_cast<float>(drawInfo.m_size), static_cast<float>(drawInfo.m_size), 0.0f, 1.0f };
						cmdList->setViewport(0, 1, &viewport);
						cmdList->setScissor(0, 1, &renderArea);

						cmdList->bindIndexBuffer(renderResources.m_lightProxyIndexBuffer, 0, IndexType::UINT16);
						uint64_t vertexOffset = 0;
						cmdList->bindVertexBuffers(0, 1, &renderResources.m_lightProxyVertexBuffer, &vertexOffset);

						DescriptorSet *sets[]{ fomVolumeDescriptorSet, renderResources.m_texture3DDescriptorSet, renderResources.m_computeSamplerDescriptorSet };
						cmdList->bindDescriptorSets(fomVolumePipeline, 0, 3, sets);

						// iterate over all volumes
						for (uint32_t j = 0; j < commonData->m_localParticipatingMediaCount; ++j)
						{
							PushConsts2 pushConsts;
							pushConsts.lightIndex = (uint32_t)i;
							pushConsts.volumeIndex = j;

							cmdList->pushConstants(fomVolumePipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

							bool spherical = data.m_lightData->m_localParticipatingMedia[data.m_lightData->m_localMediaOrder[j]].m_spherical;
							uint32_t indexCount = spherical ? pointLightProxyMeshIndexCount : boxProxyMeshIndexCount;
							uint32_t firstIndex = spherical ? pointLightProxyMeshFirstIndex : boxProxyMeshFirstIndex;
							uint32_t vertexOffset = spherical ? pointLightProxyMeshVertexOffset : boxProxyMeshVertexOffset;

							cmdList->drawIndexed(indexCount, 1, firstIndex, vertexOffset, 0);
						}
					}
				}
				cmdList->endRenderPass();
			}
		}, true);
}
