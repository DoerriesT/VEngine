#pragma once
#include "FrameGraph/FrameGraph.h"
#include "Graphics/LightData.h"
#include "Utility/Utility.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	namespace VKResourceDefinitions
	{
		inline FrameGraph::ImageHandle createFinalImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "Final Image";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_R8G8B8A8_UNORM;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createDepthImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "Depth Image";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_imageClearValue.depthStencil.depth = 0.0f;
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_D32_SFLOAT;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createUVImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "UV Image";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_R16G16_SNORM;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createDerivativesLengthImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "DDXY Length Image";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_R16G16_SFLOAT;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createDerivativesRotMaterialIdImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "DDXY Rot & Material ID Image";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_R16G16_UINT;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createTangentSpaceImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "Tangent Space Image";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_A2R10G10B10_UINT_PACK32;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createVelocityImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "Velocity Image";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_R16G16_SFLOAT;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createLightImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "Light Image";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createTransparencyAccumImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "Transparency Accum Image";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_imageClearValue.color.float32[0] = 0;
			desc.m_clearValue.m_imageClearValue.color.float32[1] = 0;
			desc.m_clearValue.m_imageClearValue.color.float32[2] = 0;
			desc.m_clearValue.m_imageClearValue.color.float32[3] = 0;
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createTransparencyTransmittanceImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "Transparency Transmittance Image";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_imageClearValue.color.float32[0] = 1;
			desc.m_clearValue.m_imageClearValue.color.float32[1] = 1;
			desc.m_clearValue.m_imageClearValue.color.float32[2] = 1;
			desc.m_clearValue.m_imageClearValue.color.float32[3] = 0;
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_R8G8B8A8_UNORM;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createTransparencyDeltaImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "Transparency Delta Image";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_imageClearValue.color.float32[0] = 0;
			desc.m_clearValue.m_imageClearValue.color.float32[1] = 0;
			desc.m_clearValue.m_imageClearValue.color.float32[2] = 0;
			desc.m_clearValue.m_imageClearValue.color.float32[3] = 0;
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_R8G8_UNORM;

			return graph.createImage(desc);
		}

		inline FrameGraph::ImageHandle createGTAOImageHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height)
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "GTAO Image";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_R16G16_SFLOAT;

			return graph.createImage(desc);
		}

		inline FrameGraph::BufferHandle createPointLightBitMaskBufferHandle(FrameGraph::Graph &graph, uint32_t width, uint32_t height, uint32_t pointLightCount)
		{
			uint32_t w = width / RendererConsts::LIGHTING_TILE_SIZE + ((width % RendererConsts::LIGHTING_TILE_SIZE == 0) ? 0 : 1);
			uint32_t h = height / RendererConsts::LIGHTING_TILE_SIZE + ((height % RendererConsts::LIGHTING_TILE_SIZE == 0) ? 0 : 1);
			uint32_t tileCount = w * h;
			VkDeviceSize bufferSize = Utility::alignUp(pointLightCount, uint32_t(32)) / 32 * sizeof(uint32_t) * tileCount;

			FrameGraph::BufferDescription desc = {};
			desc.m_name = "Point Light Bit Mask Buffer";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = bufferSize;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;
			desc.m_hostVisible = false;

			return graph.createBuffer(desc);
		}

		inline FrameGraph::BufferHandle createLuminanceHistogramBufferHandle(FrameGraph::Graph &graph)
		{
			FrameGraph::BufferDescription desc = {};
			desc.m_name = "Luminance Histogram Buffer";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * RendererConsts::LUMINANCE_HISTOGRAM_SIZE;
			desc.m_hostVisible = false;

			return graph.createBuffer(desc);
		}

		inline FrameGraph::BufferHandle createIndirectBufferHandle(FrameGraph::Graph &graph, uint32_t drawCount)
		{
			FrameGraph::BufferDescription desc = {};
			desc.m_name = "Indirect Buffer";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(VkDrawIndexedIndirectCommand) * drawCount;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;
			desc.m_hostVisible = false;

			return graph.createBuffer(desc);
		}

		inline FrameGraph::BufferHandle createIndirectDrawCountsBufferHandle(FrameGraph::Graph &graph, uint32_t drawCount)
		{
			FrameGraph::BufferDescription desc = {};
			desc.m_name = "Indirect Draw Counts Buffer";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * drawCount;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;
			desc.m_hostVisible = false;

			return graph.createBuffer(desc);
		}

		inline FrameGraph::BufferHandle createTriangleFilterIndexCountsBufferHandle(FrameGraph::Graph &graph, uint32_t drawCount)
		{
			FrameGraph::BufferDescription desc = {};
			desc.m_name = "Trinagle Filter Index Counts Buffer";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * drawCount;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;
			desc.m_hostVisible = false;

			return graph.createBuffer(desc);
		}

		inline FrameGraph::BufferHandle createTriangleFilterFilteredIndexBufferHandle(FrameGraph::Graph &graph, uint32_t count)
		{
			FrameGraph::BufferDescription desc = {};
			desc.m_name = "Trinagle Filter Filtered Index Buffer";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * count;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;
			desc.m_hostVisible = false;

			return graph.createBuffer(desc);
		}

		inline FrameGraph::BufferHandle createSDSMDepthBoundsBufferHandle(FrameGraph::Graph &graph)
		{
			FrameGraph::BufferDescription desc = {};
			desc.m_name = "SDSM Depth Bounds Buffer";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * 2;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;
			desc.m_hostVisible = false;

			return graph.createBuffer(desc);
		}

		inline FrameGraph::BufferHandle createSDSMSplitsBufferHandle(FrameGraph::Graph &graph, uint32_t partitions)
		{
			FrameGraph::BufferDescription desc = {};
			desc.m_name = "SDSM Splits Buffer";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(float) * 2 * partitions;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;
			desc.m_hostVisible = false;

			return graph.createBuffer(desc);
		}

		inline FrameGraph::BufferHandle createSDSMPartitionBoundsBufferHandle(FrameGraph::Graph &graph, uint32_t partitions)
		{
			FrameGraph::BufferDescription desc = {};
			desc.m_name = "SDSM Partition Bounds Buffer";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * 6 * partitions;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;
			desc.m_hostVisible = false;

			return graph.createBuffer(desc);
		}
	}
}