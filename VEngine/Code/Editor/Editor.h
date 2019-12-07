#pragma once
#include <cstdint>
#include <memory>

namespace VEngine
{
	class Engine;
	class EntityDetailsWindow;

	class Editor
	{
	public:
		explicit Editor(Engine *engine);
		~Editor();
		void update();
		uint32_t getEditorCameraEntity() const;

	private:
		Engine *m_engine;
		std::unique_ptr<EntityDetailsWindow> m_detailsWindow;
		uint32_t m_editorCameraEntity;
		bool m_showProfilerWindow = false;
		bool m_showMemoryWindow = false;
		bool m_showLuminanceHistogramWindow = false;
		bool m_showEntityWindow = true;
		bool m_showEntityDetailWindow = true;
	};
}