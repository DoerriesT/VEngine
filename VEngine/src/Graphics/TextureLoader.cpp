#include "TextureLoader.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"
#include <gli/texture2d.hpp>
#include <gli/load.hpp>
#include "Utility/Utility.h"

using namespace VEngine::gal;

VEngine::TextureLoader::TextureLoader(gal::GraphicsDevice *graphicsDevice, Buffer *stagingBuffer)
	:m_graphicsDevice(graphicsDevice),
	m_stagingBuffer(stagingBuffer),
	m_cmdListPool(),
	m_cmdList()
{
	m_graphicsDevice->createCommandListPool(m_graphicsDevice->getGraphicsQueue(), &m_cmdListPool);
	m_cmdListPool->allocate(1, &m_cmdList);
}

VEngine::TextureLoader::~TextureLoader()
{
	m_cmdListPool->free(1, &m_cmdList);
	m_graphicsDevice->destroyCommandListPool(m_cmdListPool);
}

void VEngine::TextureLoader::load(const char *filepath, gal::Image *&image, gal::ImageView *&imageView)
{
	// load texture
	gli::texture gliTex(gli::load(filepath));
	{
		if (gliTex.empty())
		{
			Utility::fatalExit(("Failed to load texture: " + std::string(filepath)).c_str(), -1);
		}

		if (gliTex.size() > RendererConsts::STAGING_BUFFER_SIZE)
		{
			Utility::fatalExit("Texture is too large for staging buffer!", EXIT_FAILURE);
		}
	}

	// create image
	{
		ImageCreateInfo imageCreateInfo;
		imageCreateInfo.m_width = static_cast<uint32_t>(gliTex.extent().x);
		imageCreateInfo.m_height = static_cast<uint32_t>(gliTex.extent().y);
		imageCreateInfo.m_depth = static_cast<uint32_t>(gliTex.extent().z);
		imageCreateInfo.m_layers = static_cast<uint32_t>(gliTex.layers());
		imageCreateInfo.m_levels = static_cast<uint32_t>(gliTex.levels());
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = (gliTex.target() == gli::TARGET_2D || gliTex.target() == gli::TARGET_2D_ARRAY) ? ImageType::_2D : ImageType::_3D;
		imageCreateInfo.m_format = static_cast<Format>(gliTex.format());
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::TEXTURE_BIT | ImageUsageFlagBits::TRANSFER_DST_BIT;
		imageCreateInfo.m_optimizedClearValue = {};

		m_graphicsDevice->createImage(imageCreateInfo, (uint32_t)MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &image);

		m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE, image, filepath);
	}

	// copy image data to staging buffer
	std::vector<BufferImageCopy> bufferCopyRegions;
	bufferCopyRegions.reserve(gliTex.levels());
	{
		uint8_t *data;
		m_stagingBuffer->map((void **)&data);

		// keep track of current offset in staging buffer
		size_t currentOffset = 0;

		// if this is a compressed format, how many texels is each block?
		auto blockExtent = gli::block_extent(gliTex.format());

		for (size_t level = 0; level < gliTex.levels(); ++level)
		{
			// blocks in this level; use max to ensure we dont divide e.g. extent = 2 by blockExtent = 4
			auto blockDims = glm::max(gliTex.extent(level), blockExtent) / blockExtent;
			// size of a texel row in bytes in the src data
			size_t rowSize = blockDims.x * gli::block_size(gliTex.format());
			// size of a row in the staging buffer
			size_t rowPitch = Utility::alignUp(rowSize, (size_t)m_graphicsDevice->getBufferCopyRowPitchAlignment());
			assert(rowPitch % gli::block_size(gliTex.format()) == 0);

			// TODO: can we copy all layers and faces in one go?
			for (size_t layer = 0; layer < gliTex.layers(); ++layer)
			{
				for (size_t face = 0; face < gliTex.faces(); ++face)
				{
					// ensure each copy region starts at the proper alignment in the staging buffer
					currentOffset = Utility::alignUp(currentOffset, (size_t)m_graphicsDevice->getBufferCopyOffsetAlignment());
					const uint8_t *srcData = (uint8_t *)gliTex.data(layer, face, level);

					BufferImageCopy bufferCopyRegion{};
					bufferCopyRegion.m_imageMipLevel = static_cast<uint32_t>(level);
					bufferCopyRegion.m_imageBaseLayer = static_cast<uint32_t>(layer);
					bufferCopyRegion.m_imageLayerCount = 1;
					bufferCopyRegion.m_extent.m_width = static_cast<uint32_t>(gliTex.extent(level).x);
					bufferCopyRegion.m_extent.m_height = static_cast<uint32_t>(gliTex.extent(level).y);
					bufferCopyRegion.m_extent.m_depth = static_cast<uint32_t>(gliTex.extent(level).z);
					bufferCopyRegion.m_bufferOffset = currentOffset;
					bufferCopyRegion.m_bufferRowLength = static_cast<uint32_t>(rowPitch / gli::block_size(gliTex.format()) * gli::block_extent(gliTex.format()).x); // this is in pixels
					bufferCopyRegion.m_bufferImageHeight = static_cast<uint32_t>(glm::max(gliTex.extent(level), blockExtent).y);

					bufferCopyRegions.push_back(bufferCopyRegion);

					for (size_t depth = 0; depth < blockDims.z; ++depth)
					{
						for (size_t row = 0; row < blockDims.y; ++row)
						{
							memcpy(data + currentOffset, srcData, rowSize);
							srcData += rowSize;
							currentOffset += rowPitch;
						}
					}
				}
			}
		}

		m_stagingBuffer->unmap();
	}

	ImageSubresourceRange subresourceRange;
	subresourceRange.m_baseMipLevel = 0;
	subresourceRange.m_levelCount = static_cast<uint32_t>(gliTex.levels());
	subresourceRange.m_baseArrayLayer = 0;
	subresourceRange.m_layerCount = static_cast<uint32_t>(gliTex.layers());

	// copy from staging buffer to image
	{
		// copy to image and transition to TEXTURE
		m_cmdListPool->reset();
		m_cmdList->begin();
		{
			// transition from UNDEFINED to TRANSFER_DST
			Barrier b0 = Initializers::imageBarrier(image,
				PipelineStageFlagBits::TOP_OF_PIPE_BIT,
				PipelineStageFlagBits::TRANSFER_BIT,
				ResourceState::UNDEFINED,
				ResourceState::WRITE_IMAGE_TRANSFER,
				subresourceRange);
			m_cmdList->barrier(1, &b0);

			m_cmdList->copyBufferToImage(m_stagingBuffer, image, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

			// transition form TRANSFER_DST to TEXTURE
			Barrier b1 = Initializers::imageBarrier(image,
				PipelineStageFlagBits::TRANSFER_BIT,
				PipelineStageFlagBits::VERTEX_SHADER_BIT | PipelineStageFlagBits::FRAGMENT_SHADER_BIT | PipelineStageFlagBits::COMPUTE_SHADER_BIT,
				ResourceState::WRITE_IMAGE_TRANSFER,
				ResourceState::READ_TEXTURE,
				subresourceRange);
			m_cmdList->barrier(1, &b1);
		}
		m_cmdList->end();
		Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), m_cmdList);
	}

	// create image view
	m_graphicsDevice->createImageView(image, &imageView);

	m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE_VIEW, imageView, filepath);
}