#include "VKPipelineCache.h"
#include "VKContext.h"
#include "Utility/Utility.h"
#include <spirv-cross/spirv_cross.hpp>

VEngine::VKPipelineCache::PipelineData VEngine::VKPipelineCache::getPipeline(const GraphicsPipelineDesc &pipelineDesc, const RenderPassCompatDesc &renderPassDesc, uint32_t subpassIndex, VkRenderPass renderPass)
{
	auto &pipelinePair = m_graphicsPipelines[{pipelineDesc, renderPassDesc, subpassIndex}];

	// pipeline does not exist yet -> create it
	if (pipelinePair.m_pipeline == VK_NULL_HANDLE)
	{
		printf("Creating Pipeline!\n");

		uint32_t stageCount = 0;
		VkShaderModule shaderModules[5] = {};
		VkPipelineShaderStageCreateInfo shaderStages[5] = {};
		VkSpecializationInfo specInfo[5];
		ReflectionInfo reflectionInfo{};

		// create shaders and perform reflection
		{
			if (pipelineDesc.m_vertexShader.m_path[0])
			{
				createShaderStage(pipelineDesc.m_vertexShader, VK_SHADER_STAGE_VERTEX_BIT, shaderModules[stageCount], shaderStages[stageCount], reflectionInfo, specInfo[stageCount]);
				++stageCount;
			}

			if (pipelineDesc.m_tessControlShader.m_path[0])
			{
				createShaderStage(pipelineDesc.m_tessControlShader, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, shaderModules[stageCount], shaderStages[stageCount], reflectionInfo, specInfo[stageCount]);
				++stageCount;
			}

			if (pipelineDesc.m_tessEvalShader.m_path[0])
			{
				createShaderStage(pipelineDesc.m_tessEvalShader, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, shaderModules[stageCount], shaderStages[stageCount], reflectionInfo, specInfo[stageCount]);
				++stageCount;
			}

			if (pipelineDesc.m_geometryShader.m_path[0])
			{
				createShaderStage(pipelineDesc.m_geometryShader, VK_SHADER_STAGE_GEOMETRY_BIT, shaderModules[stageCount], shaderStages[stageCount], reflectionInfo, specInfo[stageCount]);
				++stageCount;
			}

			if (pipelineDesc.m_fragmentShader.m_path[0])
			{
				createShaderStage(pipelineDesc.m_fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, shaderModules[stageCount], shaderStages[stageCount], reflectionInfo, specInfo[stageCount]);
				++stageCount;
			}
		}

		// create descriptor set layouts and pipeline layout
		memset(&pipelinePair.m_descriptorSetLayoutData, 0, sizeof(pipelinePair.m_descriptorSetLayoutData));
		createPipelineLayout(reflectionInfo, pipelinePair.m_layout, pipelinePair.m_descriptorSetLayoutData);

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
		viewportState.scissorCount = pipelineDesc.m_viewportState.m_viewportCount;
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
		pipelineInfo.subpass = subpassIndex;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		if (vkCreateGraphicsPipelines(g_context.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelinePair.m_pipeline) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create pipeline!", EXIT_FAILURE);
		}

		for (uint32_t i = 0; i < stageCount; ++i)
		{
			vkDestroyShaderModule(g_context.m_device, shaderModules[i], nullptr);
		}
	}

	return pipelinePair;
}

VEngine::VKPipelineCache::PipelineData VEngine::VKPipelineCache::getPipeline(const ComputePipelineDesc &pipelineDesc)
{
	auto &pipelinePair = m_computePipelines[pipelineDesc];

	// pipeline does not exist yet -> create it
	if (pipelinePair.m_pipeline == VK_NULL_HANDLE)
	{
		printf("Creating Pipeline!\n");

		VkShaderModule compShaderModule;
		VkPipelineShaderStageCreateInfo compShaderStageInfo;
		ReflectionInfo reflectionInfo{};
		VkSpecializationInfo specInfo;

		// create shader and perform reflection
		createShaderStage(pipelineDesc.m_computeShader, VK_SHADER_STAGE_COMPUTE_BIT, compShaderModule, compShaderStageInfo, reflectionInfo, specInfo);

		// create descriptor set layouts and pipeline layout
		memset(&pipelinePair.m_descriptorSetLayoutData, 0, sizeof(pipelinePair.m_descriptorSetLayoutData));
		createPipelineLayout(reflectionInfo, pipelinePair.m_layout, pipelinePair.m_descriptorSetLayoutData);

		VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pipelineInfo.stage = compShaderStageInfo;
		pipelineInfo.layout = pipelinePair.m_layout;

		if (vkCreateComputePipelines(g_context.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelinePair.m_pipeline) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create pipeline!", EXIT_FAILURE);
		}

		vkDestroyShaderModule(g_context.m_device, compShaderModule, nullptr);
	}

	return pipelinePair;
}

void VEngine::VKPipelineCache::createShaderStage(const ShaderStageDesc &stageDesc,
	VkShaderStageFlagBits stageFlag,
	VkShaderModule &shaderModule,
	VkPipelineShaderStageCreateInfo &stageCreateInfo,
	ReflectionInfo &reflectionInfo,
	VkSpecializationInfo &specInfo)
{
	std::vector<char> code = Utility::readBinaryFile(stageDesc.m_path);
	VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	if (vkCreateShaderModule(g_context.m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create shader module!", EXIT_FAILURE);
	}

	stageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stageCreateInfo.stage = stageFlag;
	stageCreateInfo.module = shaderModule;
	stageCreateInfo.pName = "main";

	if (stageDesc.m_specConstCount)
	{
		specInfo.mapEntryCount = stageDesc.m_specConstCount;
		specInfo.pMapEntries = stageDesc.m_specializationEntries;
		specInfo.dataSize = stageDesc.m_specConstCount * 4;
		specInfo.pData = stageDesc.m_specializationData;
		
		stageCreateInfo.pSpecializationInfo = &specInfo;
	}

	// reflection
	{
		spirv_cross::Compiler comp(reinterpret_cast<const uint32_t *>(code.data()), code.size() / sizeof(uint32_t));
		// The SPIR-V is now parsed, and we can perform reflection on it.
		spirv_cross::ShaderResources resources = comp.get_shader_resources();

		auto updateLayout = [&](spirv_cross::Resource &resource, uint32_t &set, uint32_t &binding)
		{
			set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
			binding = comp.get_decoration(resource.id, spv::DecorationBinding);

			assert(set < ReflectionInfo::MAX_SET_COUNT);
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

void VEngine::VKPipelineCache::createPipelineLayout(const ReflectionInfo &reflectionInfo, VkPipelineLayout &pipelineLayout, DescriptorSetLayoutData &descriptorSetLayoutData)
{
	for (uint32_t i = 0; i < ReflectionInfo::MAX_SET_COUNT; ++i)
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
					descriptorSetLayoutData.m_counts[i][VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] += setLayout.m_arraySizes[j];
					++count;
				}

				if ((setLayout.m_uniformTexelBufferMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					descriptorSetLayoutData.m_counts[i][VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER] += setLayout.m_arraySizes[j];
					++count;
				}

				if ((setLayout.m_storageBufferMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					descriptorSetLayoutData.m_counts[i][VK_DESCRIPTOR_TYPE_STORAGE_BUFFER] += setLayout.m_arraySizes[j];
					++count;
				}

				if ((setLayout.m_storageTexelBufferMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					descriptorSetLayoutData.m_counts[i][VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER] += setLayout.m_arraySizes[j];
					++count;
				}

				if ((setLayout.m_subpassInputMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					descriptorSetLayoutData.m_counts[i][VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT] += setLayout.m_arraySizes[j];
					++count;
				}

				if ((setLayout.m_storageImageMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					descriptorSetLayoutData.m_counts[i][VK_DESCRIPTOR_TYPE_STORAGE_IMAGE] += setLayout.m_arraySizes[j];
					++count;
				}

				if ((setLayout.m_sampledImageMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					descriptorSetLayoutData.m_counts[i][VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] += setLayout.m_arraySizes[j];
					++count;
				}

				if ((setLayout.m_separateImageMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					descriptorSetLayoutData.m_counts[i][VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE] += setLayout.m_arraySizes[j];
					++count;
				}

				if ((setLayout.m_separateSamplerMask & (1u << j)) != 0)
				{
					bindings.push_back({ j, VK_DESCRIPTOR_TYPE_SAMPLER, setLayout.m_arraySizes[j], setLayout.m_stageFlags[j] });
					descriptorSetLayoutData.m_counts[i][VK_DESCRIPTOR_TYPE_SAMPLER] += setLayout.m_arraySizes[j];
					++count;
				}

				// if count is lager than 1 we have multiple bindings in the same slot
				assert(count <= 1);
			}

			assert(!bindings.empty());

			VkDescriptorSetLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
			createInfo.pBindings = bindings.data();

			if (vkCreateDescriptorSetLayout(g_context.m_device, &createInfo, nullptr, &descriptorSetLayoutData.m_layouts[descriptorSetLayoutData.m_layoutCount++]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create DescriptorSetLayout!", EXIT_FAILURE);
			}
		}
	}

	VkPushConstantRange pushConstantRange;
	pushConstantRange.stageFlags = reflectionInfo.m_pushConstants.m_stageFlags;
	pushConstantRange.offset = 0;
	pushConstantRange.size = reflectionInfo.m_pushConstants.m_size;

	VkPipelineLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	createInfo.setLayoutCount = descriptorSetLayoutData.m_layoutCount;
	createInfo.pSetLayouts = descriptorSetLayoutData.m_layouts;
	createInfo.pushConstantRangeCount = reflectionInfo.m_pushConstants.m_size > 0 ? 1 : 0;
	createInfo.pPushConstantRanges = reflectionInfo.m_pushConstants.m_size > 0 ? &pushConstantRange : nullptr;

	if (vkCreatePipelineLayout(g_context.m_device, &createInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create PipelineLayout!", EXIT_FAILURE);
	}
}
