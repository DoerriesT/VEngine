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
		void load(const char *filepath, gal::Image *&image, gal::ImageView *&imageView);

	private:
		gal::GraphicsDevice *m_graphicsDevice;
		gal::Buffer *m_stagingBuffer;
		gal::CommandListPool *m_cmdListPool;
		gal::CommandList *m_cmdList;
	};
}