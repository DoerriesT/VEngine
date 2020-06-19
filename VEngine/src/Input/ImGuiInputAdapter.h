#pragma once
#include "IInputListener.h"
#include "Utility/Timer.h"

struct ImGuiContext;

namespace VEngine
{
	class UserInput;
	class Window;

	class ImGuiInputAdapter : public IInputListener
	{
	public:
		explicit ImGuiInputAdapter(ImGuiContext *context, UserInput &input, Window &window);
		void update();
		void resize(uint32_t width, uint32_t height, uint32_t windowWidth, uint32_t windowHeight);
		void onKey(InputKey key, InputAction action) override;
		void onChar(Codepoint charKey) override;
		void onMouseButton(InputMouse mouseButton, InputAction action) override;
		void onMouseMove(double x, double y) override;
		void onMouseScroll(double xOffset, double yOffset) override;

	private:
		ImGuiContext *m_context;
		UserInput &m_input;
		Window &m_window;
		Timer m_timer;
		bool m_mouseJustPressed[8] = {};
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_windowWidth;
		uint32_t m_windowHeight;
	};
}