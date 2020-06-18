#pragma once
#include <entt/entity/registry.hpp>
#include <Handles.h>

namespace VEngine
{
	class Engine;
}

namespace VEditor
{
	class AssetBrowserWindow
	{
	public:
		explicit AssetBrowserWindow(VEngine::Engine *engine);
		void draw();
		void setVisible(bool visible);
		bool isVisible() const;

	private:
		VEngine::Engine *m_engine;
		VEngine::Texture2DHandle m_selectedTexture2D;
		const char *m_selectedTexture2DName;
		bool m_visible;
	};
}