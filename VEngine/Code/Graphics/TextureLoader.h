#pragma once
#include "Graphics/RendererConsts.h"
#include "Handles.h"
#include "gal/FwdDecl.h"

namespace VEngine
{
	class TextureLoader
	{
	public:
		explicit TextureLoader(gal::GraphicsDevice *graphicsDevice, gal::Buffer *stagingBuffer);
		TextureLoader(const TextureLoader &) = delete;
		TextureLoader(const TextureLoader &&) = delete;
		TextureLoader &operator= (const TextureLoader &) = delete;
		TextureLoader &operator= (const TextureLoader &&) = delete;
		~TextureLoader();
		TextureHandle load(const char *filepath);
		void free(TextureHandle handle);
		gal::ImageView **getViews();

	private:
		gal::GraphicsDevice *m_graphicsDevice;
		TextureHandle *m_freeHandles;
		uint32_t m_freeHandleCount;
		gal::Buffer *m_stagingBuffer;
		gal::CommandListPool *m_cmdListPool;
		gal::CommandList *m_cmdList;
		// first element is dummy image
		gal::ImageView *m_views[RendererConsts::TEXTURE_ARRAY_SIZE + 1];
		gal::Image *m_images[RendererConsts::TEXTURE_ARRAY_SIZE + 1];
	};
}