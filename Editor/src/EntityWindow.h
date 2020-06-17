#pragma once
#include <entt/entity/registry.hpp>

namespace VEngine
{
	class Engine;
}

namespace VEditor
{
	class EntityWindow
	{
	public:
		explicit EntityWindow(VEngine::Engine *engine);
		void draw();
		void setVisible(bool visible);
		bool isVisible() const;
		entt::entity getSelectedEntity() const;

	private:
		VEngine::Engine *m_engine;
		entt::entity m_selectedEntity;
		bool m_visible;
	};
}