#pragma once
#include <entt/entity/registry.hpp>

namespace VEngine
{
	class Engine;
}

namespace VEditor
{
	class EntityDetailWindow
	{
	public:
		explicit EntityDetailWindow(VEngine::Engine *engine);
		void draw(entt::entity entity, entt::entity editorCameraEntity, float viewportX, float viewportY, float viewportWidth, float viewportHeight);
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