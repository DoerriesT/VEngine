//#include "VKLuminanceHistogramDebugPass.h"
//#include "Graphics/Vulkan/VKRenderResources.h"
//#include "Graphics/Vulkan/VKContext.h"
//#include "Graphics/Vulkan/VKPipelineCache.h"
//#include "Graphics/Vulkan/VKDescriptorSetCache.h"
//#include <glm/vec4.hpp>
//#include <glm/mat4x4.hpp>
//
//namespace
//{
//	using vec4 = glm::vec4;
//	using mat4 = glm::mat4;
//	using uint = uint32_t;
//#include "../../../../../Application/Resources/Shaders/luminanceHistogramDebug_bindings.h"
//}
//
//void VEngine::VKLuminanceHistogramDebugPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
//{
//	graph.addGraphicsPass("Luminance Histogram Debug Pass", data.m_width, data.m_height,
//		[&](FrameGraph::PassBuilder builder)
//	{
//		builder.readStorageBuffer(data.m_luminanceHistogramBufferHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
//
//		builder.writeColorAttachment(data.m_colorImageHandle, 0);
//	},
//		[=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
//	{
//		// create pipeline description
//		VKGraphicsPipelineDescription pipelineDesc;
//		{
//			strcpy_s(pipelineDesc.m_vertexShaderStage.m_path, "Resources/Shaders/luminanceHistogramDebug_vert.spv");
//			strcpy_s(pipelineDesc.m_fragmentShaderStage.m_path, "Resources/Shaders/luminanceHistogramDebug_frag.spv");
//
//			pipelineDesc.m_inputAssemblyState.m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
//			pipelineDesc.m_inputAssemblyState.m_primitiveRestartEnable = false;
//
//			pipelineDesc.m_viewportState.m_viewportCount = 1;
//			pipelineDesc.m_viewportState.m_viewports[0] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
//			pipelineDesc.m_viewportState.m_scissorCount = 1;
//			pipelineDesc.m_viewportState.m_scissors[0] = { {0, 0}, {1, 1} };
//
//			pipelineDesc.m_rasterizationState.m_depthClampEnable = false;
//			pipelineDesc.m_rasterizationState.m_rasterizerDiscardEnable = false;
//			pipelineDesc.m_rasterizationState.m_polygonMode = VK_POLYGON_MODE_FILL;
//			pipelineDesc.m_rasterizationState.m_cullMode = VK_CULL_MODE_NONE;
//			pipelineDesc.m_rasterizationState.m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
//			pipelineDesc.m_rasterizationState.m_depthBiasEnable = false;
//			pipelineDesc.m_rasterizationState.m_lineWidth = 1.0f;
//
//			pipelineDesc.m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
//			pipelineDesc.m_multiSampleState.m_sampleShadingEnable = false;
//			pipelineDesc.m_multiSampleState.m_sampleMask = 0xFFFFFFFF;
//
//			pipelineDesc.m_depthStencilState.m_depthTestEnable = false;
//			pipelineDesc.m_depthStencilState.m_depthWriteEnable = false;
//			pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
//			pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
//			pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;
//
//			VkPipelineColorBlendAttachmentState defaultBlendAttachment = {};
//			defaultBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
//			defaultBlendAttachment.blendEnable = VK_FALSE;
//
//			pipelineDesc.m_blendState.m_logicOpEnable = false;
//			pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
//			pipelineDesc.m_blendState.m_attachmentCount = 1;
//			pipelineDesc.m_blendState.m_attachments[0] = defaultBlendAttachment;
//
//			pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
//			pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
//			pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;
//
//			pipelineDesc.finalize();
//		}
//
//		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc, *renderPassDescription, renderPass);
//
//		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);
//
//		// update descriptor sets
//		{
//			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);
//
//			// histogram
//			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_luminanceHistogramBufferHandle), LUMINANCE_HISTOGRAM_BINDING);
//			
//			writer.commit();
//		}
//
//		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);
//		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);
//
//		VkViewport viewport;
//		viewport.x = data.m_width * data.m_offsetX;
//		viewport.y = data.m_height * data.m_offsetY;
//		viewport.width = static_cast<float>(data.m_width) * data.m_scaleX;
//		viewport.height = static_cast<float>(data.m_height) * data.m_scaleY;
//		viewport.minDepth = 0.0f;
//		viewport.maxDepth = 1.0f;
//
//		VkRect2D scissor = {};
//		scissor.offset = { static_cast<int32_t>(viewport.x), static_cast<int32_t>(viewport.y) };
//		scissor.extent = { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };
//
//		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
//		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
//
//		PushConsts pushConsts;
//		pushConsts.invPixelCount = 1.0f / (data.m_width * data.m_height);
//
//		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);
//
//		vkCmdDraw(cmdBuf, 6, 1, 0, 0);
//	});
//}
