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
		{ResourceViewHandle(data.m_indicesBufferHandle), ResourceState::READ_INDEX_BUFFER},
		{ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::READ_INDIRECT_BUFFER},
		{ResourceViewHandle(data.m_voxelSceneImageHandle), ResourceState::READ_WRITE_STORAGE_IMAGE_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_voxelSceneOpacityImageHandle), ResourceState::READ_WRITE_STORAGE_IMAGE_FRAGMENT_SHADER},
	};

	graph.addPass("Voxelization", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			const uint32_t superSamplingFactor = 4;
			const uint32_t voxelGridResolution = glm::max(glm::max(RendererConsts::VOXEL_SCENE_WIDTH, RendererConsts::VOXEL_SCENE_HEIGHT), RendererConsts::VOXEL_SCENE_DEPTH) * superSamplingFactor;
			
			// begin renderpass
			VkRenderPass renderPass;
			RenderPassCompatibilityDescription renderPassCompatDesc;
			{
				RenderPassDescription renderPassDesc{};
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
			VKGraphicsPipelineDescription pipelineDesc;
			{
				strcpy_s(pipelineDesc.m_vertexShaderStage.m_path, "Resources/Shaders/voxelization_vert.spv");
				strcpy_s(pipelineDesc.m_geometryShaderStage.m_path, "Resources/Shaders/voxelization_geom.spv");
				strcpy_s(pipelineDesc.m_fragmentShaderStage.m_path, "Resources/Shaders/voxelization_frag.spv");
				pipelineDesc.m_fragmentShaderStage.m_specializationInfo.addEntry(VOXEL_GRID_WIDTH_CONST_ID, RendererConsts::VOXEL_SCENE_WIDTH);
				pipelineDesc.m_fragmentShaderStage.m_specializationInfo.addEntry(VOXEL_GRID_HEIGHT_CONST_ID, RendererConsts::VOXEL_SCENE_HEIGHT);
				pipelineDesc.m_fragmentShaderStage.m_specializationInfo.addEntry(VOXEL_GRID_DEPTH_CONST_ID, RendererConsts::VOXEL_SCENE_DEPTH);

				pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 0;
				pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 0;

				pipelineDesc.m_inputAssemblyState.m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				pipelineDesc.m_inputAssemblyState.m_primitiveRestartEnable = false;

				pipelineDesc.m_viewportState.m_viewportCount = 1;
				pipelineDesc.m_viewportState.m_viewports[0] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
				pipelineDesc.m_viewportState.m_scissorCount = 1;
				pipelineDesc.m_viewportState.m_scissors[0] = { {0, 0}, {1, 1} };

				pipelineDesc.m_rasterizationState.m_depthClampEnable = false;
				pipelineDesc.m_rasterizationState.m_rasterizerDiscardEnable = false;
				pipelineDesc.m_rasterizationState.m_polygonMode = VK_POLYGON_MODE_FILL;
				pipelineDesc.m_rasterizationState.m_cullMode = VK_CULL_MODE_NONE;
				pipelineDesc.m_rasterizationState.m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
				pipelineDesc.m_rasterizationState.m_depthBiasEnable = false;
				pipelineDesc.m_rasterizationState.m_lineWidth = 1.0f;

				pipelineDesc.m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
				pipelineDesc.m_multiSampleState.m_sampleShadingEnable = false;
				pipelineDesc.m_multiSampleState.m_sampleMask = 0xFFFFFFFF;

				pipelineDesc.m_depthStencilState.m_depthTestEnable = false;
				pipelineDesc.m_depthStencilState.m_depthWriteEnable = false;
				pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
				pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
				pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;

				pipelineDesc.m_blendState.m_logicOpEnable = false;
				pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
				pipelineDesc.m_blendState.m_attachmentCount = 0;

				pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
				pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
				pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

				pipelineDesc.finalize();
			}

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
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_subMeshInfoBufferInfo, SUB_MESH_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_voxelSceneOpacityImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_FRAGMENT_SHADER), OPACITY_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_voxelSceneImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_FRAGMENT_SHADER), VOXEL_IMAGE_BINDING);

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

			vkCmdBindIndexBuffer(cmdBuf, registry.getBuffer(data.m_indicesBufferHandle), 0, VK_INDEX_TYPE_UINT32);

			const glm::vec3 camPos = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
			float curVoxelScale = 1.0f / RendererConsts::VOXEL_SCENE_BASE_SIZE;

			for (uint32_t i = 0; i < RendererConsts::VOXEL_SCENE_CASCADES; ++i)
			{
				PushConsts pushConsts;
				pushConsts.superSamplingFactor = superSamplingFactor;
				pushConsts.gridOffset = round(camPos * curVoxelScale) - glm::floor(glm::vec3(RendererConsts::VOXEL_SCENE_WIDTH, RendererConsts::VOXEL_SCENE_HEIGHT, RendererConsts::VOXEL_SCENE_DEPTH) / 2.0f);
				pushConsts.voxelScale = curVoxelScale;
				pushConsts.invGridResolution = 1.0f / voxelGridResolution;
				pushConsts.cascade = i;

				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

				vkCmdDrawIndexedIndirect(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), 0, 1, sizeof(VkDrawIndexedIndirectCommand));

				curVoxelScale *= 0.5f;
			}

			vkCmdEndRenderPass(cmdBuf);
		});
}
