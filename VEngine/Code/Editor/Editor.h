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
		explicit Editor(Engine *engine, uint32_t editorWindowWidth, uint32_t editorWindowHeight);
		~Editor();
		void update(float timeDelta);
		uint32_t getEditorCameraEntity() const;
		void resize(uint32_t editorWindowWidth, uint32_t editorWindowHeight);

	private:
		Engine *m_engine;
		std::unique_ptr<EntityDetailsWindow> m_detailsWindow;
		std::unique_ptr<float[]> m_frametimes;
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_editorCameraEntity;
		bool m_showProfilerWindow = false;
		bool m_showMemoryWindow = false;
		bool m_showLuminanceHistogramWindow = false;
		bool m_showEntityWindow = false;
		bool m_showEntityDetailWindow = false;
	};
}