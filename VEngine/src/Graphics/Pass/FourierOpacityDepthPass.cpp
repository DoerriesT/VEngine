#include "FourierOpacityDepthPass.h"
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

void VEngine::FourierOpacityDepthPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	assert(data.m_lightDataCount <= 2);

	rg::ResourceUsageDescription passUsages[2 * DirectionalLightComponent::MAX_CASCADES * 2];
	rg::ImageViewHandle fomDepthViewHandles[2 * DirectionalLightComponent::MAX_CASCADES * 2];
	uint32_t viewHandleCount = 0;

	for (size_t i = 0; i < data.m_lightDataCount; ++i)
	{
		for (size_t j = 0; j < data.m_lightData[i].m_shadowCount; ++j)
		{
			for (size_t k = 0; k < 2; ++k)
			{
				fomDepthViewHandles[viewHandleCount] = graph.createImageView({ "FOM Directional Light Depth Layer", data.m_directionalLightFomDepthImageHandle, { 0, 1, viewHandleCount, 1 }, ImageViewType::_2D });
				passUsages[viewHandleCount] = { rg::ResourceViewHandle(fomDepthViewHandles[viewHandleCount]), {gal::ResourceState::READ_WRITE_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT} };
				++viewHandleCount;
			}
		}
	}

	uint32_t particleCount = 0;
	for (size_t i = 0; i < data.m_listCount; ++i)
	{
		particleCount += data.m_listSizes[i];
	}

	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *ssboBuffer = data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[commonData->m_curResIdx].get();

	// direction info
	DescriptorBufferInfo particleTransformBufferInfo{ nullptr, 0, sizeof(glm::vec4) * 3 * particleCount };
	uint8_t *transformDataPtr = nullptr;
	ssboBuffer->allocate(particleTransformBufferInfo.m_range, particleTransformBufferInfo.m_offset, particleTransformBufferInfo.m_buffer, transformDataPtr);

	for (size_t i = 0; i < data.m_listCount; ++i)
	{
		for (size_t j = 0; j < data.m_listSizes[i]; ++j)
		{
			glm::mat4 transform = glm::transpose(glm::translate(data.m_particleLists[i][j].m_position) * glm::scale(glm::vec3(0.6f)));
			((glm::vec4 *)transformDataPtr)[0] = transform[0];
			((glm::vec4 *)transformDataPtr)[1] = transform[1];
			((glm::vec4 *)transformDataPtr)[2] = transform[2];
			transformDataPtr += sizeof(glm::vec4) * 3;
		}
	}

	graph.addPass("Fourier Opacity Depth Directional Light", rg::QueueType::GRAPHICS, viewHandleCount, passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			GraphicsPipeline *pipelines[2];
			for (size_t i = 0; i < 2; ++i)
			{
				// create pipeline description
				GraphicsPipelineCreateInfo pipelineCreateInfo;
				GraphicsPipelineBuilder builder(pipelineCreateInfo);
				gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };
				builder.setVertexShader("Resources/Shaders/hlsl/fourierOpacityDepth_vs.spv");
				builder.setVertexBindingDescription({ 0, sizeof(float) * 3, VertexInputRate::VERTEX });
				builder.setVertexAttributeDescription({ 0, 0, Format::R32G32B32_SFLOAT, 0 });
				builder.setPolygonModeCullMode(PolygonMode::FILL, i == 0 ? CullModeFlagBits::BACK_BIT : CullModeFlagBits::FRONT_BIT, FrontFace::COUNTER_CLOCKWISE);
				builder.setDepthTest(true, true, i == 0 ? CompareOp::LESS_OR_EQUAL : CompareOp::GREATER);
				builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
				builder.setDepthStencilAttachmentFormat(registry.getImage(data.m_directionalLightFomDepthImageHandle)->getDescription().m_format);

				pipelines[i] = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);
			}


			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelines[0]->getDescriptorSetLayout(0));

			// update descriptor sets
			{
				DescriptorSetUpdate updates[] =
				{
					Initializers::storageBuffer(&data.m_shadowMatrixBufferInfo, 0),
					Initializers::storageBuffer(&particleTransformBufferInfo, 1),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);
			}

			const auto &imageDesc = registry.getImage(data.m_directionalLightFomDepthImageHandle)->getDescription();
			uint32_t w = imageDesc.m_width;
			uint32_t h = imageDesc.m_height;

			const uint32_t pointLightProxyMeshIndexCount = data.m_passRecordContext->m_renderResources->m_pointLightProxyMeshIndexCount;
			const uint32_t pointLightProxyMeshFirstIndex = data.m_passRecordContext->m_renderResources->m_pointLightProxyMeshFirstIndex;
			const uint32_t pointLightProxyMeshVertexOffset = data.m_passRecordContext->m_renderResources->m_pointLightProxyMeshVertexOffset;

			for (size_t i = 0; i < data.m_lightDataCount; ++i)
			{
				size_t currentLayerOffset = 0;
				for (size_t j = 0; j < data.m_lightData[i].m_shadowCount; ++j)
				{
					for (size_t k = 0; k < 2; ++k)
					{
						// begin renderpass
						DepthStencilAttachmentDescription depthAttachmentDesc{ registry.getImageView(fomDepthViewHandles[currentLayerOffset]) };
						depthAttachmentDesc.m_loadOp = AttachmentLoadOp::CLEAR;
						depthAttachmentDesc.m_storeOp = AttachmentStoreOp::STORE;
						depthAttachmentDesc.m_stencilLoadOp = AttachmentLoadOp::DONT_CARE;
						depthAttachmentDesc.m_stencilStoreOp = AttachmentStoreOp::DONT_CARE;
						depthAttachmentDesc.m_clearValue = { k == 0 ? 1.0f : 0.0f, 0 };
						depthAttachmentDesc.m_readOnly = false;

						cmdList->beginRenderPass(0, nullptr, &depthAttachmentDesc, { {}, {w, h} });
						{

							cmdList->bindPipeline(pipelines[k]);

							cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_lightProxyIndexBuffer, 0, IndexType::UINT16);

							uint64_t vertexOffset = 0;
							cmdList->bindVertexBuffers(0, 1, &data.m_passRecordContext->m_renderResources->m_lightProxyVertexBuffer, &vertexOffset);

							cmdList->bindDescriptorSets(pipelines[k], 0, 1, &descriptorSet);

							Viewport viewport{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
							Rect scissor{ { 0, 0 }, { w, h } };

							cmdList->setViewport(0, 1, &viewport);
							cmdList->setScissor(0, 1, &scissor);

							uint32_t particleIndex = 0;
							for (size_t k = 0; k < data.m_listCount; ++k)
							{
								for (size_t l = 0; l < data.m_listSizes[k]; ++l)
								{
									struct PushConsts
									{
										uint32_t volumeIndex;
										uint32_t lightIndex;
									};

									PushConsts pushConsts;
									pushConsts.lightIndex = data.m_lightData[i].m_shadowOffset + (uint32_t)j;
									pushConsts.volumeIndex = particleIndex;

									cmdList->pushConstants(pipelines[k], ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

									cmdList->drawIndexed(pointLightProxyMeshIndexCount, 1, pointLightProxyMeshFirstIndex, pointLightProxyMeshVertexOffset, 0);

									++particleIndex;
								}
							}
						}
						cmdList->endRenderPass();
						++currentLayerOffset;
					}
				}
			}
		}, true);
}
