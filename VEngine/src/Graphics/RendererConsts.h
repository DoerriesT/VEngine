#pragma once
#include <cstdint>

namespace VEngine
{
	namespace RendererConsts
	{
		constexpr uint32_t FRAMES_IN_FLIGHT = 2;
		constexpr uint32_t TEXTURE_ARRAY_SIZE = 1024;
		constexpr uint32_t Z_BINS = 8192;
		constexpr uint32_t LIGHTING_TILE_SIZE = 16;
		constexpr uint32_t LUMINANCE_HISTOGRAM_SIZE = 256;
		constexpr uint32_t MAX_TAA_HALTON_SAMPLES = 16;
		constexpr uint32_t MAX_MATERIALS = 32 * 1024;
		constexpr uint32_t MAX_VERTICES = 4 * 1024 * 1024; // ~4 million vertices
		constexpr uint32_t MAX_INDICES = MAX_VERTICES * 3;
		constexpr uint32_t STAGING_BUFFER_SIZE = 128 * 1024 * 1024; 
		constexpr uint32_t MAX_SUB_MESHES = 32 * 1024;
		constexpr uint32_t MAPPABLE_UBO_BLOCK_SIZE = 1024 * 1024;
		constexpr uint32_t MAPPABLE_SSBO_BLOCK_SIZE = 8 * 1024 * 1024;
		constexpr uint32_t TRIANGLE_FILTERING_CLUSTER_SIZE = 256;
		constexpr uint32_t SAMPLER_LINEAR_CLAMP_IDX = 0;
		constexpr uint32_t SAMPLER_LINEAR_REPEAT_IDX = 1;
		constexpr uint32_t SAMPLER_POINT_CLAMP_IDX = 2;
		constexpr uint32_t SAMPLER_POINT_REPEAT_IDX = 3;
		constexpr uint32_t REFLECTION_PROBE_CACHE_SIZE = 32;
		constexpr uint32_t REFLECTION_PROBE_MAX_MIPS = 8; // max probe size is 512 and we skip all mips below 4x4
		constexpr uint32_t REFLECTION_PROBE_RES = 256;
		constexpr uint32_t REFLECTION_PROBE_MIPS = 5;
		constexpr float Z_BIN_DEPTH = 1.0f;
	}
}