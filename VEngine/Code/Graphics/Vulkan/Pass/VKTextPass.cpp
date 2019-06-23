#include "VKTextPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKContext.h"
#include <glm/vec4.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>

namespace
{
	using vec2 = glm::vec2;
	using vec4 = glm::vec4;
	using mat4 = glm::mat4;
	using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/text_bindings.h"
}

void VEngine::VKTextPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addGraphicsPass("Text Pass", data.m_width, data.m_height,
		[&](FrameGraph::PassBuilder builder)
	{
		// this pass reads from the global textures array. these descriptors are not managed by framegraph,
		// which is why the reads are not declared here.

		builder.writeColorAttachment(data.m_colorImageHandle, 0);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKGraphicsPipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_vertexShaderStage.m_path, "Resources/Shaders/text_vert.spv");
			strcpy_s(pipelineDesc.m_fragmentShaderStage.m_path, "Resources/Shaders/text_frag.spv");

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

			VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
			colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachment.blendEnable = VK_TRUE;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

			pipelineDesc.m_blendState.m_logicOpEnable = false;
			pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
			pipelineDesc.m_blendState.m_attachmentCount = 1;
			pipelineDesc.m_blendState.m_attachments[0] = colorBlendAttachment;

			pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
			pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc, *renderPassDescription, renderPass);

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 1, &data.m_renderResources->m_textureDescriptorSet, 0, nullptr);

		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(data.m_width);
		viewport.height = static_cast<float>(data.m_height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { static_cast<int32_t>(viewport.x), static_cast<int32_t>(viewport.y) };
		scissor.extent = { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		const size_t symbolHeight = 48;
		const size_t symbolWidth = 24;
		const size_t atlasWidth = 1024;
		const size_t atlasHeight = 512;
		const size_t symbolsPerRow = atlasWidth / symbolWidth;
		const float invAtlasWidth = 1.0f / atlasWidth;
		const float invAtlasHeight = 1.0f / atlasHeight;
		const float invWidth = 1.0f / data.m_width;
		const float invHeight = 1.0f / data.m_height;
		const float texCoordSizeX = symbolWidth * invAtlasWidth;
		const float texCoordSizeY = symbolHeight * invAtlasHeight;
		const float scaleX = 0.4f;
		const float scaleY = 0.4f;
		const float charScaleX = symbolWidth * scaleX * invWidth;
		const float charScaleY = symbolHeight * scaleY * invHeight;

		for (size_t i = 0; i < data.m_stringCount; ++i)
		{
			const String &str = data.m_strings[i];
			const char *c = str.m_chars;

			size_t offsetX = str.m_positionX;
			size_t offsetY = str.m_positionY;

			while (*c)
			{
				if (*c == '\n')
				{
					offsetY += static_cast<size_t>(symbolHeight * scaleY);
					offsetX = str.m_positionX;
				}
				else
				{
					// we can skip rendering spaces
					if (*c != ' ')
					{
						PushConsts pushConsts;

						pushConsts.scaleBias[0] = charScaleX;
						pushConsts.scaleBias[1] = charScaleY;
						pushConsts.scaleBias[2] = offsetX * invWidth;
						pushConsts.scaleBias[3] = offsetY * invHeight;

						pushConsts.color[0] = 1.0f;
						pushConsts.color[1] = 1.0f;
						pushConsts.color[2] = 0.0f;
						pushConsts.color[3] = 1.0f;

						size_t atlasIndex = *c - 32; // atlas starts at symbol 32
						pushConsts.texCoordOffset[0] = ((atlasIndex % symbolsPerRow) * symbolWidth) * invAtlasWidth;
						pushConsts.texCoordOffset[1] = ((atlasIndex / symbolsPerRow) * symbolHeight) * invAtlasHeight;
						pushConsts.texCoordSize[0] = texCoordSizeX;
						pushConsts.texCoordSize[1] = texCoordSizeY;

						pushConsts.textureIndex = data.m_atlasTextureIndex;

						vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);
						vkCmdDraw(cmdBuf, 6, 1, 0, 0);
					}

					offsetX += static_cast<size_t>(symbolWidth * scaleX);

					if (offsetX + static_cast<size_t>(symbolWidth * scaleX) > data.m_width)
					{
						offsetY += static_cast<size_t>(symbolHeight * scaleY);
						offsetX = str.m_positionX;
					}

				}

				++c;
			}
		}
	});
}
