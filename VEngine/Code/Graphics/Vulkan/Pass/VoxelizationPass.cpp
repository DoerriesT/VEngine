#include "VoxelizationPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Mesh.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/Vulkan/RenderPassCache.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/DeferredObjectDeleter.h"
#include "Utility/Utility.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/voxelization_bindings.h"
}

void VEngine::VoxelizationPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		//{ResourceViewHandle(data.m_indicesBufferHandle), ResourceState::READ_INDEX_BUFFER},
		//{ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::READ_INDIRECT_BUFFER},
		{ResourceViewHandle(data.m_voxelSceneImageHandle), ResourceState::READ_WRITE_STORAGE_IMAGE_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_voxelSceneOpacityImageHandle), ResourceState::READ_WRITE_STORAGE_IMAGE_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_shadowImageViewHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
		{ ResourceViewHandle(data.m_irradianceVolumeImageHandles[0]), ResourceState::READ_TEXTURE_FRAGMENT_SHADER },
		{ ResourceViewHandle(data.m_irradianceVolumeImageHandles[1]), ResourceState::READ_TEXTURE_FRAGMENT_SHADER },
		{ ResourceViewHandle(data.m_irradianceVolumeImageHandles[2]), ResourceState::READ_TEXTURE_FRAGMENT_SHADER },
	};

	graph.addPass("Voxelization", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			const uint32_t superSamplingFactor = 4;
			const uint32_t voxelGridResolution = glm::max(glm::max(RendererConsts::VOXEL_SCENE_WIDTH, RendererConsts::VOXEL_SCENE_HEIGHT), RendererConsts::VOXEL_SCENE_DEPTH) * superSamplingFactor;
			
			// begin renderpass
			VkRenderPass renderPass;
			RenderPassCompatDesc renderPassCompatDesc;
			{
				RenderPassDesc renderPassDesc{};
				renderPassDesc.m_attachmentCount = 0;
				renderPassDesc.m_subpassCount = 1;
				renderPassDesc.m_subpasses[0] = { 0, 0, false, 0 };

				data.m_passRecordContext->m_renderPassCache->getRenderPass(renderPassDesc, renderPassCompatDesc, renderPass);

				VkFramebuffer framebuffer;

				VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
				framebufferCreateInfo.renderPass = renderPass;
				framebufferCreateInfo.attachmentCount = renderPassDesc.m_attachmentCount;
				framebufferCreateInfo.pAttachments = nullptr;
				framebufferCreateInfo.width = voxelGridResolution;
				framebufferCreateInfo.height = voxelGridResolution;
				framebufferCreateInfo.layers = 1;

				if (vkCreateFramebuffer(g_context.m_device, &framebufferCreateInfo, nullptr, &framebuffer) != VK_SUCCESS)
				{
					Utility::fatalExit("Failed to create framebuffer!", EXIT_FAILURE);
				}

				data.m_passRecordContext->m_deferredObjectDeleter->add(framebuffer);

				VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
				renderPassBeginInfo.renderPass = renderPass;
				renderPassBeginInfo.framebuffer = framebuffer;
				renderPassBeginInfo.renderArea = { {}, {framebufferCreateInfo.width, framebufferCreateInfo.height} };
				renderPassBeginInfo.clearValueCount = 0;
				renderPassBeginInfo.pClearValues = nullptr;

				vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			}

			// create pipeline description
			SpecEntry fragmentShaderSpecEntries[]
			{
				SpecEntry(VOXEL_GRID_WIDTH_CONST_ID, RendererConsts::VOXEL_SCENE_WIDTH),
				SpecEntry(VOXEL_GRID_HEIGHT_CONST_ID, RendererConsts::VOXEL_SCENE_HEIGHT),
				SpecEntry(VOXEL_GRID_DEPTH_CONST_ID, RendererConsts::VOXEL_SCENE_DEPTH),
				SpecEntry(DIRECTIONAL_LIGHT_COUNT_CONST_ID, data.m_passRecordContext->m_commonRenderData->m_directionalLightCount),
			};

			VkDynamicState dynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

			GraphicsPipelineDesc pipelineDesc;
			pipelineDesc.setVertexShader("Resources/Shaders/voxelization_vert.spv");
			pipelineDesc.setGeometryShader("Resources/Shaders/voxelization_geom.spv");
			pipelineDesc.setFragmentShader("Resources/Shaders/voxelization_frag.spv", sizeof(fragmentShaderSpecEntries) / sizeof(fragmentShaderSpecEntries[0]), fragmentShaderSpecEntries);
			pipelineDesc.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc, renderPassCompatDesc, 0, renderPass);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
				VkSampler linearSamplerRepeat = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX];
				VkBuffer vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer.getBuffer();

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition) }, VERTEX_POSITIONS_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { vertexBuffer, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * sizeof(VertexNormal) }, VERTEX_NORMALS_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord) }, VERTEX_TEXCOORDS_BINDING);
				//writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);
				//writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_subMeshInfoBufferInfo, SUB_MESH_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_voxelSceneOpacityImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_FRAGMENT_SHADER), OPACITY_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_voxelSceneImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_FRAGMENT_SHADER), VOXEL_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_shadowImageViewHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER), SHADOW_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLER, { data.m_passRecordContext->m_renderResources->m_shadowSampler }, SHADOW_SAMPLER_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_lightDataBufferInfo, DIRECTIONAL_LIGHT_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_shadowMatricesBufferInfo, SHADOW_MATRICES_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeImageHandles[0], ResourceState::READ_TEXTURE_FRAGMENT_SHADER, linearSamplerRepeat), IRRADIANCE_VOLUME_IMAGE_BINDING, 0);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeImageHandles[1], ResourceState::READ_TEXTURE_FRAGMENT_SHADER, linearSamplerRepeat), IRRADIANCE_VOLUME_IMAGE_BINDING, 1);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeImageHandles[2], ResourceState::READ_TEXTURE_FRAGMENT_SHADER, linearSamplerRepeat), IRRADIANCE_VOLUME_IMAGE_BINDING, 2);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 2, descriptorSets, 0, nullptr);

			VkViewport viewport;
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(voxelGridResolution);
			viewport.height = static_cast<float>(voxelGridResolution);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor;
			scissor.offset = { 0, 0 };
			scissor.extent = { voxelGridResolution, voxelGridResolution };

			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
			vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

			//vkCmdBindIndexBuffer(cmdBuf, registry.getBuffer(data.m_indicesBufferHandle), 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindIndexBuffer(cmdBuf, data.m_passRecordContext->m_renderResources->m_indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT16);

			const glm::vec3 camPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
			float curVoxelScale = 1.0f / RendererConsts::VOXEL_SCENE_BASE_SIZE;

			for (uint32_t i = 0; i < RendererConsts::VOXEL_SCENE_CASCADES; ++i)
			{
				for (uint32_t j = 0; j < data.m_instanceDataCount; ++j)
				{
					const auto &instanceData = data.m_instanceData[j];
					const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

					PushConsts pushConsts;
					pushConsts.superSamplingFactor = superSamplingFactor;
					pushConsts.gridOffset = round(camPos * curVoxelScale) - glm::floor(glm::vec3(RendererConsts::VOXEL_SCENE_WIDTH, RendererConsts::VOXEL_SCENE_HEIGHT, RendererConsts::VOXEL_SCENE_DEPTH) / 2.0f);
					pushConsts.voxelScale = curVoxelScale;
					pushConsts.invGridResolution = 1.0f / voxelGridResolution;
					pushConsts.cascade = i;
					pushConsts.transformIndex = instanceData.m_transformIndex;
					pushConsts.materialIndex = instanceData.m_materialIndex;
					pushConsts.invViewMatrix = data.m_passRecordContext->m_commonRenderData->m_invViewMatrix;

					vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

					vkCmdDrawIndexed(cmdBuf, subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, subMeshInfo.m_vertexOffset, 0);
					//vkCmdDrawIndexedIndirect(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), 0, 1, sizeof(VkDrawIndexedIndirectCommand));
				}
				
				curVoxelScale *= 0.5f;
			}

			vkCmdEndRenderPass(cmdBuf);
		});
}
