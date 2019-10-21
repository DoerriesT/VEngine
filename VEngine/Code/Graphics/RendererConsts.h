#pragma once

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
		constexpr uint32_t VOXEL_SCENE_CASCADES = 3;
		constexpr uint32_t VOXEL_SCENE_WIDTH = 128;
		constexpr uint32_t VOXEL_SCENE_HEIGHT = 64;
		constexpr uint32_t VOXEL_SCENE_DEPTH = 128;
		constexpr float VOXEL_SCENE_BASE_SIZE = 0.25f;
		constexpr float Z_BIN_DEPTH = 1.0f;
	}
}