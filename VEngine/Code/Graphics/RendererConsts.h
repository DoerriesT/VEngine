#pragma once

namespace VEngine
{
	namespace RendererConsts
	{
		constexpr size_t FRAMES_IN_FLIGHT = 2;
		constexpr size_t TEXTURE_ARRAY_SIZE = 512;
		constexpr size_t Z_BINS = 8192;
		constexpr size_t LIGHTING_TILE_SIZE = 16;
		constexpr size_t LUMINANCE_HISTOGRAM_SIZE = 256;
		constexpr size_t MAX_TAA_HALTON_SAMPLES = 16;
		constexpr size_t MAX_MATERIALS = 32 * 1024;
		constexpr size_t STAGING_BUFFER_SIZE = 64 * 1024 * 1024;
		constexpr size_t VERTEX_BUFFER_SIZE = 64 * 1024 * 1024;
		constexpr size_t INDEX_BUFFER_SIZE = 16 * 1024 * 1024;
		constexpr size_t MAX_SUB_MESHES = 32 * 1024;
		constexpr float Z_BIN_DEPTH = 1.0f;
	}
}