#include "UserInput.h"
#include "Utility/ContainerUtility.h"

VEngine::UserInput::UserInput()
{
}

void VEngine::UserInput::input()
{
	m_mousePosDelta = (m_mousePos - m_previousMousePos);
	m_previousMousePos = m_mousePos;
}

glm::vec2 VEngine::UserInput::getPreviousMousePos()
{
	return m_previousMousePos;
}

glm::vec2 VEngine::UserInput::getCurrentMousePos()
{
	return m_mousePos;
}

glm::vec2 VEngine::UserInput::getMousePosDelta()
{
	return m_mousePosDelta;
}

glm::vec2 VEngine::UserInput::getScrollOffset()
{
	return m_scrollOffset;
}

bool VEngine::UserInput::isKeyPressed(InputKey key, bool ignoreRepeated)
{
	size_t pos = static_cast<size_t>(key);
	return m_pressedKeys[pos] && (!ignoreRepeated || !m_repeatedKeys[pos]);
}

bool VEngine::UserInput::isMouseButtonPressed(InputMouse mouseButton)
{
	return m_pressedMouseButtons[static_cast<size_t>(mouseButton)];
}

void VEngine::UserInput::addKeyListener(IKeyListener *listener)
{
	m_keyListeners.push_back(listener);
}

void VEngine::UserInput::removeKeyListener(IKeyListener *listener)
{
	ContainerUtility::remove(m_keyListeners, listener);
}

void VEngine::UserInput::addCharListener(ICharListener *listener)
{
	m_charListeners.push_back(listener);
}

void VEngine::UserInput::removeCharListener(ICharListener *listener)
{
	ContainerUtility::remove(m_charListeners, listener);
}

void VEngine::UserInput::addScrollListener(IScrollListener *listener)
{
	m_scrollListeners.push_back(listener);
}

void VEngine::UserInput::removeScrollListener(IScrollListener *listener)
{
	ContainerUtility::remove(m_scrollListeners, listener);
}

void VEngine::UserInput::addMouseButtonListener(IMouseButtonListener *listener)
{
	m_mouseButtonlisteners.push_back(listener);
}

void VEngine::UserInput::removeMouseButtonListener(IMouseButtonListener *listener)
{
	ContainerUtility::remove(m_mouseButtonlisteners, listener);
}

void VEngine::UserInput::onKey(InputKey key, InputAction action)
{
	for (IKeyListener *listener : m_keyListeners)
	{
		listener->onKey(key, action);
	}

	switch (action)
	{
	case InputAction::RELEASE:
		m_pressedKeys.set(static_cast<size_t>(key), false);
		m_repeatedKeys.set(static_cast<size_t>(key), false);
		break;
	case InputAction::PRESS:
		m_pressedKeys.set(static_cast<size_t>(key), true);
		break;
	case InputAction::REPEAT:
		m_repeatedKeys.set(static_cast<size_t>(key), true);
		break;
	default:
		break;
	}
}

void VEngine::UserInput::onChar(Codepoint charKey)
{
	for (ICharListener *listener : m_charListeners)
	{
		listener->onChar(charKey);
	}
}

void VEngine::UserInput::onMouseButton(InputMouse mouseButton, InputAction action)
{
	for (IMouseButtonListener *listener : m_mouseButtonlisteners)
	{
		listener->onMouseButton(mouseButton, action);
	}

	if (action == InputAction::RELEASE)
	{
		m_pressedMouseButtons.set(static_cast<size_t>(mouseButton), false);
	}
	else if (action == InputAction::PRESS)
	{
		m_pressedMouseButtons.set(static_cast<size_t>(mouseButton), true);
	}
}

void VEngine::UserInput::onMouseMove(double x, double y)
{
	m_mousePos.x = static_cast<float>(x);
	m_mousePos.y = static_cast<float>(y);
}

void VEngine::UserInput::onMouseScroll(double xOffset, double yOffset)
{
	for (IScrollListener *listener : m_scrollListeners)
	{
		listener->onMouseScroll(xOffset, yOffset);
	}

	m_scrollOffset.x = static_cast<float>(xOffset);
	m_scrollOffset.y = static_cast<float>(yOffset);
}
