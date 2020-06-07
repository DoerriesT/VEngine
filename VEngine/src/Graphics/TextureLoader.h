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
		Texture2DHandle loadTexture2D(const char *filepath);
		Texture3DHandle loadTexture3D(const char *filepath);
		void free(Texture2DHandle handle);
		void free(Texture3DHandle handle);
		gal::ImageView **get2DViews();
		gal::ImageView **get3DViews();

	private:

		enum class Type
		{
			_2D, _3D, _2D_ARRAY
		};

		gal::GraphicsDevice *m_graphicsDevice;
		uint32_t *m_free2DHandles;
		uint32_t *m_free3DHandles;
		uint32_t m_free2DHandleCount;
		uint32_t m_free3DHandleCount;
		gal::Buffer *m_stagingBuffer;
		gal::CommandListPool *m_cmdListPool;
		gal::CommandList *m_cmdList;
		gal::ImageView *m_dummy2DView;
		gal::ImageView *m_dummy3DView;
		gal::Image *m_dummy2DImage;
		gal::Image *m_dummy3DImage;
		gal::ImageView *m_2DViews[RendererConsts::TEXTURE_ARRAY_SIZE];
		gal::ImageView *m_3DViews[RendererConsts::TEXTURE_ARRAY_SIZE];
		gal::Image *m_2DImages[RendererConsts::TEXTURE_ARRAY_SIZE];
		gal::Image *m_3DImages[RendererConsts::TEXTURE_ARRAY_SIZE];

		uint32_t load(const char *filepath, Type type);
	};
}