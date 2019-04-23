#include "VKMemoryHeapDebugPass.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/memoryHeapDebug_bindings.h"

void VEngine::VKMemoryHeapDebugPass::addToGraph(FrameGraph::Graph & graph, const Data & data)
{
	graph.addGraphicsPass("Memory Heap Debug Pass", data.m_width, data.m_height,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.writeColorAttachment(data.m_colorImageHandle, 0);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKGraphicsPipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/memoryHeapDebug_vert.spv");
			strcpy_s(pipelineDesc.m_shaderStages.m_fragmentShaderPath, "Resources/Shaders/memoryHeapDebug_frag.spv");

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
			pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
			pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;

			VkPipelineColorBlendAttachmentState defaultBlendAttachment = {};
			defaultBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			defaultBlendAttachment.blendEnable = VK_FALSE;

			pipelineDesc.m_blendState.m_logicOpEnable = false;
			pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
			pipelineDesc.m_blendState.m_attachmentCount = 1;
			pipelineDesc.m_blendState.m_attachments[0] = defaultBlendAttachment;

			pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
			pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc, *renderPassDescription, renderPass);

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);


		VkViewport viewport;
		viewport.x = data.m_width * data.m_offsetX;
		viewport.y = data.m_height * data.m_offsetY;
		viewport.width = static_cast<float>(data.m_width) * data.m_scaleX;
		viewport.height = static_cast<float>(data.m_height) * data.m_scaleY;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { static_cast<int32_t>(viewport.x), static_cast<int32_t>(viewport.y) };
		scissor.extent = { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		auto debugInfo = g_context.m_allocator.getDebugInfo();
		const float maxBlockSize = static_cast<float>(g_context.m_allocator.getMaximumBlockSize());
		const float maxBlocks = 4.0f;
		const float blockSpacingMult = 1.05f;

		size_t memoryTypeBlockCount[VK_MAX_MEMORY_TYPES] = {};

		for (size_t i = 0; i < debugInfo.size(); ++i)
		{
			const auto &blockInfo = debugInfo[i];

			for (size_t j = 0; j < blockInfo.m_spans.size(); ++j)
			{
				const auto &span = blockInfo.m_spans[j];

				PushConsts pushConsts;

				switch (span.m_state)
				{
				case TLSFSpanDebugInfo::State::FREE:
					pushConsts.color[0] = 0.0f;
					pushConsts.color[1] = 1.0f;
					pushConsts.color[2] = 0.0f;
					break;
				case TLSFSpanDebugInfo::State::USED:
					pushConsts.color[0] = 0.0f;
					pushConsts.color[1] = 0.0f;
					pushConsts.color[2] = 1.0f;
					break;
				case TLSFSpanDebugInfo::State::WASTED:
					pushConsts.color[0] = 1.0f;
					pushConsts.color[1] = 0.0f;
					pushConsts.color[2] = 0.0f;
					break;
				default:
					assert(false);
					break;
				}

				pushConsts.scaleBias[0] = span.m_size / (maxBlockSize * maxBlocks);
				pushConsts.scaleBias[1] = 1.0f / float(VK_MAX_MEMORY_TYPES);
				pushConsts.scaleBias[2] = (memoryTypeBlockCount[blockInfo.m_memoryType] * blockSpacingMult + span.m_offset / maxBlockSize) / maxBlocks;
				pushConsts.scaleBias[3] = blockInfo.m_memoryType / float(VK_MAX_MEMORY_TYPES) * blockSpacingMult;

				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);
				vkCmdDraw(cmdBuf, 6, 1, 0, 0);
			}

			++memoryTypeBlockCount[blockInfo.m_memoryType];
		}
	});
}
