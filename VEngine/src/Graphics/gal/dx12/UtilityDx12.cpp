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
