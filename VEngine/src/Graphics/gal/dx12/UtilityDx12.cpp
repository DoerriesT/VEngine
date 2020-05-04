#include "UtilityDx12.h"
#include <cassert>

HRESULT VEngine::gal::UtilityDx12::checkResult(HRESULT result, const char *errorMsg, bool exitOnError)
{
	return E_NOTIMPL;
}

D3D12_COMPARISON_FUNC VEngine::gal::UtilityDx12::translate(CompareOp compareOp)
{
	switch (compareOp)
	{
	case CompareOp::NEVER:
		return D3D12_COMPARISON_FUNC_NEVER;
	case CompareOp::LESS:
		return D3D12_COMPARISON_FUNC_LESS;
	case CompareOp::EQUAL:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case CompareOp::LESS_OR_EQUAL:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case CompareOp::GREATER:
		return D3D12_COMPARISON_FUNC_GREATER;
	case CompareOp::NOT_EQUAL:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case CompareOp::GREATER_OR_EQUAL:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case CompareOp::ALWAYS:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	default:
		assert(false);
		return D3D12_COMPARISON_FUNC_ALWAYS;
	}
}

D3D12_TEXTURE_ADDRESS_MODE VEngine::gal::UtilityDx12::translate(SamplerAddressMode addressMode)
{
	switch (addressMode)
	{
	case SamplerAddressMode::REPEAT:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	case SamplerAddressMode::MIRRORED_REPEAT:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	case SamplerAddressMode::CLAMP_TO_EDGE:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case SamplerAddressMode::CLAMP_TO_BORDER:
		return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	case SamplerAddressMode::MIRROR_CLAMP_TO_EDGE:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
	default:
		assert(false);
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

D3D12_FILTER VEngine::gal::UtilityDx12::translate(Filter magFilter, Filter minFilter, SamplerMipmapMode mipmapMode, bool compareEnable)
{
	if (compareEnable)
	{
		if (minFilter == Filter::NEAREST && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_MIN_MAG_MIP_POINT;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		}
	}
	else
	{
		if (minFilter == Filter::NEAREST && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
		}
		else if (minFilter == Filter::NEAREST && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::NEAREST && mipmapMode == SamplerMipmapMode::LINEAR)
		{
			return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		}
		else if (minFilter == Filter::LINEAR && magFilter == Filter::LINEAR && mipmapMode == SamplerMipmapMode::NEAREST)
		{
			return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		}
	}
	assert(false);
	return D3D12_FILTER();
}

UINT VEngine::gal::UtilityDx12::translate(ComponentMapping mapping)
{
	auto translateSwizzle = [](ComponentSwizzle swizzle, D3D12_SHADER_COMPONENT_MAPPING identity)
	{
		switch (swizzle)
		{
		case ComponentSwizzle::IDENTITY:
			return identity;
		case ComponentSwizzle::ZERO:
			return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0;
		case ComponentSwizzle::ONE:
			return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1;
		case ComponentSwizzle::R:
			return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
		case ComponentSwizzle::G:
			return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1;
		case ComponentSwizzle::B:
			return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2;
		case ComponentSwizzle::A:
			return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3;
		default:
			assert(false);
			break;
		}
		return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
	};

	return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
		translateSwizzle(mapping.m_r, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0),
		translateSwizzle(mapping.m_g, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1),
		translateSwizzle(mapping.m_b, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2),
		translateSwizzle(mapping.m_a, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3));
}

D3D12_BLEND VEngine::gal::UtilityDx12::translate(BlendFactor factor)
{
	switch (factor)
	{
	case BlendFactor::ZERO:
		return D3D12_BLEND_ZERO;
	case BlendFactor::ONE:
		return D3D12_BLEND_ONE;
	case BlendFactor::SRC_COLOR:
		return D3D12_BLEND_SRC_COLOR;
	case BlendFactor::ONE_MINUS_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_COLOR;
	case BlendFactor::DST_COLOR:
		return D3D12_BLEND_DEST_COLOR;
	case BlendFactor::ONE_MINUS_DST_COLOR:
		return D3D12_BLEND_INV_DEST_COLOR;
	case BlendFactor::SRC_ALPHA:
		return D3D12_BLEND_SRC_ALPHA;
	case BlendFactor::ONE_MINUS_SRC_ALPHA:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case BlendFactor::DST_ALPHA:
		return D3D12_BLEND_DEST_ALPHA;
	case BlendFactor::ONE_MINUS_DST_ALPHA:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case BlendFactor::CONSTANT_COLOR:
		return D3D12_BLEND_BLEND_FACTOR;
	case BlendFactor::ONE_MINUS_CONSTANT_COLOR:
		return D3D12_BLEND_INV_BLEND_FACTOR;
	case BlendFactor::CONSTANT_ALPHA:
		return D3D12_BLEND_BLEND_FACTOR; // TODO
	case BlendFactor::ONE_MINUS_CONSTANT_ALPHA:
		return D3D12_BLEND_INV_BLEND_FACTOR; // TODO
	case BlendFactor::SRC_ALPHA_SATURATE:
		return D3D12_BLEND_SRC_ALPHA_SAT;
	case BlendFactor::SRC1_COLOR:
		return D3D12_BLEND_SRC1_COLOR;
	case BlendFactor::ONE_MINUS_SRC1_COLOR:
		return D3D12_BLEND_INV_SRC1_COLOR;
	case BlendFactor::SRC1_ALPHA:
		return D3D12_BLEND_SRC1_ALPHA;
	case BlendFactor::ONE_MINUS_SRC1_ALPHA:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	default:
		assert(false);
		break;
	}
	return D3D12_BLEND();
}

D3D12_BLEND_OP VEngine::gal::UtilityDx12::translate(BlendOp blendOp)
{
	switch (blendOp)
	{
	case BlendOp::ADD:
		return D3D12_BLEND_OP_ADD;
	case BlendOp::SUBTRACT:
		return D3D12_BLEND_OP_SUBTRACT;
	case BlendOp::REVERSE_SUBTRACT:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case BlendOp::MIN:
		return D3D12_BLEND_OP_MIN;
	case BlendOp::MAX:
		return D3D12_BLEND_OP_MAX;
	default:
		assert(false);
		break;
	}
	return D3D12_BLEND_OP();
}

D3D12_LOGIC_OP VEngine::gal::UtilityDx12::translate(LogicOp logicOp)
{
	switch (logicOp)
	{
	case LogicOp::CLEAR:
		return D3D12_LOGIC_OP_CLEAR;
	case LogicOp::AND:
		return D3D12_LOGIC_OP_AND;
	case LogicOp::AND_REVERSE:
		return D3D12_LOGIC_OP_AND_REVERSE;
	case LogicOp::COPY:
		return D3D12_LOGIC_OP_COPY;
	case LogicOp::AND_INVERTED:
		return D3D12_LOGIC_OP_AND_INVERTED;
	case LogicOp::NO_OP:
		return D3D12_LOGIC_OP_NOOP;
	case LogicOp::XOR:
		return D3D12_LOGIC_OP_XOR;
	case LogicOp::OR:
		return D3D12_LOGIC_OP_OR;
	case LogicOp::NOR:
		return D3D12_LOGIC_OP_NOR;
	case LogicOp::EQUIVALENT:
		return D3D12_LOGIC_OP_EQUIV;
	case LogicOp::INVERT:
		return D3D12_LOGIC_OP_INVERT;
	case LogicOp::OR_REVERSE:
		return D3D12_LOGIC_OP_OR_REVERSE;
	case LogicOp::COPY_INVERTED:
		return D3D12_LOGIC_OP_COPY_INVERTED;
	case LogicOp::OR_INVERTED:
		return D3D12_LOGIC_OP_OR_INVERTED;
	case LogicOp::NAND:
		return D3D12_LOGIC_OP_NAND;
	case LogicOp::SET:
		return D3D12_LOGIC_OP_SET;
	default:
		assert(false);
		break;
	}
	return D3D12_LOGIC_OP();
}

D3D12_STENCIL_OP VEngine::gal::UtilityDx12::translate(StencilOp stencilOp)
{
	switch (stencilOp)
	{
	case StencilOp::KEEP:
		return D3D12_STENCIL_OP_KEEP;
	case StencilOp::ZERO:
		return D3D12_STENCIL_OP_ZERO;
	case StencilOp::REPLACE:
		return D3D12_STENCIL_OP_REPLACE;
	case StencilOp::INCREMENT_AND_CLAMP:
		return D3D12_STENCIL_OP_INCR_SAT;
	case StencilOp::DECREMENT_AND_CLAMP:
		return D3D12_STENCIL_OP_DECR_SAT;
	case StencilOp::INVERT:
		return D3D12_STENCIL_OP_INVERT;
	case StencilOp::INCREMENT_AND_WRAP:
		return D3D12_STENCIL_OP_INCR;
	case StencilOp::DECREMENT_AND_WRAP:
		return D3D12_STENCIL_OP_DECR;
	default:
		assert(false);
		break;
	}
	return D3D12_STENCIL_OP();
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE VEngine::gal::UtilityDx12::translate(PrimitiveTopology topology)
{
	// TODO: map this better to d3d12
	switch (topology)
	{
	case PrimitiveTopology::POINT_LIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	case PrimitiveTopology::LINE_LIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopology::LINE_STRIP:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopology::TRIANGLE_LIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::TRIANGLE_STRIP:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::TRIANGLE_FAN:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::LINE_LIST_WITH_ADJACENCY:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopology::LINE_STRIP_WITH_ADJACENCY:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopology::TRIANGLE_LIST_WITH_ADJACENCY:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::TRIANGLE_STRIP_WITH_ADJACENCY:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::PATCH_LIST:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	default:
		assert(false);
		break;
	}
	return D3D12_PRIMITIVE_TOPOLOGY_TYPE();
}

D3D12_QUERY_HEAP_TYPE VEngine::gal::UtilityDx12::translate(QueryType queryType)
{
	switch (queryType)
	{
	case QueryType::OCCLUSION:
		return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	case QueryType::PIPELINE_STATISTICS:
		return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
	case QueryType::TIMESTAMP:
		return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	default:
		assert(false);
		break;
	}
	return D3D12_QUERY_HEAP_TYPE();
}

DXGI_FORMAT VEngine::gal::UtilityDx12::translate(Format format)
{
	switch (format)
	{
	case Format::UNDEFINED:
		return DXGI_FORMAT_UNKNOWN;
	case Format::R4G4_UNORM_PACK8:
		break;
	case Format::R4G4B4A4_UNORM_PACK16:
		break;
	case Format::B4G4R4A4_UNORM_PACK16:
		break;
	case Format::R5G6B5_UNORM_PACK16:
		return DXGI_FORMAT_B5G6R5_UNORM;
	case Format::B5G6R5_UNORM_PACK16:
		return DXGI_FORMAT_B5G6R5_UNORM;
	case Format::R5G5B5A1_UNORM_PACK16:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case Format::B5G5R5A1_UNORM_PACK16:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case Format::A1R5G5B5_UNORM_PACK16:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case Format::R8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case Format::R8_SNORM:
		return DXGI_FORMAT_R8_SNORM;
	case Format::R8_USCALED:
		break;
	case Format::R8_SSCALED:
		break;
	case Format::R8_UINT:
		return DXGI_FORMAT_R8_UINT;
	case Format::R8_SINT:
		return DXGI_FORMAT_R8_SINT;
	case Format::R8_SRGB:
		break;
	case Format::R8G8_UNORM:
		return DXGI_FORMAT_R8G8_UNORM;
	case Format::R8G8_SNORM:
		return DXGI_FORMAT_R8G8_SNORM;
	case Format::R8G8_USCALED:
		break;
	case Format::R8G8_SSCALED:
		break;
	case Format::R8G8_UINT:
		return DXGI_FORMAT_R8G8_UINT;
	case Format::R8G8_SINT:
		return DXGI_FORMAT_R8G8_SINT;
	case Format::R8G8_SRGB:
		break;
	case Format::R8G8B8_UNORM:
		break;
	case Format::R8G8B8_SNORM:
		break;
	case Format::R8G8B8_USCALED:
		break;
	case Format::R8G8B8_SSCALED:
		break;
	case Format::R8G8B8_UINT:
		break;
	case Format::R8G8B8_SINT:
		break;
	case Format::R8G8B8_SRGB:
		break;
	case Format::B8G8R8_UNORM:
		break;
	case Format::B8G8R8_SNORM:
		break;
	case Format::B8G8R8_USCALED:
		break;
	case Format::B8G8R8_SSCALED:
		break;
	case Format::B8G8R8_UINT:
		break;
	case Format::B8G8R8_SINT:
		break;
	case Format::B8G8R8_SRGB:
		break;
	case Format::R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case Format::R8G8B8A8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case Format::R8G8B8A8_USCALED:
		break;
	case Format::R8G8B8A8_SSCALED:
		break;
	case Format::R8G8B8A8_UINT:
		return DXGI_FORMAT_R8G8B8A8_UINT;
	case Format::R8G8B8A8_SINT:
		return DXGI_FORMAT_R8G8B8A8_SINT;
	case Format::R8G8B8A8_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case Format::B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case Format::B8G8R8A8_SNORM:
		break;
	case Format::B8G8R8A8_USCALED:
		break;
	case Format::B8G8R8A8_SSCALED:
		break;
	case Format::B8G8R8A8_UINT:
		break;
	case Format::B8G8R8A8_SINT:
		break;
	case Format::B8G8R8A8_SRGB:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	case Format::A8B8G8R8_UNORM_PACK32:
		break;
	case Format::A8B8G8R8_SNORM_PACK32:
		break;
	case Format::A8B8G8R8_USCALED_PACK32:
		break;
	case Format::A8B8G8R8_SSCALED_PACK32:
		break;
	case Format::A8B8G8R8_UINT_PACK32:
		break;
	case Format::A8B8G8R8_SINT_PACK32:
		break;
	case Format::A8B8G8R8_SRGB_PACK32:
		break;
	case Format::A2R10G10B10_UNORM_PACK32:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case Format::A2R10G10B10_SNORM_PACK32:
		break;
	case Format::A2R10G10B10_USCALED_PACK32:
		break;
	case Format::A2R10G10B10_SSCALED_PACK32:
		break;
	case Format::A2R10G10B10_UINT_PACK32:
		return DXGI_FORMAT_R10G10B10A2_UINT;
	case Format::A2R10G10B10_SINT_PACK32:
		break;
	case Format::A2B10G10R10_UNORM_PACK32:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case Format::A2B10G10R10_SNORM_PACK32:
		break;
	case Format::A2B10G10R10_USCALED_PACK32:
		break;
	case Format::A2B10G10R10_SSCALED_PACK32:
		break;
	case Format::A2B10G10R10_UINT_PACK32:
		return DXGI_FORMAT_R10G10B10A2_UINT;
	case Format::A2B10G10R10_SINT_PACK32:
		break;
	case Format::R16_UNORM:
		return DXGI_FORMAT_R16_UNORM;
	case Format::R16_SNORM:
		return DXGI_FORMAT_R16_SNORM;
	case Format::R16_USCALED:
		break;
	case Format::R16_SSCALED:
		break;
	case Format::R16_UINT:
		return DXGI_FORMAT_R16_UINT;
	case Format::R16_SINT:
		return DXGI_FORMAT_R16_SINT;
	case Format::R16_SFLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case Format::R16G16_UNORM:
		return DXGI_FORMAT_R16G16_UNORM;
	case Format::R16G16_SNORM:
		return DXGI_FORMAT_R16G16_SNORM;
	case Format::R16G16_USCALED:
		break;
	case Format::R16G16_SSCALED:
		break;
	case Format::R16G16_UINT:
		return DXGI_FORMAT_R16G16_UINT;
	case Format::R16G16_SINT:
		return DXGI_FORMAT_R16G16_SINT;
	case Format::R16G16_SFLOAT:
		return DXGI_FORMAT_R16G16_FLOAT;
	case Format::R16G16B16_UNORM:
		break;
	case Format::R16G16B16_SNORM:
		break;
	case Format::R16G16B16_USCALED:
		break;
	case Format::R16G16B16_SSCALED:
		break;
	case Format::R16G16B16_UINT:
		break;
	case Format::R16G16B16_SINT:
		break;
	case Format::R16G16B16_SFLOAT:
		break;
	case Format::R16G16B16A16_UNORM:
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	case Format::R16G16B16A16_SNORM:
		return DXGI_FORMAT_R16G16B16A16_SNORM;
	case Format::R16G16B16A16_USCALED:
		break;
	case Format::R16G16B16A16_SSCALED:
		break;
	case Format::R16G16B16A16_UINT:
		return DXGI_FORMAT_R16G16B16A16_UINT;
	case Format::R16G16B16A16_SINT:
		return DXGI_FORMAT_R16G16B16A16_SINT;
	case Format::R16G16B16A16_SFLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case Format::R32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case Format::R32_SINT:
		return DXGI_FORMAT_R32_SINT;
	case Format::R32_SFLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case Format::R32G32_UINT:
		return DXGI_FORMAT_R32G32_UINT;
	case Format::R32G32_SINT:
		return DXGI_FORMAT_R32G32_SINT;
	case Format::R32G32_SFLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case Format::R32G32B32_UINT:
		return DXGI_FORMAT_R32G32B32_UINT;
	case Format::R32G32B32_SINT:
		return DXGI_FORMAT_R32G32B32_SINT;
	case Format::R32G32B32_SFLOAT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case Format::R32G32B32A32_UINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	case Format::R32G32B32A32_SINT:
		return DXGI_FORMAT_R32G32B32A32_SINT;
	case Format::R32G32B32A32_SFLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case Format::R64_UINT:
		break;
	case Format::R64_SINT:
		break;
	case Format::R64_SFLOAT:
		break;
	case Format::R64G64_UINT:
		break;
	case Format::R64G64_SINT:
		break;
	case Format::R64G64_SFLOAT:
		break;
	case Format::R64G64B64_UINT:
		break;
	case Format::R64G64B64_SINT:
		break;
	case Format::R64G64B64_SFLOAT:
		break;
	case Format::R64G64B64A64_UINT:
		break;
	case Format::R64G64B64A64_SINT:
		break;
	case Format::R64G64B64A64_SFLOAT:
		break;
	case Format::B10G11R11_UFLOAT_PACK32:
		return DXGI_FORMAT_R11G11B10_FLOAT;
	case Format::E5B9G9R9_UFLOAT_PACK32:
		return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
	case Format::D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case Format::X8_D24_UNORM_PACK32:
		break;
	case Format::D32_SFLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case Format::S8_UINT:
		break;
	case Format::D16_UNORM_S8_UINT:
		break;
	case Format::D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case Format::D32_SFLOAT_S8_UINT:
		break;
	case Format::BC1_RGB_UNORM_BLOCK:
		break;
	case Format::BC1_RGB_SRGB_BLOCK:
		break;
	case Format::BC1_RGBA_UNORM_BLOCK:
		return DXGI_FORMAT_BC1_UNORM;
	case Format::BC1_RGBA_SRGB_BLOCK:
		return DXGI_FORMAT_BC1_UNORM_SRGB;
	case Format::BC2_UNORM_BLOCK:
		return DXGI_FORMAT_BC2_UNORM;
	case Format::BC2_SRGB_BLOCK:
		return DXGI_FORMAT_BC2_UNORM_SRGB;
	case Format::BC3_UNORM_BLOCK:
		return DXGI_FORMAT_BC3_UNORM;
	case Format::BC3_SRGB_BLOCK:
		return DXGI_FORMAT_BC3_UNORM_SRGB;
	case Format::BC4_UNORM_BLOCK:
		return DXGI_FORMAT_BC4_UNORM;
	case Format::BC4_SNORM_BLOCK:
		return DXGI_FORMAT_BC4_SNORM;
	case Format::BC5_UNORM_BLOCK:
		return DXGI_FORMAT_BC5_UNORM;
	case Format::BC5_SNORM_BLOCK:
		return DXGI_FORMAT_BC5_SNORM;
	case Format::BC6H_UFLOAT_BLOCK:
		return DXGI_FORMAT_BC6H_UF16;
	case Format::BC6H_SFLOAT_BLOCK:
		return DXGI_FORMAT_BC6H_SF16;
	case Format::BC7_UNORM_BLOCK:
		return DXGI_FORMAT_BC7_UNORM;
	case Format::BC7_SRGB_BLOCK:
		return DXGI_FORMAT_BC7_UNORM_SRGB;
	default:
		break;
	}
	assert(false);
	return DXGI_FORMAT();
}
