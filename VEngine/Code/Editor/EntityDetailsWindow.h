#pragma once
#include <cstdint>

namespace VEngine
{
	class Engine;

	class EntityDetailsWindow
	{
	public:
		explicit EntityDetailsWindow(Engine *engine);
		~EntityDetailsWindow();
		void draw(uint32_t entity, uint32_t editorCameraEntity);

	private:
		Engine *m_engine;
		uint32_t m_lastDisplayedEntity;
		int m_translateRotateScaleMode;
		bool m_localTransformMode;
	};
}