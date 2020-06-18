#pragma once
#include "Handles.h"
#include "gal/FwdDecl.h"
#include "RendererConsts.h"

namespace VEngine
{
	class TextureManager
	{
	public:
		explicit TextureManager(gal::GraphicsDevice *graphicsDevice);
		TextureManager(const TextureManager &) = delete;
		TextureManager(const TextureManager &&) = delete;
		TextureManager &operator= (const TextureManager &) = delete;
		TextureManager &operator= (const TextureManager &&) = delete;
		~TextureManager();
		// pass nullptr for image if resource is externally owned
		Texture2DHandle addTexture2D(gal::Image *image, gal::ImageView *imageView);
		// pass nullptr for image if resource is externally owned
		Texture3DHandle addTexture3D(gal::Image *image, gal::ImageView *imageView);
		void free(Texture2DHandle handle);
		void free(Texture3DHandle handle);
		void update(Texture2DHandle handle, gal::Image *image, gal::ImageView *imageView);
		void update(Texture3DHandle handle, gal::Image *image, gal::ImageView *imageView);
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
		gal::ImageView *m_dummy2DView;
		gal::ImageView *m_dummy3DView;
		gal::Image *m_dummy2DImage;
		gal::Image *m_dummy3DImage;
		gal::ImageView *m_2DViews[RendererConsts::TEXTURE_ARRAY_SIZE];
		gal::ImageView *m_3DViews[RendererConsts::TEXTURE_ARRAY_SIZE];
		gal::Image *m_2DImages[RendererConsts::TEXTURE_ARRAY_SIZE];
		gal::Image *m_3DImages[RendererConsts::TEXTURE_ARRAY_SIZE];

		uint32_t findHandle(Type type);
	};
}