#include "PipelineDx12.h"
#include "UtilityDx12.h"
#include "Utility/Utility.h"

static void reflectRootSignature(ID3D12Device *device, size_t blobSize, void *blobData, ID3D12RootSignature **rootSignature, uint32_t &descriptorTableOffset, uint32_t &descriptorTableCount, VEngine::gal::DescriptorTableLayout *layouts);

VEngine::gal::GraphicsPipelineDx12::GraphicsPipelineDx12(ID3D12Device *device, const GraphicsPipelineCreateInfo &createInfo)
	:m_pipeline(),
	m_rootSignature(),
	m_vertexBufferStrides(),
	m_blendFactors(),
	m_stencilRef()
{
	std::vector<char> vsCode;
	std::vector<char> dsCode;
	std::vector<char> hsCode;
	std::vector<char> gsCode;
	std::vector<char> psCode;

	assert(createInfo.m_vertexShader.m_path[0]);
	vsCode = VEngine::Utility::readBinaryFile(createInfo.m_vertexShader.m_path);
	reflectRootSignature(device, vsCode.size(), vsCode.data(), &m_rootSignature, m_descriptorTableOffset, m_descriptorTableCount, m_descriptorTableLayouts);

	if (createInfo.m_tessControlShader.m_path[0])
	{
		dsCode = VEngine::Utility::readBinaryFile(createInfo.m_tessControlShader.m_path);
	}
	if (createInfo.m_tessEvalShader.m_path[0])
	{
		hsCode = VEngine::Utility::readBinaryFile(createInfo.m_tessEvalShader.m_path);
	}
	if (createInfo.m_geometryShader.m_path[0])
	{
		gsCode = VEngine::Utility::readBinaryFile(createInfo.m_geometryShader.m_path);
	}
	if (createInfo.m_fragmentShader.m_path[0])
	{
		psCode = VEngine::Utility::readBinaryFile(createInfo.m_fragmentShader.m_path);
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC stateDesc{};
	stateDesc.pRootSignature = nullptr;// m_rootSignature;
	stateDesc.VS.pShaderBytecode = vsCode.data();
	stateDesc.VS.BytecodeLength = vsCode.size();
	stateDesc.PS.pShaderBytecode = psCode.data();
	stateDesc.PS.BytecodeLength = psCode.size();
	stateDesc.DS.pShaderBytecode = dsCode.data();
	stateDesc.DS.BytecodeLength = dsCode.size();
	stateDesc.HS.pShaderBytecode = hsCode.data();
	stateDesc.HS.BytecodeLength = hsCode.size();
	stateDesc.GS.pShaderBytecode = gsCode.data();
	stateDesc.GS.BytecodeLength = gsCode.size();

	stateDesc.StreamOutput.pSODeclaration = nullptr;
	stateDesc.StreamOutput.NumEntries = 0;
	stateDesc.StreamOutput.pBufferStrides = nullptr;
	stateDesc.StreamOutput.NumStrides = 0;
	stateDesc.StreamOutput.RasterizedStream = 0;

	stateDesc.BlendState.AlphaToCoverageEnable = createInfo.m_multiSampleState.m_alphaToCoverageEnable;
	stateDesc.BlendState.IndependentBlendEnable = false;
	for (size_t i = 0; i < 8; ++i)
	{
		const auto &blendDesc = createInfo.m_blendState.m_attachments[i];
		const uint32_t attachmentCount = createInfo.m_blendState.m_attachmentCount;

		auto &blendDescDx = stateDesc.BlendState.RenderTarget[i];
		memset(&blendDescDx, 0, sizeof(blendDescDx));

		blendDescDx.BlendEnable = i < attachmentCount ? blendDesc.m_blendEnable : false;
		blendDescDx.LogicOpEnable = i < attachmentCount ? createInfo.m_blendState.m_logicOpEnable : false;
		blendDescDx.SrcBlend = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_srcColorBlendFactor) : D3D12_BLEND_ONE;
		blendDescDx.DestBlend = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_dstColorBlendFactor) : D3D12_BLEND_ZERO;
		blendDescDx.BlendOp = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_colorBlendOp) : D3D12_BLEND_OP_ADD;
		blendDescDx.SrcBlendAlpha = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_srcAlphaBlendFactor) : D3D12_BLEND_ONE;
		blendDescDx.DestBlendAlpha = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_dstAlphaBlendFactor) : D3D12_BLEND_ZERO;
		blendDescDx.BlendOpAlpha = i < attachmentCount ? UtilityDx12::translate(blendDesc.m_alphaBlendOp) : D3D12_BLEND_OP_ADD;
		blendDescDx.LogicOp = i < attachmentCount ? UtilityDx12::translate(createInfo.m_blendState.m_logicOp) : D3D12_LOGIC_OP_NOOP;
		blendDescDx.RenderTargetWriteMask = i < attachmentCount ? static_cast<UINT8>(blendDesc.m_colorWriteMask) : D3D12_COLOR_WRITE_ENABLE_ALL;

		stateDesc.BlendState.IndependentBlendEnable = (i < attachmentCount &&i > 0 && memcmp(&stateDesc.BlendState.RenderTarget[i - 1], &blendDescDx, sizeof(blendDescDx)) != 0)
			? true : stateDesc.BlendState.IndependentBlendEnable;
	}
	memcpy(m_blendFactors, createInfo.m_blendState.m_blendConstants, sizeof(m_blendFactors));

	stateDesc.SampleMask = createInfo.m_multiSampleState.m_sampleMask;

	stateDesc.RasterizerState.FillMode = createInfo.m_rasterizationState.m_polygonMode == PolygonMode::FILL ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
	stateDesc.RasterizerState.CullMode = createInfo.m_rasterizationState.m_cullMode == CullModeFlagBits::NONE
		? D3D12_CULL_MODE_NONE : createInfo.m_rasterizationState.m_cullMode == CullModeFlagBits::FRONT_BIT ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK;
	stateDesc.RasterizerState.FrontCounterClockwise = createInfo.m_rasterizationState.m_frontFace == FrontFace::COUNTER_CLOCKWISE;
	stateDesc.RasterizerState.DepthBias = createInfo.m_rasterizationState.m_depthBiasEnable ? static_cast<INT>(createInfo.m_rasterizationState.m_depthBiasConstantFactor) : 0;
	stateDesc.RasterizerState.DepthBiasClamp = createInfo.m_rasterizationState.m_depthBiasEnable ? createInfo.m_rasterizationState.m_depthBiasClamp : 0.0f;
	stateDesc.RasterizerState.SlopeScaledDepthBias = createInfo.m_rasterizationState.m_depthBiasEnable ? createInfo.m_rasterizationState.m_depthBiasSlopeFactor : 0.0f;
	stateDesc.RasterizerState.DepthClipEnable = createInfo.m_rasterizationState.m_depthClampEnable;
	stateDesc.RasterizerState.MultisampleEnable = createInfo.m_multiSampleState.m_rasterizationSamples != SampleCount::_1;
	stateDesc.RasterizerState.AntialiasedLineEnable = false;
	stateDesc.RasterizerState.ForcedSampleCount = createInfo.m_multiSampleState.m_sampleShadingEnable;
	stateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	stateDesc.DepthStencilState.DepthEnable = createInfo.m_depthStencilState.m_depthTestEnable;
	stateDesc.DepthStencilState.DepthWriteMask = createInfo.m_depthStencilState.m_depthTestEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	stateDesc.DepthStencilState.DepthFunc = UtilityDx12::translate(createInfo.m_depthStencilState.m_depthCompareOp);
	stateDesc.DepthStencilState.StencilEnable = createInfo.m_depthStencilState.m_stencilTestEnable;
	stateDesc.DepthStencilState.StencilReadMask = static_cast<UINT8>(createInfo.m_depthStencilState.m_front.m_compareMask);
	stateDesc.DepthStencilState.StencilWriteMask = static_cast<UINT8>(createInfo.m_depthStencilState.m_front.m_writeMask);
	stateDesc.DepthStencilState.FrontFace.StencilFailOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_front.m_failOp);
	stateDesc.DepthStencilState.FrontFace.StencilDepthFailOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_front.m_depthFailOp);
	stateDesc.DepthStencilState.FrontFace.StencilPassOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_front.m_passOp);
	stateDesc.DepthStencilState.FrontFace.StencilFunc = UtilityDx12::translate(createInfo.m_depthStencilState.m_front.m_compareOp);
	stateDesc.DepthStencilState.BackFace.StencilFailOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_back.m_failOp);
	stateDesc.DepthStencilState.BackFace.StencilDepthFailOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_back.m_depthFailOp);
	stateDesc.DepthStencilState.BackFace.StencilPassOp = UtilityDx12::translate(createInfo.m_depthStencilState.m_back.m_passOp);
	stateDesc.DepthStencilState.BackFace.StencilFunc = UtilityDx12::translate(createInfo.m_depthStencilState.m_back.m_compareOp);
	m_stencilRef = createInfo.m_depthStencilState.m_front.m_reference;

	D3D12_INPUT_ELEMENT_DESC inputElements[VertexInputState::MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS];
	for (size_t i = 0; i < createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount; ++i)
	{
		const auto &desc = createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i];
		auto &descDx = inputElements[i];
		descDx = {};
		descDx.SemanticName; // TODO
		descDx.SemanticIndex = 0;
		descDx.Format = (DXGI_FORMAT)desc.m_format;
		descDx.InputSlot = desc.m_binding;
		descDx.AlignedByteOffset = desc.m_offset;
		descDx.InputSlotClass = createInfo.m_vertexInputState.m_vertexBindingDescriptions[desc.m_binding].m_inputRate == VertexInputRate::VERTEX ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;

		m_vertexBufferStrides[descDx.InputSlot] = createInfo.m_vertexInputState.m_vertexBindingDescriptions[desc.m_binding].m_stride;
	}

	stateDesc.InputLayout.NumElements = createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount;
	stateDesc.InputLayout.pInputElementDescs = inputElements;

	stateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // TODO
	stateDesc.PrimitiveTopologyType = UtilityDx12::translate(createInfo.m_inputAssemblyState.m_primitiveTopology);

	stateDesc.NumRenderTargets = createInfo.m_attachmentFormats.m_colorAttachmentCount;
	for (size_t i = 0; i < 8; ++i)
	{
		stateDesc.RTVFormats[i] = (DXGI_FORMAT)createInfo.m_attachmentFormats.m_colorAttachmentFormats[i];
	}
	stateDesc.DSVFormat = (DXGI_FORMAT)createInfo.m_attachmentFormats.m_depthStencilFormat;

	stateDesc.SampleDesc.Count = static_cast<UINT>(createInfo.m_multiSampleState.m_rasterizationSamples);
	stateDesc.SampleDesc.Quality = createInfo.m_multiSampleState.m_rasterizationSamples != SampleCount::_1 ? 1 : 0;

	stateDesc.NodeMask = 0;

	stateDesc.CachedPSO.pCachedBlob = nullptr;
	stateDesc.CachedPSO.CachedBlobSizeInBytes = 0;

	stateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	UtilityDx12::checkResult(device->CreateGraphicsPipelineState(&stateDesc, __uuidof(ID3D12PipelineState), (void **)&m_pipeline), "Failed to create pipeline!");
}

VEngine::gal::GraphicsPipelineDx12::~GraphicsPipelineDx12()
{
	m_pipeline->Release();
	m_rootSignature->Release();
}

void *VEngine::gal::GraphicsPipelineDx12::getNativeHandle() const
{
	return m_pipeline;
}

uint32_t VEngine::gal::GraphicsPipelineDx12::getDescriptorSetLayoutCount() const
{
	return m_descriptorTableCount;
}

const VEngine::gal::DescriptorSetLayout *VEngine::gal::GraphicsPipelineDx12::getDescriptorSetLayout(uint32_t index) const
{
	assert(index < m_descriptorSetLayouts.m_layoutCount);
	return m_descriptorSetLayouts.m_descriptorSetLayouts[index];
}

ID3D12RootSignature *VEngine::gal::GraphicsPipelineDx12::getRootSignature() const
{
	return m_rootSignature;
}

uint32_t VEngine::gal::GraphicsPipelineDx12::getDescriptorTableOffset() const
{
	return m_descriptorTableOffset;
}

uint32_t VEngine::gal::GraphicsPipelineDx12::getVertexBufferStride(uint32_t bufferBinding) const
{
	assert(bufferBinding < 32);
	return m_vertexBufferStrides[bufferBinding];
}

void VEngine::gal::GraphicsPipelineDx12::initializeState(ID3D12GraphicsCommandList *cmdList) const
{
	cmdList->OMSetBlendFactor(m_blendFactors);
	cmdList->OMSetStencilRef(m_stencilRef);
}

VEngine::gal::ComputePipelineDx12::ComputePipelineDx12(ID3D12Device *device, const ComputePipelineCreateInfo &createInfo)
	:m_pipeline(),
	m_rootSignature()
{
	std::vector<char> csCode = VEngine::Utility::readBinaryFile(createInfo.m_computeShader.m_path);

	reflectRootSignature(device, csCode.size(), csCode.data(), &m_rootSignature, m_descriptorTableOffset, m_descriptorTableCount, m_descriptorTableLayouts);

	D3D12_COMPUTE_PIPELINE_STATE_DESC stateDesc{};
	stateDesc.pRootSignature = nullptr;// m_rootSignature;
	stateDesc.CS.pShaderBytecode = csCode.data();
	stateDesc.CS.BytecodeLength = csCode.size();
	stateDesc.NodeMask = 0;
	stateDesc.CachedPSO.pCachedBlob = nullptr;
	stateDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	stateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	UtilityDx12::checkResult(device->CreateComputePipelineState(&stateDesc, __uuidof(ID3D12PipelineState), (void **)&m_pipeline), "Failed to create pipeline!");
}

VEngine::gal::ComputePipelineDx12::~ComputePipelineDx12()
{
	m_pipeline->Release();
	m_rootSignature->Release();
}

void *VEngine::gal::ComputePipelineDx12::getNativeHandle() const
{
	return m_pipeline;
}

uint32_t VEngine::gal::ComputePipelineDx12::getDescriptorSetLayoutCount() const
{
	return m_descriptorTableCount;
}

const VEngine::gal::DescriptorSetLayout *VEngine::gal::ComputePipelineDx12::getDescriptorSetLayout(uint32_t index) const
{
	assert(index < m_descriptorSetLayouts.m_layoutCount);
	return m_descriptorSetLayouts.m_descriptorSetLayouts[index];
}

ID3D12RootSignature *VEngine::gal::ComputePipelineDx12::getRootSignature() const
{
	return m_rootSignature;
}

uint32_t VEngine::gal::ComputePipelineDx12::getDescriptorTableOffset() const
{
	return m_descriptorTableOffset;
}

using namespace VEngine;
using namespace VEngine::gal;

static void reflectRootSignature(ID3D12Device *device, size_t blobSize, void *blobData, ID3D12RootSignature **rootSignature, uint32_t &descriptorTableOffset, uint32_t &descriptorTableCount, DescriptorTableLayout *layouts)
{
	ID3D12VersionedRootSignatureDeserializer *deserializer;
	UtilityDx12::checkResult(D3D12CreateVersionedRootSignatureDeserializer(blobData, blobSize, __uuidof(ID3D12VersionedRootSignatureDeserializer), (void **)&deserializer), "Failed to create root signature deserializer");

	const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *rootSigDesc;
	UtilityDx12::checkResult(deserializer->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1, &rootSigDesc), "Failed to get root signature!");

	// reflect
	{
		const auto &desc = rootSigDesc->Desc_1_1;

		// validate our constraints on root signature layout
		// we only support descriptor tables and optionally root constants in slot 0
		for (size_t i = 0; i < desc.NumParameters; ++i)
		{
			const auto &param = desc.pParameters[i];

			if (i == 0 && param.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && param.ParameterType != D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
			{
				Utility::fatalExit("Illegal root parameter in first root signature slot! Only descriptor tables and root constants are allowed!", EXIT_FAILURE);
			}
			if (i > 0 && param.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				Utility::fatalExit("Illegal root parameter in root signature! Only descriptor tables are allowed in root signature slots > 0!", EXIT_FAILURE);
			}
		}

		// determine binding offset of descriptor tables
		descriptorTableOffset = (desc.NumParameters > 0 && desc.pParameters[0].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) ? 1 : 0;
		descriptorTableCount = desc.NumParameters - descriptorTableOffset;

		if (descriptorTableCount > 4)
		{
			Utility::fatalExit("Too many descriptor tables in root signature! Limit is 4.", EXIT_FAILURE);
		}

		// create descriptor table layout
		for (size_t i = descriptorTableOffset; i < desc.NumParameters; ++i)
		{
			const auto &table = desc.pParameters[i].DescriptorTable;

			if (table.NumDescriptorRanges > 32)
			{
				Utility::fatalExit("Too many descriptor ranges in descriptor table! Limit is 32.", EXIT_FAILURE);
			}

			auto &layout = layouts[i - descriptorTableOffset];
			layout = {};
			layout.m_rangeCount = table.NumDescriptorRanges;

			for (size_t j = 0; j < table.NumDescriptorRanges; ++j)
			{
				layout.m_rangeTypes[j] = table.pDescriptorRanges[j].RangeType;
				layout.m_descriptorCount[j] = table.pDescriptorRanges[j].NumDescriptors;
				layout.m_offsetFromTableStart[j] = table.pDescriptorRanges[j].OffsetInDescriptorsFromTableStart;
				layout.m_totalDescriptorCount += layout.m_descriptorCount[j];
			}
		}
	}

	deserializer->Release();

	UtilityDx12::checkResult(device->CreateRootSignature(0, blobData, blobSize, __uuidof(ID3D12RootSignature), (void **)rootSignature), "Failed to create root signature!");
}
