#include "UserInput.h"

VEngine::UserInput &VEngine::UserInput::getInstance()
{
	static UserInput userInput;
	return userInput;
}

void VEngine::UserInput::input()
{
}

glm::vec2 VEngine::UserInput::getPreviousMousePos()
{
	return m_previousMousePos;
}

glm::vec2 VEngine::UserInput::getCurrentMousePos()
{
	return m_currentMousePos;
}

glm::vec2 VEngine::UserInput::getMousePosDelta()
{
	return m_currentMousePos - m_previousMousePos;
}

glm::vec2 VEngine::UserInput::getScrollOffset()
{
	return m_scrollOffset;
}

bool VEngine::UserInput::isKeyPressed(InputKey key)
{
	return m_pressedKeys[static_cast<size_t>(key)];
}

bool VEngine::UserInput::isMouseButtonPressed(InputMouse mouseButton)
{
	return m_pressedMouseButtons[static_cast<size_t>(mouseButton)];
}

void VEngine::UserInput::addKeyListener(IKeyListener * listener)
{
	m_keyListeners.push_back(listener);
}

void VEngine::UserInput::removeKeyListener(IKeyListener * listener)
{
}

void VEngine::UserInput::addCharListener(ICharListener * listener)
{
}

void VEngine::UserInput::removeCharListener(ICharListener * listener)
{
}

void VEngine::UserInput::addScrollListener(IScrollListener * listener)
{
}

void VEngine::UserInput::removeScrollListener(IScrollListener * listener)
{
}

void VEngine::UserInput::addMouseButtonListener(IMouseButtonListener * listener)
{
}

void VEngine::UserInput::removeMouseButtonListener(IMouseButtonListener * listener)
{
}

void VEngine::UserInput::onKey(InputKey _key, InputAction _action)
{
}

void VEngine::UserInput::onChar(InputKey _charKey)
{
}

void VEngine::UserInput::onMouseButton(InputMouse _mouseButton, InputAction _action)
{
}

void VEngine::UserInput::onScroll(double xOffset, double yOffset)
{
}
