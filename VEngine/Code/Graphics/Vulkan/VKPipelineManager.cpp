#include "VKPipelineManager.h"
#include "VKContext.h"
#include "Utility/Utility.h"
#include <iostream>

VEngine::VKPipelineManager::PipelineLayoutPair VEngine::VKPipelineManager::getPipeline(const VKGraphicsPipelineDescription &pipelineDesc, VkRenderPass renderPass)
{
	auto &pipelinePair = m_graphicsPipelines[pipelineDesc];

	// pipeline does not exist yet -> create it
	if (pipelinePair.m_pipeline == VK_NULL_HANDLE)
	{
		std::cout << "Creating Pipeline!" << std::endl;
		uint32_t stageCount = 0;
		VkShaderModule shaderModules[5] = {};
		VkPipelineShaderStageCreateInfo shaderStages[5] = {};

		auto createShaderStage = [&](const char *filepath, VkShaderStageFlagBits stage)
		{
			std::vector<char> code = Utility::readBinaryFile(filepath);
			VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
			createInfo.codeSize = code.size();
			createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

			if (vkCreateShaderModule(g_context.m_device, &createInfo, nullptr, &shaderModules[stageCount]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create shader module!", -1);
			}

			shaderStages[stageCount] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
			shaderStages[stageCount].stage = stage;
			shaderStages[stageCount].module = shaderModules[stageCount];
			shaderStages[stageCount].pName = "main";

			++stageCount;
		};

		if (pipelineDesc.m_shaderStages.m_vertexShaderPath[0])
		{
			createShaderStage(pipelineDesc.m_shaderStages.m_vertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
		}

		if (pipelineDesc.m_shaderStages.m_tesselationControlShaderPass[0])
		{
			createShaderStage(pipelineDesc.m_shaderStages.m_tesselationControlShaderPass, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		}

		if (pipelineDesc.m_shaderStages.m_tesselationEvaluationShaderPass[0])
		{
			createShaderStage(pipelineDesc.m_shaderStages.m_tesselationEvaluationShaderPass, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
		}

		if (pipelineDesc.m_shaderStages.m_geometryShaderPath[0])
		{
			createShaderStage(pipelineDesc.m_shaderStages.m_geometryShaderPath, VK_SHADER_STAGE_GEOMETRY_BIT);
		}

		if (pipelineDesc.m_shaderStages.m_fragmentShaderPath[0])
		{
			createShaderStage(pipelineDesc.m_shaderStages.m_fragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
		}

		VkPipelineVertexInputStateCreateInfo vertexInputState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		vertexInputState.vertexBindingDescriptionCount = pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount;
		vertexInputState.pVertexBindingDescriptions = pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions;
		vertexInputState.vertexAttributeDescriptionCount = pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount;
		vertexInputState.pVertexAttributeDescriptions = pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		inputAssemblyState.topology = pipelineDesc.m_inputAssemblyState.m_primitiveTopology;
		inputAssemblyState.primitiveRestartEnable = pipelineDesc.m_inputAssemblyState.m_primitiveRestartEnable;

		VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewportState.viewportCount = pipelineDesc.m_viewportState.m_viewportCount;
		viewportState.pViewports = pipelineDesc.m_viewportState.m_viewports;
		viewportState.scissorCount = pipelineDesc.m_viewportState.m_scissorCount;
		viewportState.pScissors = pipelineDesc.m_viewportState.m_scissors;

		VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizationState.depthClampEnable = pipelineDesc.m_rasterizationState.m_depthClampEnable;
		rasterizationState.rasterizerDiscardEnable = pipelineDesc.m_rasterizationState.m_rasterizerDiscardEnable;
		rasterizationState.polygonMode = pipelineDesc.m_rasterizationState.m_polygonMode;
		rasterizationState.cullMode = pipelineDesc.m_rasterizationState.m_cullMode;
		rasterizationState.frontFace = pipelineDesc.m_rasterizationState.m_frontFace;
		rasterizationState.depthBiasEnable = pipelineDesc.m_rasterizationState.m_depthBiasEnable;
		rasterizationState.depthBiasConstantFactor = pipelineDesc.m_rasterizationState.m_depthBiasConstantFactor;
		rasterizationState.depthBiasClamp = pipelineDesc.m_rasterizationState.m_depthBiasClamp;
		rasterizationState.depthBiasSlopeFactor = pipelineDesc.m_rasterizationState.m_depthBiasSlopeFactor;
		rasterizationState.lineWidth = pipelineDesc.m_rasterizationState.m_lineWidth;

		VkPipelineMultisampleStateCreateInfo multisamplingState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisamplingState.rasterizationSamples = pipelineDesc.m_multiSampleState.m_rasterizationSamples;
		multisamplingState.sampleShadingEnable = pipelineDesc.m_multiSampleState.m_sampleShadingEnable;
		multisamplingState.minSampleShading = pipelineDesc.m_multiSampleState.m_minSampleShading;
		multisamplingState.pSampleMask = &pipelineDesc.m_multiSampleState.m_sampleMask;
		multisamplingState.alphaToCoverageEnable = pipelineDesc.m_multiSampleState.m_alphaToCoverageEnable;
		multisamplingState.alphaToOneEnable = pipelineDesc.m_multiSampleState.m_alphaToOneEnable;

		VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depthStencilState.depthTestEnable = pipelineDesc.m_depthStencilState.m_depthTestEnable;
		depthStencilState.depthWriteEnable = pipelineDesc.m_depthStencilState.m_depthWriteEnable;
		depthStencilState.depthCompareOp = pipelineDesc.m_depthStencilState.m_depthCompareOp;
		depthStencilState.depthBoundsTestEnable = pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable;
		depthStencilState.stencilTestEnable = pipelineDesc.m_depthStencilState.m_stencilTestEnable;
		depthStencilState.front = pipelineDesc.m_depthStencilState.m_front;
		depthStencilState.back = pipelineDesc.m_depthStencilState.m_back;
		depthStencilState.minDepthBounds = pipelineDesc.m_depthStencilState.m_minDepthBounds;
		depthStencilState.maxDepthBounds = pipelineDesc.m_depthStencilState.m_maxDepthBounds;

		VkPipelineColorBlendStateCreateInfo blendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		blendState.logicOpEnable = pipelineDesc.m_blendState.m_logicOpEnable;
		blendState.logicOp = pipelineDesc.m_blendState.m_logicOp;
		blendState.attachmentCount = pipelineDesc.m_blendState.m_attachmentCount;
		blendState.pAttachments = pipelineDesc.m_blendState.m_attachments;
		blendState.blendConstants[0] = pipelineDesc.m_blendState.m_blendConstants[0];
		blendState.blendConstants[1] = pipelineDesc.m_blendState.m_blendConstants[1];
		blendState.blendConstants[2] = pipelineDesc.m_blendState.m_blendConstants[2];
		blendState.blendConstants[3] = pipelineDesc.m_blendState.m_blendConstants[3];

		VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicState.dynamicStateCount = pipelineDesc.m_dynamicState.m_dynamicStateCount;
		dynamicState.pDynamicStates = pipelineDesc.m_dynamicState.m_dynamicStates;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pipelineLayoutInfo.setLayoutCount = pipelineDesc.m_layout.m_setLayoutCount;
		pipelineLayoutInfo.pSetLayouts = pipelineDesc.m_layout.m_setLayouts;
		pipelineLayoutInfo.pushConstantRangeCount = pipelineDesc.m_layout.m_pushConstantRangeCount;
		pipelineLayoutInfo.pPushConstantRanges = pipelineDesc.m_layout.m_pushConstantRanges;

		if (vkCreatePipelineLayout(g_context.m_device, &pipelineLayoutInfo, nullptr, &pipelinePair.m_layout) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create pipeline layout!", -1);
		}

		VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineInfo.stageCount = stageCount;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputState;
		pipelineInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizationState;
		pipelineInfo.pMultisampleState = &multisamplingState;
		pipelineInfo.pDepthStencilState = &depthStencilState;
		pipelineInfo.pColorBlendState = &blendState;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = pipelinePair.m_layout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		if (vkCreateGraphicsPipelines(g_context.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelinePair.m_pipeline) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create pipeline!", -1);
		}

		for (uint32_t i = 0; i < stageCount; ++i)
		{
			vkDestroyShaderModule(g_context.m_device, shaderModules[i], nullptr);
		}
	}

	return pipelinePair;
}

VEngine::VKPipelineManager::PipelineLayoutPair VEngine::VKPipelineManager::getPipeline(const VKComputePipelineDescription &pipelineDesc)
{
	auto &pipelinePair = m_computePipelines[pipelineDesc];

	// pipeline does not exist yet -> create it
	if (pipelinePair.m_pipeline == VK_NULL_HANDLE)
	{
		std::cout << "Creating Pipeline!" << std::endl;
		VkShaderModule compShaderModule;

		std::vector<char> code = Utility::readBinaryFile(pipelineDesc.m_computeShaderPath);
		VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		if (vkCreateShaderModule(g_context.m_device, &createInfo, nullptr, &compShaderModule) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create shader module!", -1);
		}

		VkPipelineShaderStageCreateInfo compShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		compShaderStageInfo.module = compShaderModule;
		compShaderStageInfo.pName = "main";

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pipelineLayoutInfo.setLayoutCount = pipelineDesc.m_layout.m_setLayoutCount;
		pipelineLayoutInfo.pSetLayouts = pipelineDesc.m_layout.m_setLayouts;
		pipelineLayoutInfo.pushConstantRangeCount = pipelineDesc.m_layout.m_pushConstantRangeCount;
		pipelineLayoutInfo.pPushConstantRanges = pipelineDesc.m_layout.m_pushConstantRanges;

		if (vkCreatePipelineLayout(g_context.m_device, &pipelineLayoutInfo, nullptr, &pipelinePair.m_layout) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create pipeline layout!", -1);
		}

		VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pipelineInfo.stage = compShaderStageInfo;
		pipelineInfo.layout = pipelinePair.m_layout;

		if (vkCreateComputePipelines(g_context.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelinePair.m_pipeline) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create pipeline!", -1);
		}

		vkDestroyShaderModule(g_context.m_device, compShaderModule, nullptr);
	}

	return pipelinePair;
}
