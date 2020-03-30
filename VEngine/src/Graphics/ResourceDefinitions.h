#pragma once
#include "RenderGraph.h"
#include "Graphics/LightData.h"
#include "Utility/Utility.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	namespace ResourceDefinitions
	{
		inline rg::ImageViewHandle createFinalImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "Final Image";
			
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::A2B10G10R10_UNORM_PACK32;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::ImageViewHandle createDepthImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "Depth Image";
			
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue.m_depthStencil.m_depth = 0.0f;
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::D32_SFLOAT;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::ImageViewHandle createUVImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "UV Image";
			
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::R16G16_SNORM;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::ImageViewHandle createDerivativesLengthImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "DDXY Length Image";
			
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::R16G16_SFLOAT;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::ImageViewHandle createDerivativesRotMaterialIdImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "DDXY Rot & Material ID Image";
			
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::R16G16_UINT;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::ImageViewHandle createTangentSpaceImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "Tangent Space Image";
			
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::A2B10G10R10_UINT_PACK32;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::ImageViewHandle createVelocityImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "Velocity Image";
			
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::R16G16_SFLOAT;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::ImageViewHandle createLightImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "Light Image";
			
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::R16G16B16A16_SFLOAT;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::ImageViewHandle createTransparencyAccumImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "Transparency Accum Image";
			
			desc.m_clear = true;
			desc.m_clearValue.m_imageClearValue.m_color.m_float32[0] = 0;
			desc.m_clearValue.m_imageClearValue.m_color.m_float32[1] = 0;
			desc.m_clearValue.m_imageClearValue.m_color.m_float32[2] = 0;
			desc.m_clearValue.m_imageClearValue.m_color.m_float32[3] = 0;
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::R16G16B16A16_SFLOAT;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::ImageViewHandle createGTAOImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "GTAO Image";
			
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::R16G16_SFLOAT;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::ImageViewHandle createDeferredShadowsImageViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height)
		{
			rg::ImageDescription desc = {};
			desc.m_name = "Deferred Shadows Image";
			
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = gal::SampleCount::_1;
			desc.m_format = gal::Format::R8G8B8A8_UNORM;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		}

		inline rg::BufferViewHandle createTiledLightingBitMaskBufferViewHandle(rg::RenderGraph &graph, uint32_t width, uint32_t height, uint32_t itemCount)
		{
			uint32_t w = (width + RendererConsts::LIGHTING_TILE_SIZE - 1) / RendererConsts::LIGHTING_TILE_SIZE;
			uint32_t h = (height + RendererConsts::LIGHTING_TILE_SIZE - 1) / RendererConsts::LIGHTING_TILE_SIZE;
			uint32_t tileCount = w * h;
			uint64_t bufferSize = Utility::alignUp(itemCount == 0 ? 1 : itemCount, uint32_t(32)) / 32 * sizeof(uint32_t) * tileCount;

			rg::BufferDescription desc = {};
			desc.m_name = "Tiled Lighting Bit Mask Buffer";

			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = bufferSize;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline rg::BufferViewHandle createLuminanceHistogramBufferViewHandle(rg::RenderGraph &graph)
		{
			rg::BufferDescription desc = {};
			desc.m_name = "Luminance Histogram Buffer";
			
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * RendererConsts::LUMINANCE_HISTOGRAM_SIZE;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline rg::BufferViewHandle createTriangleFilterIndexCountsBufferViewHandle(rg::RenderGraph &graph, uint32_t drawCount)
		{
			rg::BufferDescription desc = {};
			desc.m_name = "Trinagle Filter Index Counts Buffer";
			
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * drawCount;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline rg::BufferViewHandle createTriangleFilterFilteredIndexBufferViewHandle(rg::RenderGraph &graph, uint32_t count)
		{
			rg::BufferDescription desc = {};
			desc.m_name = "Trinagle Filter Filtered Index Buffer";
			
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * count;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline rg::BufferViewHandle createSDSMDepthBoundsBufferViewHandle(rg::RenderGraph &graph)
		{
			rg::BufferDescription desc = {};
			desc.m_name = "SDSM Depth Bounds Buffer";
			
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * 2;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline rg::BufferViewHandle createSDSMSplitsBufferViewHandle(rg::RenderGraph &graph, uint32_t partitions)
		{
			rg::BufferDescription desc = {};
			desc.m_name = "SDSM Splits Buffer";
			
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(float) * partitions;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline rg::BufferViewHandle createSDSMPartitionBoundsBufferViewHandle(rg::RenderGraph &graph, uint32_t partitions)
		{
			rg::BufferDescription desc = {};
			desc.m_name = "SDSM Partition Bounds Buffer";
			
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * 6 * partitions;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline rg::BufferViewHandle createOcclusionCullingVisibilityBufferViewHandle(rg::RenderGraph &graph, uint32_t instances)
		{
			rg::BufferDescription desc = {};
			desc.m_name = "Occlusion Culling Visibility Buffer";
			
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * instances;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}
	}
}