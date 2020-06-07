#include "TextureLoader.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"
#include <gli/texture2d.hpp>
#include <gli/load.hpp>
#include "Utility/Utility.h"

using namespace VEngine::gal;

VEngine::TextureLoader::TextureLoader(gal::GraphicsDevice *graphicsDevice, Buffer *stagingBuffer)
	:m_graphicsDevice(graphicsDevice),
	m_free2DHandles(new uint32_t[RendererConsts::TEXTURE_ARRAY_SIZE]),
	m_free3DHandles(new uint32_t[RendererConsts::TEXTURE_ARRAY_SIZE]),
	m_free2DHandleCount(RendererConsts::TEXTURE_ARRAY_SIZE),
	m_free3DHandleCount(RendererConsts::TEXTURE_ARRAY_SIZE),
	m_stagingBuffer(stagingBuffer),
	m_cmdListPool(),
	m_cmdList(),
	m_2DViews(),
	m_3DViews(),
	m_2DImages(),
	m_3DImages()
{
	for (uint32_t i = 0; i < RendererConsts::TEXTURE_ARRAY_SIZE; ++i)
	{
		m_free2DHandles[i] = RendererConsts::TEXTURE_ARRAY_SIZE - i;
		m_free3DHandles[i] = RendererConsts::TEXTURE_ARRAY_SIZE - i;
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

		m_graphicsDevice->createImage(imageCreateInfo, (uint32_t)MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_dummy2DImage);
		m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE, m_dummy2DImage, "Dummy Image");

		imageCreateInfo.m_imageType = ImageType::_3D;

		m_graphicsDevice->createImage(imageCreateInfo, (uint32_t)MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_dummy3DImage);
		m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE, m_dummy3DImage, "Dummy Image");

		// transition from UNDEFINED to TEXTURE
		m_cmdListPool->reset();
		m_cmdList->begin();
		{
			Barrier b2D = Initializers::imageBarrier(m_dummy2DImage,
				PipelineStageFlagBits::TOP_OF_PIPE_BIT,
				PipelineStageFlagBits::VERTEX_SHADER_BIT | PipelineStageFlagBits::FRAGMENT_SHADER_BIT | PipelineStageFlagBits::COMPUTE_SHADER_BIT,
				ResourceState::UNDEFINED,
				ResourceState::READ_TEXTURE);
			Barrier b3D = Initializers::imageBarrier(m_dummy3DImage,
				PipelineStageFlagBits::TOP_OF_PIPE_BIT,
				PipelineStageFlagBits::VERTEX_SHADER_BIT | PipelineStageFlagBits::FRAGMENT_SHADER_BIT | PipelineStageFlagBits::COMPUTE_SHADER_BIT,
				ResourceState::UNDEFINED,
				ResourceState::READ_TEXTURE);
			Barrier barriers[]{ b2D, b3D };
			m_cmdList->barrier(2, barriers);
		}
		m_cmdList->end();
		Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), m_cmdList);

		m_graphicsDevice->createImageView(m_dummy2DImage, &m_dummy2DView);
		m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE_VIEW, m_dummy2DView, "Dummy Image View");

		m_graphicsDevice->createImageView(m_dummy3DImage, &m_dummy3DView);
		m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE_VIEW, m_dummy2DView, "Dummy Image View");

		for (size_t i = 0; i < RendererConsts::TEXTURE_ARRAY_SIZE; ++i)
		{
			m_2DViews[i] = m_dummy2DView;
			m_3DViews[i] = m_dummy3DView;
		}
	}
}

VEngine::TextureLoader::~TextureLoader()
{
	// free all remaining textures
	for (uint32_t i = 0; i < RendererConsts::TEXTURE_ARRAY_SIZE; ++i)
	{
		if (m_2DViews[i] != m_dummy2DView)
		{
			free(Texture2DHandle{ i + 1 });
		}
		if (m_3DViews[i] != m_dummy3DView)
		{
			free(Texture3DHandle{ i + 1 });
		}
	}

	// destroy dummy texture
	m_graphicsDevice->destroyImage(m_dummy2DImage);
	m_graphicsDevice->destroyImage(m_dummy3DImage);
	m_graphicsDevice->destroyImageView(m_dummy2DView);
	m_graphicsDevice->destroyImageView(m_dummy3DView);

	m_cmdListPool->free(1, &m_cmdList);
	m_graphicsDevice->destroyCommandListPool(m_cmdListPool);

	delete[] m_free2DHandles;
	delete[] m_free3DHandles;
}

VEngine::Texture2DHandle VEngine::TextureLoader::loadTexture2D(const char *filepath)
{
	return { load(filepath, Type::_2D) };
}

VEngine::Texture3DHandle VEngine::TextureLoader::loadTexture3D(const char *filepath)
{
	return { load(filepath, Type::_3D) };
}

void VEngine::TextureLoader::free(Texture2DHandle handle)
{
	// free handle
	assert(handle.m_handle);
	assert(m_free2DHandleCount < RendererConsts::TEXTURE_ARRAY_SIZE);
	m_free2DHandles[m_free2DHandleCount] = handle.m_handle;
	++m_free2DHandleCount;

	auto &image = m_2DImages[handle.m_handle - 1];
	auto &view = m_2DViews[handle.m_handle - 1];

	// destroy resources
	m_graphicsDevice->destroyImageView(view);
	m_graphicsDevice->destroyImage(image);

	// clear texture
	view = m_2DViews[0];
	image = nullptr;
}

void VEngine::TextureLoader::free(Texture3DHandle handle)
{
	// free handle
	assert(handle.m_handle);
	assert(m_free3DHandleCount < RendererConsts::TEXTURE_ARRAY_SIZE);
	m_free3DHandles[m_free3DHandleCount] = handle.m_handle;
	++m_free3DHandleCount;

	auto &image = m_3DImages[handle.m_handle - 1];
	auto &view = m_3DViews[handle.m_handle - 1];

	// destroy resources
	m_graphicsDevice->destroyImageView(view);
	m_graphicsDevice->destroyImage(image);

	// clear texture
	view = m_3DViews[0];
	image = nullptr;
}

VEngine::gal::ImageView **VEngine::TextureLoader::get2DViews()
{
	return m_2DViews;
}

VEngine::gal::ImageView **VEngine::TextureLoader::get3DViews()
{
	return m_3DViews;
}

uint32_t VEngine::TextureLoader::load(const char *filepath, Type type)
{
	// acquire handle
	uint32_t handle = 0;
	{
		uint32_t &freeHandleCount = type == Type::_2D ? m_free2DHandleCount : m_free3DHandleCount;
		auto &freeHandles = type == Type::_2D ? m_free2DHandles : m_free3DHandles;
		if (!freeHandleCount)
		{
			Utility::fatalExit("Out of Texture Handles!", EXIT_FAILURE);
		}

		--freeHandleCount;
		handle = freeHandles[freeHandleCount];
	}

	auto &image = type == Type::_2D ? m_2DImages[handle - 1] : m_3DImages[handle - 1];
	auto &view = type == Type::_2D ? m_2DViews[handle - 1] : m_3DViews[handle - 1];

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
		imageCreateInfo.m_depth = static_cast<uint32_t>(gliTex.extent().z);
		imageCreateInfo.m_layers = static_cast<uint32_t>(gliTex.layers());
		imageCreateInfo.m_levels = static_cast<uint32_t>(gliTex.levels());
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = type == Type::_2D ? ImageType::_2D : ImageType::_3D;
		imageCreateInfo.m_format = static_cast<Format>(gliTex.format());
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = (uint32_t)ImageUsageFlagBits::SAMPLED_BIT | (uint32_t)ImageUsageFlagBits::TRANSFER_DST_BIT;

		m_graphicsDevice->createImage(imageCreateInfo, (uint32_t)MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &image);
	}

	m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE, image, filepath);

	ImageSubresourceRange subresourceRange;
	subresourceRange.m_baseMipLevel = 0;
	subresourceRange.m_levelCount = static_cast<uint32_t>(gliTex.levels());
	subresourceRange.m_baseArrayLayer = 0;
	subresourceRange.m_layerCount = static_cast<uint32_t>(gliTex.layers());

	// copy from staging buffer to image
	{
		std::vector<BufferImageCopy> bufferCopyRegions(gliTex.levels() * gliTex.layers());
		size_t offset = 0;

		for (size_t layer = 0; layer < gliTex.layers(); ++layer)
		{
			for (size_t level = 0; level < gliTex.levels(); ++level)
			{
				BufferImageCopy &bufferCopyRegion = bufferCopyRegions[level];
				bufferCopyRegion.m_imageMipLevel = static_cast<uint32_t>(level);
				bufferCopyRegion.m_imageBaseLayer = static_cast<uint32_t>(layer);
				bufferCopyRegion.m_imageLayerCount = 1;
				bufferCopyRegion.m_extent.m_width = static_cast<uint32_t>(gliTex.extent(level).x);
				bufferCopyRegion.m_extent.m_height = static_cast<uint32_t>(gliTex.extent(level).y);
				bufferCopyRegion.m_extent.m_depth = static_cast<uint32_t>(gliTex.extent(level).z);
				bufferCopyRegion.m_bufferOffset = offset;

				offset += gliTex.size(level);
			}
		}

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
	m_graphicsDevice->createImageView(image, &view);

	m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE_VIEW, view, filepath);

	return handle;
}