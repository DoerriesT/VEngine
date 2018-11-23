#pragma once
#include "IInputListener.h"
#include <glm/vec2.hpp>
#include <vector>
#include <bitset>

namespace VEngine
{
	class UserInput :public IInputListener
	{
	public:
		explicit UserInput();
		UserInput(const UserInput &) = delete;
		UserInput(const UserInput &&) = delete;
		UserInput &operator= (const UserInput &) = delete;
		UserInput &operator= (const UserInput &&) = delete;
		void input();
		glm::vec2 getPreviousMousePos();
		glm::vec2 getCurrentMousePos();
		glm::vec2 getMousePosDelta();
		glm::vec2 getScrollOffset();
		bool isKeyPressed(InputKey key, bool ignoreRepeated = false);
		bool isMouseButtonPressed(InputMouse mouseButton);
		void addKeyListener(IKeyListener *listener);
		void removeKeyListener(IKeyListener *listener);
		void addCharListener(ICharListener *listener);
		void removeCharListener(ICharListener *listener);
		void addScrollListener(IScrollListener *listener);
		void removeScrollListener(IScrollListener *listener);
		void addMouseButtonListener(IMouseButtonListener *listener);
		void removeMouseButtonListener(IMouseButtonListener *listener);
		void onKey(InputKey key, InputAction action) override;
		void onChar(Codepoint charKey) override;
		void onMouseButton(InputMouse mouseButton, InputAction action) override;
		void onMouseMove(double x, double y) override;
		void onMouseScroll(double xOffset, double yOffset) override;

	private:
		glm::vec2 m_mousePos;
		glm::vec2 m_previousMousePos;
		glm::vec2 m_mousePosDelta;
		glm::vec2 m_scrollOffset;
		std::vector<IKeyListener*> m_keyListeners;
		std::vector<ICharListener*> m_charListeners;
		std::vector<IScrollListener*> m_scrollListeners;
		std::vector<IMouseButtonListener*> m_mouseButtonlisteners;
		std::bitset<350> m_pressedKeys;
		std::bitset<350> m_repeatedKeys;
		std::bitset<8> m_pressedMouseButtons;
	};
}