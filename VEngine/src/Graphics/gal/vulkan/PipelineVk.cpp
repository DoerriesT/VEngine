#include "PipelineVk.h"
#include <cstring>
#include "GraphicsDeviceVk.h"
#include "UtilityVk.h"
#include "Utility/Utility.h"
#include <spirv-cross/spirv_cross.hpp>
#include "RenderPassDescriptionVk.h"
#include "DescriptorSetVk.h"

namespace
{
	struct ReflectionInfo
	{
		enum
		{
			MAX_BINDING_COUNT = 32,
		};

		struct SetLayout
		{
			uint32_t m_uniformBufferMask;
			uint32_t m_uniformTexelBufferMask;
			uint32_t m_storageBufferMask;
			uint32_t m_storageTexelBufferMask;
			uint32_t m_subpassInputMask;
			uint32_t m_storageImageMask;
			uint32_t m_sampledImageMask;
			uint32_t m_separateImageMask;
			uint32_t m_separateSamplerMask;
			uint32_t m_arraySizes[MAX_BINDING_COUNT];
			VkShaderStageFlags m_stageFlags[MAX_BINDING_COUNT];
		};

		struct PushConstantInfo
		{
			uint32_t m_size;
			VkShaderStageFlags m_stageFlags;
		};

		SetLayout m_setLayouts[VEngine::gal::DescriptorSetLayoutsVk::MAX_SET_COUNT];
		PushConstantInfo m_pushConstants;
		uint32_t m_setMask;
	};
}

static void createShaderStage(VkDevice device, const VEngine::gal::ShaderStageCreateInfo &stageDesc, VkShaderStageFlagBits stageFlag, VkShaderModule &shaderModule, VkPipelineShaderStageCreateInfo &stageCreateInfo, ReflectionInfo &reflectionInfo, VkSpecializationInfo &specInfo);
static void createPipelineLayout(VkDevice device, const ReflectionInfo &reflectionInfo, VkPipelineLayout &pipelineLayout, VEngine::gal::DescriptorSetLayoutsVk &descriptorSetLayouts);

VEngine::gal::GraphicsPipelineVk::GraphicsPipelineVk(GraphicsDeviceVk &device, const GraphicsPipelineCreateInfo &createInfo)
	:m_pipeline(VK_NULL_HANDLE),
	m_pipelineLayout(VK_NULL_HANDLE),
	m_device(&device),
	m_descriptorSetLayouts()
{
	VkDevice deviceVk = m_device->getDevice();
	uint32_t stageCount = 0;
	VkShaderModule shaderModules[5] = {};
	VkPipelineShaderStageCreateInfo shaderStages[5] = {};
	VkSpecializationInfo specInfo[5];
	ReflectionInfo reflectionInfo{};
	VkRenderPass renderPass;

	// create shaders and perform reflection
	{
		if (createInfo.m_vertexShader.m_path[0])
		{
			createShaderStage(deviceVk, createInfo.m_vertexShader, VK_SHADER_STAGE_VERTEX_BIT, shaderModules[stageCount], shaderStages[stageCount], reflectionInfo, specInfo[stageCount]);
			++stageCount;
		}

		if (createInfo.m_tessControlShader.m_path[0])
		{
			createShaderStage(deviceVk, createInfo.m_tessControlShader, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, shaderModules[stageCount], shaderStages[stageCount], reflectionInfo, specInfo[stageCount]);
			++stageCount;
		}

		if (createInfo.m_tessEvalShader.m_path[0])
		{
			createShaderStage(deviceVk, createInfo.m_tessEvalShader, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, shaderModules[stageCount], shaderStages[stageCount], reflectionInfo, specInfo[stageCount]);
			++stageCount;
		}

		if (createInfo.m_geometryShader.m_path[0])
		{
			createShaderStage(deviceVk, createInfo.m_geometryShader, VK_SHADER_STAGE_GEOMETRY_BIT, shaderModules[stageCount], shaderStages[stageCount], reflectionInfo, specInfo[stageCount]);
			++stageCount;
		}

		if (createInfo.m_fragmentShader.m_path[0])
		{
			createShaderStage(deviceVk, createInfo.m_fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, shaderModules[stageCount], shaderStages[stageCount], reflectionInfo, specInfo[stageCount]);
			++stageCount;
		}
	}

	// get compatible renderpass
	{
		RenderPassDescriptionVk::ColorAttachmentDescriptionVk colorAttachments[8];
		RenderPassDescriptionVk::DepthStencilAttachmentDescriptionVk depthStencilAttachment;

		uint32_t attachmentCount = 0;

		VkSampleCountFlagBits samples = static_cast<VkSampleCountFlagBits>(createInfo.m_multiSampleState.m_rasterizationSamples);

		// fill out color attachment info structs
		for (uint32_t i = 0; i < createInfo.m_attachmentFormats.m_colorAttachmentCount; ++i)
		{
			auto &attachmentDesc = colorAttachments[i];
			attachmentDesc = {};
			attachmentDesc.m_format = UtilityVk::translate(createInfo.m_attachmentFormats.m_colorAttachmentFormats[i]);
			attachmentDesc.m_samples = samples;

			// these values dont actually matter, because they dont affect renderpass compatibility and are provided later by the actual renderpass used when drawing with this pipeline.
			// still, we should try to guess the most likely values in order to reduce the need for creating additional renderpasses
			attachmentDesc.m_loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachmentDesc.m_storeOp = VK_ATTACHMENT_STORE_OP_STORE;

			++attachmentCount;
		}

		// fill out depth/stencil attachment info struct
		if (createInfo.m_depthStencilState.m_depthTestEnable)
		{
			auto &attachmentDesc = depthStencilAttachment;
			attachmentDesc = {};
			attachmentDesc.m_format = UtilityVk::translate(createInfo.m_attachmentFormats.m_depthStencilFormat);
			attachmentDesc.m_samples = samples;

			// these values dont actually matter, because they dont affect renderpass compatibility and are provided later by the actual renderpass used when drawing with this pipeline.
			// still, we should try to guess the most likely values in order to reduce the need for creating additional renderpasses
			attachmentDesc.m_loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachmentDesc.m_storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDesc.m_stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDesc.m_stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDesc.m_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			++attachmentCount;
		}

		// get renderpass
		{
			RenderPassDescriptionVk renderPassDescription;
			renderPassDescription.setColorAttachments(createInfo.m_attachmentFormats.m_colorAttachmentCount, colorAttachments);
			if (createInfo.m_depthStencilState.m_depthTestEnable)
			{
				renderPassDescription.setDepthStencilAttachment(depthStencilAttachment);
			}
			renderPassDescription.finalize();

			renderPass = m_device->getRenderPass(renderPassDescription);
		}
	}

	// create descriptor set layouts and pipeline layout
	createPipelineLayout(deviceVk, reflectionInfo, m_pipelineLayout, m_descriptorSetLayouts);

	VkPipelineVertexInputStateCreateInfo vertexInputState{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkVertexInputBindingDescription vertexBindingDescriptions[VertexInputState::MAX_VERTEX_BINDING_DESCRIPTIONS];
	VkVertexInputAttributeDescription vertexAttributeDescriptions[VertexInputState::MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS];
	{
		vertexInputState.vertexBindingDescriptionCount = createInfo.m_vertexInputState.m_vertexBindingDescriptionCount;
		vertexInputState.pVertexBindingDescriptions = vertexBindingDescriptions;
		vertexInputState.vertexAttributeDescriptionCount = createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount;
		vertexInputState.pVertexAttributeDescriptions = vertexAttributeDescriptions;

		for (size_t i = 0; i < createInfo.m_vertexInputState.m_vertexBindingDescriptionCount; ++i)
		{
			auto &binding = vertexBindingDescriptions[i];
			binding.binding = createInfo.m_vertexInputState.m_vertexBindingDescriptions[i].m_binding;
			binding.stride = createInfo.m_vertexInputState.m_vertexBindingDescriptions[i].m_stride;
			binding.inputRate = UtilityVk::translate(createInfo.m_vertexInputState.m_vertexBindingDescriptions[i].m_inputRate);
		}

		for (size_t i = 0; i < createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount; ++i)
		{
			auto &attribute = vertexAttributeDescriptions[i];
			attribute.location = createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i].m_location;
			attribute.binding = createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i].m_binding;
			attribute.format = UtilityVk::translate(createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i].m_format);
			attribute.offset = createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i].m_offset;
		}
	}
	

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	{
		inputAssemblyState.topology = UtilityVk::translate(createInfo.m_inputAssemblyState.m_primitiveTopology);
		inputAssemblyState.primitiveRestartEnable = createInfo.m_inputAssemblyState.m_primitiveRestartEnable;
	}

	VkPipelineTessellationStateCreateInfo tesselatationState = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
	{
		tesselatationState.patchControlPoints = createInfo.m_tesselationState.m_patchControlPoints;
	}

	VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	{
		viewportState.viewportCount = createInfo.m_viewportState.m_viewportCount;
		viewportState.pViewports = reinterpret_cast<const VkViewport *>(createInfo.m_viewportState.m_viewports);
		viewportState.scissorCount = createInfo.m_viewportState.m_viewportCount;
		viewportState.pScissors = reinterpret_cast<const VkRect2D *>(createInfo.m_viewportState.m_scissors);
	}


	VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	{
		rasterizationState.depthClampEnable = createInfo.m_rasterizationState.m_depthClampEnable;
		rasterizationState.rasterizerDiscardEnable = createInfo.m_rasterizationState.m_rasterizerDiscardEnable;
		rasterizationState.polygonMode = UtilityVk::translate(createInfo.m_rasterizationState.m_polygonMode);
		rasterizationState.cullMode = UtilityVk::translateCullModeFlags(createInfo.m_rasterizationState.m_cullMode);
		rasterizationState.frontFace = UtilityVk::translate(createInfo.m_rasterizationState.m_frontFace);
		rasterizationState.depthBiasEnable = createInfo.m_rasterizationState.m_depthBiasEnable;
		rasterizationState.depthBiasConstantFactor = createInfo.m_rasterizationState.m_depthBiasConstantFactor;
		rasterizationState.depthBiasClamp = createInfo.m_rasterizationState.m_depthBiasClamp;
		rasterizationState.depthBiasSlopeFactor = createInfo.m_rasterizationState.m_depthBiasSlopeFactor;
		rasterizationState.lineWidth = createInfo.m_rasterizationState.m_lineWidth;
	}
	

	VkPipelineMultisampleStateCreateInfo multisamplingState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	{
		multisamplingState.rasterizationSamples = static_cast<VkSampleCountFlagBits>(createInfo.m_multiSampleState.m_rasterizationSamples);
		multisamplingState.sampleShadingEnable = createInfo.m_multiSampleState.m_sampleShadingEnable;
		multisamplingState.minSampleShading = createInfo.m_multiSampleState.m_minSampleShading;
		multisamplingState.pSampleMask = &createInfo.m_multiSampleState.m_sampleMask;
		multisamplingState.alphaToCoverageEnable = createInfo.m_multiSampleState.m_alphaToCoverageEnable;
		multisamplingState.alphaToOneEnable = createInfo.m_multiSampleState.m_alphaToOneEnable;
	}
	

	VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	{
		auto translateStencilOpState = [](const StencilOpState &state)
		{
			VkStencilOpState result{};
			result.failOp = UtilityVk::translate(state.m_failOp);
			result.passOp = UtilityVk::translate(state.m_passOp);
			result.depthFailOp = UtilityVk::translate(state.m_depthFailOp);
			result.compareOp = UtilityVk::translate(state.m_compareOp);
			result.compareMask = state.m_compareMask;
			result.writeMask = state.m_writeMask;
			result.reference = state.m_reference;
			return result;
		};

		depthStencilState.depthTestEnable = createInfo.m_depthStencilState.m_depthTestEnable;
		depthStencilState.depthWriteEnable = createInfo.m_depthStencilState.m_depthWriteEnable;
		depthStencilState.depthCompareOp = UtilityVk::translate(createInfo.m_depthStencilState.m_depthCompareOp);
		depthStencilState.depthBoundsTestEnable = createInfo.m_depthStencilState.m_depthBoundsTestEnable;
		depthStencilState.stencilTestEnable = createInfo.m_depthStencilState.m_stencilTestEnable;
		depthStencilState.front = translateStencilOpState(createInfo.m_depthStencilState.m_front);
		depthStencilState.back = translateStencilOpState(createInfo.m_depthStencilState.m_back);
		depthStencilState.minDepthBounds = createInfo.m_depthStencilState.m_minDepthBounds;
		depthStencilState.maxDepthBounds = createInfo.m_depthStencilState.m_maxDepthBounds;
	}
	

	VkPipelineColorBlendStateCreateInfo blendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[8];
	{
		blendState.logicOpEnable = createInfo.m_blendState.m_logicOpEnable;
		blendState.logicOp = UtilityVk::translate(createInfo.m_blendState.m_logicOp);
		blendState.attachmentCount = createInfo.m_blendState.m_attachmentCount;
		blendState.pAttachments = colorBlendAttachmentStates;
		blendState.blendConstants[0] = createInfo.m_blendState.m_blendConstants[0];
		blendState.blendConstants[1] = createInfo.m_blendState.m_blendConstants[1];
		blendState.blendConstants[2] = createInfo.m_blendState.m_blendConstants[2];
		blendState.blendConstants[3] = createInfo.m_blendState.m_blendConstants[3];

		for (size_t i = 0; i < createInfo.m_blendState.m_attachmentCount; ++i)
		{
			auto &state = colorBlendAttachmentStates[i];
			state.blendEnable = createInfo.m_blendState.m_attachments[i].m_blendEnable;
			state.srcColorBlendFactor = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_srcColorBlendFactor);
			state.dstColorBlendFactor = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_dstColorBlendFactor);
			state.colorBlendOp = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_colorBlendOp);
			state.srcAlphaBlendFactor = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_srcAlphaBlendFactor);
			state.dstAlphaBlendFactor = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_dstAlphaBlendFactor);
			state.alphaBlendOp = UtilityVk::translate(createInfo.m_blendState.m_attachments[i].m_alphaBlendOp);
			state.colorWriteMask = UtilityVk::translateColorComponentFlags(createInfo.m_blendState.m_attachments[i].m_colorWriteMask);
		}
	}
	
	VkDynamicState dynamicStatesArray[9];
	VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.pDynamicStates = dynamicStatesArray;
	UtilityVk::translateDynamicStateFlags(createInfo.m_dynamicStateFlags, dynamicState.dynamicStateCount, dynamicStatesArray);

	
	VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.flags = 0;
	pipelineInfo.stageCount = stageCount;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputState;
	pipelineInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineInfo.pTessellationState = &tesselatationState;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizationState;
	pipelineInfo.pMultisampleState = &multisamplingState;
	pipelineInfo.pDepthStencilState = &depthStencilState;
	pipelineInfo.pColorBlendState = &blendState;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	UtilityVk::checkResult(vkCreateGraphicsPipelines(deviceVk, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "Failed to create pipeline!");

	for (uint32_t i = 0; i < stageCount; ++i)
	{
		vkDestroyShaderModule(deviceVk, shaderModules[i], nullptr);
	}
}

VEngine::gal::GraphicsPipelineVk::~GraphicsPipelineVk()
{
	VkDevice deviceVk = m_device->getDevice();
	vkDestroyPipeline(deviceVk, m_pipeline, nullptr);
	vkDestroyPipelineLayout(deviceVk, m_pipelineLayout, nullptr);

	for (uint32_t i = 0; i < m_descriptorSetLayouts.m_layoutCount; ++i)
	{
		// call destructor and free backing memory
		m_descriptorSetLayouts.m_descriptorSetLayouts[i]->~DescriptorSetLayoutVk();
		m_descriptorSetLayouts.m_descriptorSetLayoutMemoryPool.free(reinterpret_cast<ByteArray<sizeof(DescriptorSetLayoutVk)> *>(m_descriptorSetLayouts.m_descriptorSetLayouts[i]));
	}
}

void *VEngine::gal::GraphicsPipelineVk::getNativeHandle() const
{
	return (void *)m_pipeline;
}

uint32_t VEngine::gal::GraphicsPipelineVk::getDescriptorSetLayoutCount() const
{
	return m_descriptorSetLayouts.m_layoutCount;
}

const VEngine::gal::DescriptorSetLayout *VEngine::gal::GraphicsPipelineVk::getDescriptorSetLayout(uint32_t index) const
{
	assert(index < m_descriptorSetLayouts.m_layoutCount);
	return m_descriptorSetLayouts.m_descriptorSetLayouts[index];
}

VkPipelineLayout VEngine::gal::GraphicsPipelineVk::getLayout() const
{
	return m_pipelineLayout;
}

VEngine::gal::ComputePipelineVk::ComputePipelineVk(GraphicsDeviceVk &device, const ComputePipelineCreateInfo &createInfo)
	:m_pipeline(VK_NULL_HANDLE),
	m_pipelineLayout(VK_NULL_HANDLE),
	m_device(&device),
	m_descriptorSetLayouts()
{
	VkDevice deviceVk = m_device->getDevice();
	VkShaderModule compShaderModule;
	VkPipelineShaderStageCreateInfo compShaderStageInfo;
	ReflectionInfo reflectionInfo{};
	VkSpecializationInfo specInfo;

	// create shader and perform reflection
	createShaderStage(deviceVk, createInfo.m_computeShader, VK_SHADER_STAGE_COMPUTE_BIT, compShaderModule, compShaderStageInfo, reflectionInfo, specInfo);

	// create descriptor set layouts and pipeline layout
	createPipelineLayout(deviceVk, reflectionInfo, m_pipelineLayout, m_descriptorSetLayouts);

	VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	pipelineInfo.stage = compShaderStageInfo;
	pipelineInfo.layout = m_pipelineLayout;

	UtilityVk::checkResult(vkCreateComputePipelines(deviceVk, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "Failed to create pipeline!");

	vkDestroyShaderModule(deviceVk, compShaderModule, nullptr);
}

VEngine::gal::ComputePipelineVk::~ComputePipelineVk()
{
	VkDevice deviceVk = m_device->getDevice();
	vkDestroyPipeline(deviceVk, m_pipeline, nullptr);
	vkDestroyPipelineLayout(deviceVk, m_pipelineLayout, nullptr);

	for (uint32_t i = 0; i < m_descriptorSetLayouts.m_layoutCount; ++i)
	{
		// call destructor and free backing memory
		m_descriptorSetLayouts.m_descriptorSetLayouts[i]->~DescriptorSetLayoutVk();
		m_descriptorSetLayouts.m_descriptorSetLayoutMemoryPool.free(reinterpret_cast<ByteArray<sizeof(DescriptorSetLayoutVk)> *>(m_descriptorSetLayouts.m_descriptorSetLayouts[i]));
	}
}

void *VEngine::gal::ComputePipelineVk::getNativeHandle() const
{
	return (void *)m_pipeline;
}

uint32_t VEngine::gal::ComputePipelineVk::getDescriptorSetLayoutCount() const
{
	return m_descriptorSetLayouts.m_layoutCount;
}

const VEngine::gal::DescriptorSetLayout *VEngine::gal::ComputePipelineVk::getDescriptorSetLayout(uint32_t index) const
{
	assert(index < m_descriptorSetLayouts.m_layoutCount);
	return m_descriptorSetLayouts.m_descriptorSetLayouts[index];
}

VkPipelineLayout VEngine::gal::ComputePipelineVk::getLayout() const
{
	return m_pipelineLayout;
}

static void createShaderStage(VkDevice device,
	const VEngine::gal::ShaderStageCreateInfo &stageDesc,
	VkShaderStageFlagBits stageFlag,
	VkShaderModule &shaderModule,
	VkPipelineShaderStageCreateInfo &stageCreateInfo,
	ReflectionInfo &reflectionInfo,
	VkSpecializationInfo &specInfo)
{
	char path[VEngine::gal::ShaderStageCreateInfo::MAX_PATH_LENGTH + 5];
	strcpy_s(path, stageDesc.m_path);
	strcat_s(path, ".spv");
	std::vector<char> code = VEngine::Utility::readBinaryFile(path);
	VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VEngine::gal::UtilityVk::checkResult(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "Failed to create shader module!");

	stageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stageCreateInfo.stage = stageFlag;
	stageCreateInfo.module = shaderModule;
	stageCreateInfo.pName = "main";

	//if (stageDesc.m_specConstCount)
	//{
	//	specInfo.mapEntryCount = stageDesc.m_specConstCount;
	//	specInfo.pMapEntries = stageDesc.m_specializationEntries;
	//	specInfo.dataSize = stageDesc.m_specConstCount * 4;
	//	specInfo.pData = stageDesc.m_specializationData;
	//
	//	stageCreateInfo.pSpecializationInfo = &specInfo;
	//}

	// reflection
	{
		spirv_cross::Compiler comp(reinterpret_cast<const uint32_t *>(code.data()), code.size() / sizeof(uint32_t));
		// The SPIR-V is now parsed, and we can perform reflection on it.
		spirv_cross::ShaderResources resources = comp.get_shader_resources();

		auto updateLayout = [&](spirv_cross::Resource &resource, uint32_t &set, uint32_t &binding)
		{
			set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
			binding = comp.get_decoration(resource.id, spv::DecorationBinding);

			assert(set < VEngine::gal::DescriptorSetLayoutsVk::MAX_SET_COUNT);
			assert(binding < ReflectionInfo::MAX_BINDING_COUNT);

			auto &type = comp.get_type(resource.type_id);
			uint32_t arraySize = type.array.empty() ? 1 : type.array.front();

			// if the array size comes from a specialization constant, we need to hunt down the value ourselves
			if (!type.array.empty() && !type.array_size_literal[0])
			{
				bool foundValue = false;
				const auto specConsts = comp.get_specialization_constants();
				for (const auto &sc : specConsts)
				{
					// arraySize currently holds the id of the spec const we're looking for
					if (sc.id == arraySize)
					{
						// we found the correct spec constant; now search through the VkSpecializationInfo to look up the actual value
						if (stageCreateInfo.pSpecializationInfo)
						{
							const auto &specInfo = *stageCreateInfo.pSpecializationInfo;
							for (uint32_t i = 0; i < specInfo.mapEntryCount; ++i)
							{
								const auto &mapEntry = specInfo.pMapEntries[i];
								if (mapEntry.constantID == sc.constant_id)
								{
									arraySize = *(uint32_t *)((uint8_t *)specInfo.pData + mapEntry.offset);
									foundValue = true;
									break;
								}
							}
						}
						// if there is no VkSpecializationInfo, or if it doesnt contain the value, look up the default value
						if (!foundValue)
						{
							arraySize = comp.get_constant(sc.id).scalar();
							foundValue = true;
						}
						break;
					}
				}
				assert(foundValue);
			}

			auto &layout = reflectionInfo.m_setLayouts[set];
			assert(layout.m_arraySizes[binding] == 0 || layout.m_arraySizes[binding] == arraySize);
			layout.m_arraySizes[binding] = arraySize;
			layout.m_stageFlags[binding] |= stageFlag;

			reflectionInfo.m_setMask |= 1u << set;
		};

		// uniform buffers
		for (auto &resource : resources.uniform_buffers)
		{
			uint32_t set;
			uint32_t binding;
			updateLayout(resource, set, binding);
			reflectionInfo.m_setLayouts[set].m_uniformBufferMask |= 1u << binding;
		}

		// storage buffers
		for (auto &resource : resources.storage_buffers)
		{
			uint32_t set;
			uint32_t binding;
			updateLayout(resource, set, binding);
			reflectionInfo.m_setLayouts[set].m_storageBufferMask |= 1u << binding;
		}

		// subpass inputs
		for (auto &resource : resources.subpass_inputs)
		{
			uint32_t set;
			uint32_t binding;
			updateLayout(resource, set, binding);
			reflectionInfo.m_setLayouts[set].m_subpassInputMask |= 1u << binding;
		}

		// storage images / texel buffers
		for (auto &resource : resources.storage_images)
		{
			uint32_t set;
			uint32_t binding;
			updateLayout(resource, set, binding);
			if (comp.get_type(resource.type_id).image.dim == spv::Dim::DimBuffer)
			{
				reflectionInfo.m_setLayouts[set].m_storageTexelBufferMask |= 1u << binding;
			}
			else
			{
				reflectionInfo.m_setLayouts[set].m_storageImageMask |= 1u << binding;
			}
		}

		// sampled images
		for (auto &resource : resources.sampled_images)
		{
			uint32_t set;
			uint32_t binding;
			updateLayout(resource, set, binding);
			if (comp.get_type(resource.type_id).image.dim == spv::Dim::DimBuffer)
			{
				reflectionInfo.m_setLayouts[set].m_uniformTexelBufferMask |= 1u << binding;
			}
			else
			{
				reflectionInfo.m_setLayouts[set].m_sampledImageMask |= 1u << binding;
			}
		}

		// separate images
		for (auto &resource : resources.separate_images)
		{
			uint32_t set;
			uint32_t binding;
			updateLayout(resource, set, binding);
			if (comp.get_type(resource.type_id).image.dim == spv::Dim::DimBuffer)
			{
				reflectionInfo.m_setLayouts[set].m_uniformTexelBufferMask |= 1u << binding;
			}
			else
			{
				reflectionInfo.m_setLayouts[set].m_separateImageMask |= 1u << binding;
			}
		}

		// separate samplers
		for (auto &resource : resources.separate_samplers)
		{
			uint32_t set;
			uint32_t binding;
			updateLayout(resource, set, binding);
			reflectionInfo.m_setLayouts[set].m_separateSamplerMask |= 1u << binding;
		}

		// push constant
		if (!resources.push_constant_buffers.empty())
		{
			auto &resource = resources.push_constant_buffers.front();
			uint32_t size = static_cast<uint32_t>(comp.get_declared_struct_size(comp.get_type(resource.base_type_id)));

			reflectionInfo.m_pushConstants.m_size = std::max(size, reflectionInfo.m_pushConstants.m_size);
			reflectionInfo.m_pushConstants.m_stageFlags |= stageFlag;
		}
	}
}

static void createPipelineLayout(VkDevice device, const ReflectionInfo &reflectionInfo, VkPipelineLayout &pipelineLayout, VEngine::gal::DescriptorSetLayoutsVk &descriptorSetLayouts)
{
	VkDescriptorSetLayout layoutsVk[VEngine::gal::DescriptorSetLayoutsVk::MAX_SET_COUNT];

	for (uint32_t i = 0; i < VEngine::gal::DescriptorSetLayoutsVk::MAX_SET_COUNT; ++i)
	{
		if ((reflectionInfo.m_setMask & (1u << i)) != 0)
		{
			auto &setLayout = reflectionInfo.m_setLayouts[i];

			std::vector<VkDescriptorSetLayoutBinding> bindings;

			for (uint32_t j = 0; j < ReflectionInfo::MAX_BINDING_COUNT; ++j)
			{
				size_t count = 0;

				if ((setLayout.m_uniformBufferMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					++count;
				}

				if ((setLayout.m_uniformTexelBufferMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					++count;
				}

				if ((setLayout.m_storageBufferMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					++count;
				}

				if ((setLayout.m_storageTexelBufferMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					++count;
				}

				if ((setLayout.m_subpassInputMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					++count;
				}

				if ((setLayout.m_storageImageMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					++count;
				}

				if ((setLayout.m_sampledImageMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					++count;
				}

				if ((setLayout.m_separateImageMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					++count;
				}

				if ((setLayout.m_separateSamplerMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_SAMPLER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					++count;
				}

				// if count is lager than 1 we have multiple bindings in the same slot
				assert(count <= 1);
			}

			assert(!bindings.empty());

			auto *memory = descriptorSetLayouts.m_descriptorSetLayoutMemoryPool.alloc();
			assert(memory);

			descriptorSetLayouts.m_descriptorSetLayouts[descriptorSetLayouts.m_layoutCount] = 
				new(memory) VEngine::gal::DescriptorSetLayoutVk(device, static_cast<uint32_t>(bindings.size()), bindings.data());
		
			layoutsVk[descriptorSetLayouts.m_layoutCount] = (VkDescriptorSetLayout)descriptorSetLayouts.m_descriptorSetLayouts[descriptorSetLayouts.m_layoutCount]->getNativeHandle();
			
			++descriptorSetLayouts.m_layoutCount;
		}
	}

	VkPushConstantRange pushConstantRange;
	pushConstantRange.stageFlags = reflectionInfo.m_pushConstants.m_stageFlags;
	pushConstantRange.offset = 0;
	pushConstantRange.size = reflectionInfo.m_pushConstants.m_size;

	VkPipelineLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	createInfo.setLayoutCount = descriptorSetLayouts.m_layoutCount;
	createInfo.pSetLayouts = layoutsVk;
	createInfo.pushConstantRangeCount = reflectionInfo.m_pushConstants.m_size > 0 ? 1 : 0;
	createInfo.pPushConstantRanges = reflectionInfo.m_pushConstants.m_size > 0 ? &pushConstantRange : nullptr;

	VEngine::gal::UtilityVk::checkResult(vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout), "Failed to create PipelineLayout!");
}
