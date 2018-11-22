#pragma once
#include "VKImageData.h"

namespace VEngine
{
	class VKRenderer;

	struct VKRenderResources
	{
		friend VKRenderer;
	public:
		VKImageData m_colorAttachment;
		VKImageData m_depthAttachment;

		VKRenderResources(const VKRenderResources &) = delete;
		VKRenderResources(const VKRenderResources &&) = delete;
		VKRenderResources &operator= (const VKRenderResources &) = delete;
		VKRenderResources &operator= (const VKRenderResources &&) = delete;
		~VKRenderResources();
		void init(unsigned int width, unsigned int height);
		void resize(unsigned int width, unsigned int height);

	private:
		explicit VKRenderResources();
		void createResizableTextures(unsigned int width, unsigned int height);
		void createAllTextures(unsigned int width, unsigned int height);
		void deleteResizableTextures();
		void deleteAllTextures();
	};
}