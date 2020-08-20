#include "PipelineDx12.h"
#include "UtilityDx12.h"
#include "Utility/Utility.h"
#include <d3d12shader.h>
#include <dxc/dxcapi.h>
#include <dxc/Support/dxcapi.use.h>
#include <dxc/DxilContainer/DxilContainer.h>

static dxc::DxcDllSupport s_dxcDllSupport;

using namespace VEngine;
using namespace VEngine::gal;

namespace
{
	struct ReflectionInfo
	{
		enum
		{
			MAX_BINDING_COUNT = 32,
		};

		enum
		{
			VERTEX_BIT = 1u << 0u,
			HULL_BIT = 1u << 1u,
			DOMAIN_BIT = 1u << 2u,
			GEOMETRY_BIT = 1u << 3u,
			PIXEL_BIT = 1u << 4u,
			COMPUTE_BIT = 1u << 5u,
		};

		struct SetLayout
		{
			uint32_t m_srvMask;
			uint32_t m_uavMask;
			uint32_t m_cbvMask;
			uint32_t m_samplerMask;
			uint32_t m_arraySizes[MAX_BINDING_COUNT];
			uint32_t m_stageFlags[MAX_BINDING_COUNT];
		};

		struct PushConstantInfo
		{
			uint32_t m_size;
			uint32_t m_stageFlags;
		};

		SetLayout m_setLayouts[4];
		PushConstantInfo m_pushConstants;
		uint32_t m_setMask;
	};
}

static void reflectShader(ID3D12Device *device, size_t blobSize, void *blobData, uint32_t stageFlag, ReflectionInfo &reflectionInfo);
static ID3D12RootSignature *createRootSignature(ID3D12Device *device, const ReflectionInfo &reflectionInfo, bool useIA, uint32_t &descriptorTableOffset, uint32_t &descriptorTableCount);

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

	ReflectionInfo reflectionInfo{};

	if (createInfo.m_vertexShader.m_path[0])
	{
		vsCode = VEngine::Utility::readBinaryFile(createInfo.m_vertexShader.m_path);
		reflectShader(device, vsCode.size(), vsCode.data(), ReflectionInfo::VERTEX_BIT, reflectionInfo);
	}
	if (createInfo.m_tessControlShader.m_path[0])
	{
		dsCode = VEngine::Utility::readBinaryFile(createInfo.m_tessControlShader.m_path);
		reflectShader(device, dsCode.size(), dsCode.data(), ReflectionInfo::DOMAIN_BIT, reflectionInfo);
	}
	if (createInfo.m_tessEvalShader.m_path[0])
	{
		hsCode = VEngine::Utility::readBinaryFile(createInfo.m_tessEvalShader.m_path);
		reflectShader(device, hsCode.size(), hsCode.data(), ReflectionInfo::HULL_BIT, reflectionInfo);
	}
	if (createInfo.m_geometryShader.m_path[0])
	{
		gsCode = VEngine::Utility::readBinaryFile(createInfo.m_geometryShader.m_path);
		reflectShader(device, gsCode.size(), gsCode.data(), ReflectionInfo::GEOMETRY_BIT, reflectionInfo);
	}
	if (createInfo.m_fragmentShader.m_path[0])
	{
		psCode = VEngine::Utility::readBinaryFile(createInfo.m_fragmentShader.m_path);
		reflectShader(device, psCode.size(), psCode.data(), ReflectionInfo::PIXEL_BIT, reflectionInfo);
	}

	// create root signature from reflection data
	m_rootSignature = createRootSignature(device, reflectionInfo, createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount > 0, m_descriptorTableOffset, m_descriptorTableCount);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC stateDesc{};
	stateDesc.pRootSignature = m_rootSignature;

	// shaders
	{
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
	}

	// stream output
	{
		stateDesc.StreamOutput.pSODeclaration = nullptr;
		stateDesc.StreamOutput.NumEntries = 0;
		stateDesc.StreamOutput.pBufferStrides = nullptr;
		stateDesc.StreamOutput.NumStrides = 0;
		stateDesc.StreamOutput.RasterizedStream = 0;
	}

	// blend state
	{
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
	}

	stateDesc.SampleMask = createInfo.m_multiSampleState.m_sampleMask;

	// rasterizer state
	{
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
	}

	// depth stencil state
	{
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
	}

	m_stencilRef = createInfo.m_depthStencilState.m_front.m_reference;

	D3D12_INPUT_ELEMENT_DESC inputElements[VertexInputState::MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS];
	for (size_t i = 0; i < createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount; ++i)
	{
		const auto &desc = createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i];
		auto &descDx = inputElements[i];
		descDx = {};
		descDx.SemanticName; // TODO
		descDx.SemanticIndex = 0;
		descDx.Format = UtilityDx12::translate(desc.m_format);
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
		stateDesc.RTVFormats[i] = UtilityDx12::translate(createInfo.m_attachmentFormats.m_colorAttachmentFormats[i]);
	}
	stateDesc.DSVFormat = UtilityDx12::translate(createInfo.m_attachmentFormats.m_depthStencilFormat);

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

	// create root signature from reflection data
	ReflectionInfo reflectionInfo{};
	reflectShader(device, csCode.size(), csCode.data(), ReflectionInfo::COMPUTE_BIT, reflectionInfo);
	m_rootSignature = createRootSignature(device, reflectionInfo, false, m_descriptorTableOffset, m_descriptorTableCount);

	D3D12_COMPUTE_PIPELINE_STATE_DESC stateDesc{};
	stateDesc.pRootSignature = m_rootSignature;
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

static void reflectShader(ID3D12Device *device, size_t blobSize, void *blobData, uint32_t stageFlag, ReflectionInfo &reflectionInfo)
{
	// TODO: is there a better way to get a IDxcBlob containing the shader blob?
	struct ShaderBlob : public IDxcBlob
	{
		size_t m_size;
		void *m_data;

		explicit ShaderBlob(size_t size, void *data) : m_size(size), m_data(data) { }

		// IUnknown interface (not actually implemented)
		HRESULT QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override { assert(false); return 0; }
		ULONG AddRef(void)  override { assert(false); return 0; }
		ULONG Release(void) override { assert(false); return 0; }

		// IDxcBlob interface
		LPVOID GetBufferPointer(void) override { return m_data; }
		SIZE_T GetBufferSize(void) override { return m_size; }
	};

	// create blob from shader data
	ShaderBlob shaderBlob(blobSize, blobData);

	// initialize dxc dll once
	if (!s_dxcDllSupport.IsEnabled())
	{
		s_dxcDllSupport.Initialize();
	}

	IDxcContainerReflection *containerReflection;
	IDxcLibrary *library;
	UtilityDx12::checkResult(s_dxcDllSupport.CreateInstance(CLSID_DxcContainerReflection, &containerReflection));
	UtilityDx12::checkResult(s_dxcDllSupport.CreateInstance(CLSID_DxcLibrary, &library));

	UINT32 shaderIdx;
	ID3D12ShaderReflection *shaderReflection;
	UtilityDx12::checkResult(containerReflection->Load(&shaderBlob));
	UtilityDx12::checkResult(containerReflection->FindFirstPartKind(hlsl::DFCC_DXIL, &shaderIdx));
	UtilityDx12::checkResult(containerReflection->GetPartReflection(shaderIdx, __uuidof(ID3D12ShaderReflection), (void **)&shaderReflection));



	D3D12_SHADER_DESC desc;
	UtilityDx12::checkResult(shaderReflection->GetDesc(&desc), "Failed to retrieve shader description from reflection!");

	// iterate over all resource and fill reflection info struct
	for (UINT i = 0; i < desc.BoundResources; ++i)
	{
		D3D12_SHADER_INPUT_BIND_DESC bindDesc;
		UtilityDx12::checkResult(shaderReflection->GetResourceBindingDesc(i, &bindDesc), "Failed to retrieve resource binding description from shader description!");

		if (bindDesc.Space >= 4)
		{
			Utility::fatalExit("Shader uses a register space >= 4! Only spaces 0-3 are permitted.", EXIT_FAILURE);
		}

		if (bindDesc.BindPoint >= 32)
		{
			Utility::fatalExit("Shader uses a register >= 32! Only registers 0-31 are permitted.", EXIT_FAILURE);
		}

		uint32_t *mask = nullptr;

		switch (bindDesc.Type)
		{
		case D3D_SIT_CBUFFER:
			mask = &reflectionInfo.m_setLayouts[bindDesc.Space].m_cbvMask;
			break;
		case D3D_SIT_TBUFFER:
		case D3D_SIT_TEXTURE:
		case D3D_SIT_STRUCTURED:
		case D3D_SIT_BYTEADDRESS:
			mask = &reflectionInfo.m_setLayouts[bindDesc.Space].m_srvMask;
			break;
		case D3D_SIT_SAMPLER:
			mask = &reflectionInfo.m_setLayouts[bindDesc.Space].m_samplerMask;
			break;
		case D3D_SIT_UAV_RWTYPED:
		case D3D_SIT_UAV_RWSTRUCTURED:
		case D3D_SIT_UAV_RWBYTEADDRESS:
		case D3D_SIT_UAV_APPEND_STRUCTURED:
		case D3D_SIT_UAV_CONSUME_STRUCTURED:
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
			mask = &reflectionInfo.m_setLayouts[bindDesc.Space].m_uavMask;
			break;
		default:
			assert(false);
			break;
		}

		assert(mask);

		*mask |= 1u << bindDesc.BindPoint;
		reflectionInfo.m_setLayouts[bindDesc.Space].m_arraySizes[bindDesc.BindPoint] = bindDesc.BindCount;
		reflectionInfo.m_setLayouts[bindDesc.Space].m_stageFlags[bindDesc.BindPoint] |= stageFlag;
	}


	shaderReflection->Release();
	library->Release();
	containerReflection->Release();
}

static ID3D12RootSignature *createRootSignature(ID3D12Device *device, const ReflectionInfo &reflectionInfo, bool useIA, uint32_t &descriptorTableOffset, uint32_t &descriptorTableCount)
{
	descriptorTableOffset = 0;
	descriptorTableCount = 0;

	D3D12_ROOT_PARAMETER1 rootParams[5]; // 1 root constant and 4 descriptor tables maximum
	UINT rootParamCount = 0;

	// keep track of all stages requiring access to resources. we might be able to use DENY flags for some stages
	uint32_t mergedStageMask = 0;

	std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorRanges;
	descriptorRanges.reserve(std::size(reflectionInfo.m_setLayouts) * ReflectionInfo::MAX_BINDING_COUNT); // reserve worst case space so we dont invalidate any pointers

	// fill root signature with params
	{
		auto determineShaderVisibility = [](uint32_t stageFlags) ->D3D12_SHADER_VISIBILITY
		{
			switch (stageFlags)
			{
			case ReflectionInfo::VERTEX_BIT:
				return D3D12_SHADER_VISIBILITY_VERTEX;
			case ReflectionInfo::HULL_BIT:
				return D3D12_SHADER_VISIBILITY_HULL;
			case ReflectionInfo::DOMAIN_BIT:
				return D3D12_SHADER_VISIBILITY_DOMAIN;
			case ReflectionInfo::GEOMETRY_BIT:
				return D3D12_SHADER_VISIBILITY_GEOMETRY;
			case ReflectionInfo::PIXEL_BIT:
				return D3D12_SHADER_VISIBILITY_PIXEL;
			case ReflectionInfo::COMPUTE_BIT:
				return D3D12_SHADER_VISIBILITY_ALL;
			default:
				return D3D12_SHADER_VISIBILITY_ALL;
			}
		};

		// root consts
		if (reflectionInfo.m_pushConstants.m_size)
		{
			constexpr UINT rootConstRegister = 0;
			constexpr UINT rootConstSpace = 5000;

			assert(reflectionInfo.m_pushConstants.m_size % 4u == 0);

			auto &param = rootParams[rootParamCount++];
			param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			param.Constants.ShaderRegister = rootConstRegister;
			param.Constants.RegisterSpace = rootConstSpace;
			param.Constants.Num32BitValues = reflectionInfo.m_pushConstants.m_size / 4u;
			param.ShaderVisibility = determineShaderVisibility(reflectionInfo.m_pushConstants.m_stageFlags);

			mergedStageMask |= reflectionInfo.m_pushConstants.m_stageFlags;

			descriptorTableOffset = 1;
		}

		// descriptor tables
		for (uint32_t i = 0; i < std::size(reflectionInfo.m_setLayouts); ++i)
		{
			// check if this descriptor set (table/space) is used
			if ((reflectionInfo.m_setMask & (1u << i)) != 0)
			{
				auto &setLayout = reflectionInfo.m_setLayouts[i];

				const size_t rangeOffset = descriptorRanges.size();
				uint32_t tableStageMask = 0;

				// keep track of current descriptor offset in this table
				UINT curDescriptorOffset = 0;

				// iterate over all possible bindings
				for (uint32_t j = 0; j < ReflectionInfo::MAX_BINDING_COUNT; ++j)
				{
					size_t count = 0;

					// SRV
					if ((setLayout.m_srvMask & (1u << j)) != 0)
					{
						D3D12_DESCRIPTOR_RANGE1 range{};
						range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
						range.NumDescriptors = setLayout.m_arraySizes[j];
						range.BaseShaderRegister = j;
						range.RegisterSpace = i;
						range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE; // TODO: define more exact flags
						range.OffsetInDescriptorsFromTableStart = curDescriptorOffset;

						descriptorRanges.push_back(range);
						++count;

						tableStageMask |= setLayout.m_stageFlags[j];
					}

					// UAV
					if ((setLayout.m_uavMask & (1u << j)) != 0)
					{
						D3D12_DESCRIPTOR_RANGE1 range{};
						range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
						range.NumDescriptors = setLayout.m_arraySizes[j];
						range.BaseShaderRegister = j;
						range.RegisterSpace = i;
						range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE; // TODO: define more exact flags
						range.OffsetInDescriptorsFromTableStart = curDescriptorOffset;

						descriptorRanges.push_back(range);
						++count;

						tableStageMask |= setLayout.m_stageFlags[j];
					}

					// CBV
					if ((setLayout.m_cbvMask & (1u << j)) != 0)
					{
						D3D12_DESCRIPTOR_RANGE1 range{};
						range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
						range.NumDescriptors = setLayout.m_arraySizes[j];
						range.BaseShaderRegister = j;
						range.RegisterSpace = i;
						range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE; // TODO: define more exact flags
						range.OffsetInDescriptorsFromTableStart = curDescriptorOffset;

						descriptorRanges.push_back(range);
						++count;

						tableStageMask |= setLayout.m_stageFlags[j];
					}

					// sampler
					if ((setLayout.m_samplerMask & (1u << j)) != 0)
					{
						D3D12_DESCRIPTOR_RANGE1 range{};
						range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
						range.NumDescriptors = setLayout.m_arraySizes[j];
						range.BaseShaderRegister = j;
						range.RegisterSpace = i;
						range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE; // TODO: define more exact flags
						range.OffsetInDescriptorsFromTableStart = curDescriptorOffset;

						descriptorRanges.push_back(range);
						++count;

						tableStageMask |= setLayout.m_stageFlags[j];
					}

					curDescriptorOffset += setLayout.m_arraySizes[j];

					// if count is lager than 1 we have multiple bindings in the same slot
					assert(count <= 1);
				}

				auto &param = rootParams[rootParamCount++];
				param = {};
				param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(descriptorRanges.size() - rangeOffset);
				param.DescriptorTable.pDescriptorRanges = descriptorRanges.data() + rangeOffset;
				param.ShaderVisibility = determineShaderVisibility(tableStageMask);

				mergedStageMask |= tableStageMask;

				++descriptorTableCount;
			}
		}

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSig{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
		rootSig.Desc_1_1.NumParameters = rootParamCount;
		rootSig.Desc_1_1.pParameters = rootParams;
		rootSig.Desc_1_1.Flags = useIA ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;

		// try to add DENY flags
		{
			if ((mergedStageMask & ReflectionInfo::VERTEX_BIT) == 0)
			{
				rootSig.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
			}
			if ((mergedStageMask & ReflectionInfo::HULL_BIT) == 0)
			{
				rootSig.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
			}
			if ((mergedStageMask & ReflectionInfo::DOMAIN_BIT) == 0)
			{
				rootSig.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
			}
			if ((mergedStageMask & ReflectionInfo::GEOMETRY_BIT) == 0)
			{
				rootSig.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
			}
			if ((mergedStageMask & ReflectionInfo::PIXEL_BIT) == 0)
			{
				rootSig.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
			}
		}

		ID3DBlob *serializedRootSig;
		ID3DBlob *errorBlob;
		UtilityDx12::checkResult(D3D12SerializeVersionedRootSignature(&rootSig, &serializedRootSig, &errorBlob), "Failed to serialize root signature");

		ID3D12RootSignature *rootSignature;
		UtilityDx12::checkResult(device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), __uuidof(ID3D12RootSignature), (void **)&rootSignature));

		return rootSignature;
	}
}
