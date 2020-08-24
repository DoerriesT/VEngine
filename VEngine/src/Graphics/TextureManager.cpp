#include "TextureManager.h"
#include "Utility/Utility.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"

using namespace VEngine::gal;

VEngine::TextureManager::TextureManager(gal::GraphicsDevice *graphicsDevice)
	:m_graphicsDevice(graphicsDevice),
	m_free2DHandles(new uint32_t[RendererConsts::TEXTURE_ARRAY_SIZE]),
	m_free3DHandles(new uint32_t[RendererConsts::TEXTURE_ARRAY_SIZE]),
	m_free2DHandleCount(RendererConsts::TEXTURE_ARRAY_SIZE),
	m_free3DHandleCount(RendererConsts::TEXTURE_ARRAY_SIZE),
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

	// create temporary pool + list
	gal::CommandListPool *cmdListPool;
	gal::CommandList *cmdList;
	m_graphicsDevice->createCommandListPool(m_graphicsDevice->getGraphicsQueue(), &cmdListPool);
	cmdListPool->allocate(1, &cmdList);

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
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::TEXTURE_BIT;

		m_graphicsDevice->createImage(imageCreateInfo, (uint32_t)MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_dummy2DImage);
		m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE, m_dummy2DImage, "Dummy Image");

		imageCreateInfo.m_imageType = ImageType::_3D;

		m_graphicsDevice->createImage(imageCreateInfo, (uint32_t)MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_dummy3DImage);
		m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE, m_dummy3DImage, "Dummy Image");

		// transition from UNDEFINED to TEXTURE
		cmdListPool->reset();
		cmdList->begin();
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
			cmdList->barrier(2, barriers);
		}
		cmdList->end();
		Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), cmdList);

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

	cmdListPool->free(1, &cmdList);
	m_graphicsDevice->destroyCommandListPool(cmdListPool);
}

VEngine::TextureManager::~TextureManager()
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

	delete[] m_free2DHandles;
	delete[] m_free3DHandles;
}

VEngine::Texture2DHandle VEngine::TextureManager::addTexture2D(gal::Image *image, gal::ImageView *imageView)
{
	uint32_t handle = findHandle(Type::_2D);

	m_2DImages[handle - 1] = image;
	m_2DViews[handle - 1] = imageView;

	return { handle };
}

VEngine::Texture3DHandle VEngine::TextureManager::addTexture3D(gal::Image *image, gal::ImageView *imageView)
{
	uint32_t handle = findHandle(Type::_3D);

	m_3DImages[handle - 1] = image;
	m_3DViews[handle - 1] = imageView;

	return { handle };
}

void VEngine::TextureManager::free(Texture2DHandle handle)
{
	// free handle
	assert(handle.m_handle);
	assert(m_free2DHandleCount < RendererConsts::TEXTURE_ARRAY_SIZE);
	m_free2DHandles[m_free2DHandleCount] = handle.m_handle;
	++m_free2DHandleCount;

	auto &image = m_2DImages[handle.m_handle - 1];
	auto &view = m_2DViews[handle.m_handle - 1];

	// destroy owned resources
	if (image)
	{
		m_graphicsDevice->destroyImageView(view);
		m_graphicsDevice->destroyImage(image);
	}

	// clear texture
	view = m_2DViews[0];
	image = nullptr;
}

void VEngine::TextureManager::free(Texture3DHandle handle)
{
	// free handle
	assert(handle.m_handle);
	assert(m_free3DHandleCount < RendererConsts::TEXTURE_ARRAY_SIZE);
	m_free3DHandles[m_free3DHandleCount] = handle.m_handle;
	++m_free3DHandleCount;

	auto &image = m_3DImages[handle.m_handle - 1];
	auto &view = m_3DViews[handle.m_handle - 1];

	// destroy resources
	if (image)
	{
		m_graphicsDevice->destroyImageView(view);
		m_graphicsDevice->destroyImage(image);
	}

	// clear texture
	view = m_3DViews[0];
	image = nullptr;
}

void VEngine::TextureManager::update(Texture2DHandle handle, gal::Image *image, gal::ImageView *imageView)
{
	// destroy resources
	if (m_2DImages[handle.m_handle - 1])
	{
		m_graphicsDevice->destroyImageView(m_2DViews[handle.m_handle - 1]);
		m_graphicsDevice->destroyImage(m_2DImages[handle.m_handle - 1]);
	}

	m_2DImages[handle.m_handle - 1] = image;
	m_2DViews[handle.m_handle - 1] = imageView;
}

void VEngine::TextureManager::update(Texture3DHandle handle, gal::Image *image, gal::ImageView *imageView)
{
	// destroy resources
	if (m_3DImages[handle.m_handle - 1])
	{
		m_graphicsDevice->destroyImageView(m_3DViews[handle.m_handle - 1]);
		m_graphicsDevice->destroyImage(m_3DImages[handle.m_handle - 1]);
	}

	m_3DImages[handle.m_handle - 1] = image;
	m_3DViews[handle.m_handle - 1] = imageView;
}

VEngine::gal::ImageView **VEngine::TextureManager::get2DViews()
{
	return m_2DViews;
}

VEngine::gal::ImageView **VEngine::TextureManager::get3DViews()
{
	return m_3DViews;
}

uint32_t VEngine::TextureManager::findHandle(Type type)
{
	uint32_t handle = 0;
	{
		uint32_t &freeHandleCount = (type == Type::_2D) ? m_free2DHandleCount : m_free3DHandleCount;
		auto &freeHandles = (type == Type::_2D) ? m_free2DHandles : m_free3DHandles;
		if (!freeHandleCount)
		{
			Utility::fatalExit("Out of Texture Handles!", EXIT_FAILURE);
		}

		--freeHandleCount;
		handle = freeHandles[freeHandleCount];
	}

	return handle;
}
