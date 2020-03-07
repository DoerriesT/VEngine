#include "TextureLoader.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"
#include <gli/texture2d.hpp>
#include <gli/load.hpp>
#include "Utility/Utility.h"

using namespace VEngine::gal;

VEngine::TextureLoader::TextureLoader(gal::GraphicsDevice *graphicsDevice, Buffer *stagingBuffer)
	:m_graphicsDevice(graphicsDevice),
	m_freeHandles(new TextureHandle[RendererConsts::TEXTURE_ARRAY_SIZE]),
	m_freeHandleCount(RendererConsts::TEXTURE_ARRAY_SIZE),
	m_stagingBuffer(stagingBuffer),
	m_cmdListPool(),
	m_cmdList(),
	m_views(),
	m_images()
{
	for (TextureHandle i = 0; i < RendererConsts::TEXTURE_ARRAY_SIZE; ++i)
	{
		m_freeHandles[i] = RendererConsts::TEXTURE_ARRAY_SIZE - i;
	}

	m_graphicsDevice->createCommandListPool(m_graphicsDevice->getGraphicsQueue(), &m_cmdListPool);
	m_cmdListPool->allocate(1, &m_cmdList);

	// create dummy texture
	{
		ImageCreateInfo imageCreateInfo;
		imageCreateInfo.m_width = 1;
		imageCreateInfo.m_height = 1;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = Format::R8G8B8A8_UNORM;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = (uint32_t)ImageUsageFlagBits::SAMPLED_BIT;

		m_graphicsDevice->createImage(imageCreateInfo, (uint32_t)MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_images[0]);

		m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE, m_images[0], "Dummy Image");

		// transition from UNDEFINED to TEXTURE
		m_cmdListPool->reset();
		m_cmdList->begin();
		{
			Barrier b = Initializers::imageBarrier(m_images[0],
				PipelineStageFlagBits::TOP_OF_PIPE_BIT,
				PipelineStageFlagBits::VERTEX_SHADER_BIT | PipelineStageFlagBits::FRAGMENT_SHADER_BIT | PipelineStageFlagBits::COMPUTE_SHADER_BIT,
				ResourceState::UNDEFINED,
				ResourceState::READ_TEXTURE);
			m_cmdList->barrier(1, &b);
		}
		m_cmdList->end();
		Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), m_cmdList);

		m_graphicsDevice->createImageView(m_images[0], &m_views[0]);

		m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE_VIEW, m_views[0], "Dummy Image View");
	}
}

VEngine::TextureLoader::~TextureLoader()
{
	// free all remaining textures
	for (TextureHandle i = 0; i < RendererConsts::TEXTURE_ARRAY_SIZE; ++i)
	{
		if (m_views[i + 1] != m_views[0])
		{
			free(i + 1);
		}
	}

	// destroy dummy texture
	m_graphicsDevice->destroyImage(m_images[0]);
	m_graphicsDevice->destroyImageView(m_views[0]);

	m_cmdListPool->free(1, &m_cmdList);
	m_graphicsDevice->destroyCommandListPool(m_cmdListPool);
}

VEngine::TextureHandle VEngine::TextureLoader::load(const char *filepath)
{
	// acquire handle
	TextureHandle handle = 0;
	{
		if (!m_freeHandleCount)
		{
			Utility::fatalExit("Out of Texture Handles!", EXIT_FAILURE);
		}

		--m_freeHandleCount;
		handle = m_freeHandles[m_freeHandleCount];
	}

	// load texture
	gli::texture2d gliTex(gli::load(filepath));
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

	// copy image data to staging buffer
	{
		void *data;
		m_stagingBuffer->map(&data);
		memcpy(data, gliTex.data(), gliTex.size());
		m_stagingBuffer->unmap();
	}

	// create image
	{
		ImageCreateInfo imageCreateInfo;
		imageCreateInfo.m_width = static_cast<uint32_t>(gliTex.extent().x);
		imageCreateInfo.m_height = static_cast<uint32_t>(gliTex.extent().y);
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_levels = static_cast<uint32_t>(gliTex.levels());
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = static_cast<Format>(gliTex.format());
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = (uint32_t)ImageUsageFlagBits::SAMPLED_BIT | (uint32_t)ImageUsageFlagBits::TRANSFER_DST_BIT;

		m_graphicsDevice->createImage(imageCreateInfo, (uint32_t)MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_images[handle]);
	}

	m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE, m_images[handle], filepath);

	ImageSubresourceRange subresourceRange;
	subresourceRange.m_baseMipLevel = 0;
	subresourceRange.m_levelCount = static_cast<uint32_t>(gliTex.levels());
	subresourceRange.m_baseArrayLayer = 0;
	subresourceRange.m_layerCount = 1;

	// copy from staging buffer to image
	{
		std::vector<BufferImageCopy> bufferCopyRegions(gliTex.levels());
		size_t offset = 0;

		for (size_t level = 0; level < gliTex.levels(); ++level)
		{
			BufferImageCopy &bufferCopyRegion = bufferCopyRegions[level];
			bufferCopyRegion.m_imageMipLevel = static_cast<uint32_t>(level);
			bufferCopyRegion.m_imageBaseLayer = 0;
			bufferCopyRegion.m_imageLayerCount = 1;
			bufferCopyRegion.m_extent.m_width = static_cast<uint32_t>(gliTex[level].extent().x);
			bufferCopyRegion.m_extent.m_height = static_cast<uint32_t>(gliTex[level].extent().y);
			bufferCopyRegion.m_extent.m_depth = 1;
			bufferCopyRegion.m_bufferOffset = offset;

			offset += gliTex[level].size();
		}

		// copy to image and transition to TEXTURE
		m_cmdListPool->reset();
		m_cmdList->begin();
		{
			// transition from UNDEFINED to TRANSFER_DST
			Barrier b0 = Initializers::imageBarrier(m_images[handle],
				PipelineStageFlagBits::TOP_OF_PIPE_BIT,
				PipelineStageFlagBits::TRANSFER_BIT,
				ResourceState::UNDEFINED,
				ResourceState::WRITE_IMAGE_TRANSFER,
				subresourceRange);
			m_cmdList->barrier(1, &b0);

			m_cmdList->copyBufferToImage(m_stagingBuffer, m_images[handle], static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());
			
			// transition form TRANSFER_DST to TEXTURE
			Barrier b1 = Initializers::imageBarrier(m_images[handle],
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
	m_graphicsDevice->createImageView(m_images[handle], &m_views[handle]);

	m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE_VIEW, m_views[handle], filepath);

	return handle;
}

void VEngine::TextureLoader::free(TextureHandle handle)
{
	// free handle
	assert(m_freeHandleCount < RendererConsts::TEXTURE_ARRAY_SIZE);
	m_freeHandles[m_freeHandleCount] = handle;
	++m_freeHandleCount;

	// destroy resources
	m_graphicsDevice->destroyImageView(m_views[handle]);
	m_graphicsDevice->destroyImage(m_images[handle]);

	// clear texture
	m_views[handle] = nullptr;
	m_images[handle] = nullptr;
}
