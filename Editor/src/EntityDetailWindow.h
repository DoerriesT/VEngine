#pragma once
#include <entt/entity/registry.hpp>

namespace VEngine
{
	class Engine;
}

struct ImGuiContext;

namespace VEditor
{
	class EntityDetailWindow
	{
	public:
		explicit EntityDetailWindow(VEngine::Engine *engine);
		void draw(entt::entity entity, entt::entity editorCameraEntity, ImGuiContext *gameContext);
		void setVisible(bool visible);
		bool isVisible() const;

	private:
		VEngine::Engine *m_engine;
		entt::entity m_lastDisplayedEntity;
		int m_translateRotateScaleMode;
		bool m_localTransformMode;
		bool m_visible;
	};
}