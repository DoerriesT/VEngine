#pragma once
#include "RenderGraph.h"
#include "Graphics/LightData.h"
#include "Utility/Utility.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	namespace VKResourceDefinitions
	{
		inline ImageViewHandle createFinalImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
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

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createDepthImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
			desc.m_name = "Depth Image";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue.depthStencil.depth = 0.0f;
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_D32_SFLOAT;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createUVImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
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

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createDerivativesLengthImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
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

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createDerivativesRotMaterialIdImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
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

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createTangentSpaceImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
			desc.m_name = "Tangent Space Image";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = width;
			desc.m_height = height;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = 1;
			desc.m_format = VK_FORMAT_A2B10G10R10_UINT_PACK32;

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createVelocityImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
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

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createLightImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
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

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createTransparencyAccumImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
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

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createTransparencyTransmittanceImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
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

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createTransparencyDeltaImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
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

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline ImageViewHandle createGTAOImageViewHandle(RenderGraph &graph, uint32_t width, uint32_t height)
		{
			ImageDescription desc = {};
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

			return graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		}

		inline BufferViewHandle createPointLightBitMaskBufferViewHandle(RenderGraph &graph, uint32_t width, uint32_t height, uint32_t pointLightCount)
		{
			uint32_t w = width / RendererConsts::LIGHTING_TILE_SIZE + ((width % RendererConsts::LIGHTING_TILE_SIZE == 0) ? 0 : 1);
			uint32_t h = height / RendererConsts::LIGHTING_TILE_SIZE + ((height % RendererConsts::LIGHTING_TILE_SIZE == 0) ? 0 : 1);
			uint32_t tileCount = w * h;
			VkDeviceSize bufferSize = Utility::alignUp(pointLightCount, uint32_t(32)) / 32 * sizeof(uint32_t) * tileCount;

			BufferDescription desc = {};
			desc.m_name = "Point Light Bit Mask Buffer";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = bufferSize;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline BufferViewHandle createLuminanceHistogramBufferViewHandle(RenderGraph &graph)
		{
			BufferDescription desc = {};
			desc.m_name = "Luminance Histogram Buffer";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * RendererConsts::LUMINANCE_HISTOGRAM_SIZE;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline BufferViewHandle createIndirectBufferViewHandle(RenderGraph &graph, uint32_t drawCount)
		{
			BufferDescription desc = {};
			desc.m_name = "Indirect Buffer";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(VkDrawIndexedIndirectCommand) * drawCount;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline BufferViewHandle createIndirectDrawCountsBufferViewHandle(RenderGraph &graph, uint32_t drawCount)
		{
			BufferDescription desc = {};
			desc.m_name = "Indirect Draw Counts Buffer";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * drawCount;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline BufferViewHandle createTriangleFilterIndexCountsBufferViewHandle(RenderGraph &graph, uint32_t drawCount)
		{
			BufferDescription desc = {};
			desc.m_name = "Trinagle Filter Index Counts Buffer";
			desc.m_concurrent = false;
			desc.m_clear = true;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * drawCount;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline BufferViewHandle createTriangleFilterFilteredIndexBufferViewHandle(RenderGraph &graph, uint32_t count)
		{
			BufferDescription desc = {};
			desc.m_name = "Trinagle Filter Filtered Index Buffer";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * count;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline BufferViewHandle createSDSMDepthBoundsBufferViewHandle(RenderGraph &graph)
		{
			BufferDescription desc = {};
			desc.m_name = "SDSM Depth Bounds Buffer";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * 2;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline BufferViewHandle createSDSMSplitsBufferViewHandle(RenderGraph &graph, uint32_t partitions)
		{
			BufferDescription desc = {};
			desc.m_name = "SDSM Splits Buffer";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(float) * partitions;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}

		inline BufferViewHandle createSDSMPartitionBoundsBufferViewHandle(RenderGraph &graph, uint32_t partitions)
		{
			BufferDescription desc = {};
			desc.m_name = "SDSM Partition Bounds Buffer";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_bufferClearValue = 0;
			desc.m_size = sizeof(uint32_t) * 6 * partitions;
			desc.m_size = desc.m_size < 32 ? 32 : desc.m_size;

			return graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
		}
	}
}