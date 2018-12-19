#include "VKGeometryPipeline.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKShaderModule.h"
#include "Utility/Utility.h"
#include "Graphics/Vulkan/VKUtility.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include "Graphics/DrawItem.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/RenderParams.h"
#include "Graphics/Mesh.h"

VEngine::VKGeometryPipeline::VKGeometryPipeline()
	:m_pipeline(),
	m_pipelineLayout()
{
}

VEngine::VKGeometryPipeline::~VKGeometryPipeline()
{
}

void VEngine::VKGeometryPipeline::init(unsigned int width, unsigned int height, VkRenderPass renderPass, VKRenderResources * renderResources)
{
	VKShaderModule vertShaderModule("Resources/Shaders/geometry_vert.spv");
	VKShaderModule fragShaderModule("Resources/Shaders/geometry_frag.spv");

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule.get();
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule.get();
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attributeDescriptions[] =
	{
		{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) },
		{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal), },
		{ 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord), }
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(sizeof(attributeDescriptions) / sizeof(VkVertexInputAttributeDescription));
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(width);
	viewport.height = static_cast<float>(height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = { width, height };

	VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState defaultBlendAttachment = {};
	defaultBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	defaultBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachments[] = { defaultBlendAttachment , defaultBlendAttachment, defaultBlendAttachment, defaultBlendAttachment };

	VkPipelineColorBlendStateCreateInfo colorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = sizeof(blendAttachments) / sizeof(blendAttachments[0]);
	colorBlending.pAttachments = blendAttachments;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkDescriptorSetLayout layouts[] =
	{
		renderResources->m_perFrameDataDescriptorSetLayout,
		renderResources->m_perDrawDataDescriptorSetLayout,
		renderResources->m_textureDescriptorSetLayout
	};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(sizeof(layouts) / sizeof(layouts[0]));
	pipelineLayoutInfo.pSetLayouts = layouts;

	if (vkCreatePipelineLayout(g_context.m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create pipeline layout!", -1);
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(g_context.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create pipeline!", -1);
	}
}

void VEngine::VKGeometryPipeline::recordCommandBuffer(VkRenderPass renderPass, VKRenderResources *renderResources, const DrawLists &drawLists)
{
	if (!drawLists.m_opaqueItems.empty())
	{
		vkCmdBindPipeline(renderResources->m_geometryFillCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

		vkCmdBindIndexBuffer(renderResources->m_geometryFillCommandBuffer, renderResources->m_indexBuffer.m_buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(renderResources->m_geometryFillCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &renderResources->m_perFrameDataDescriptorSet, 0, nullptr);
		vkCmdBindDescriptorSets(renderResources->m_geometryFillCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 2, 1, &renderResources->m_textureDescriptorSet, 0, nullptr);

		for (size_t i = 0; i < drawLists.m_opaqueItems.size(); ++i)
		{
			const DrawItem &item = drawLists.m_opaqueItems[i];
			vkCmdBindVertexBuffers(renderResources->m_geometryFillCommandBuffer, 0, 1, &renderResources->m_vertexBuffer.m_buffer, &item.m_vertexOffset);

			uint32_t dynamicOffset = static_cast<uint32_t>(renderResources->m_perDrawDataSize * i);
			vkCmdBindDescriptorSets(renderResources->m_geometryFillCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1, &renderResources->m_perDrawDataDescriptorSet, 1, &dynamicOffset);

			vkCmdDrawIndexed(renderResources->m_geometryFillCommandBuffer, item.m_indexCount, 1, item.m_baseIndex, 0, 0);
		}
	}
}