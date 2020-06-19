#include "UserInput.h"
#include "Utility/ContainerUtility.h"
#include "Window/Window.h"

VEngine::UserInput::UserInput(Window &window)
	:m_window(window)
{
	m_window.addInputListener(this);
}

void VEngine::UserInput::resize(int32_t offsetX, int32_t offsetY, uint32_t width, uint32_t height)
{
	m_offsetX = offsetX;
	m_offsetY = offsetY;
	m_width = width;
	m_height = height;
}

void VEngine::UserInput::input()
{
	m_mousePosDelta = (m_mousePos - m_previousMousePos);
	m_previousMousePos = m_mousePos;
}

glm::vec2 VEngine::UserInput::getPreviousMousePos() const
{
	return m_previousMousePos;
}

glm::vec2 VEngine::UserInput::getCurrentMousePos() const
{
	return m_mousePos;
}

glm::vec2 VEngine::UserInput::getMousePosDelta() const
{
	return m_mousePosDelta;
}

glm::vec2 VEngine::UserInput::getScrollOffset() const
{
	return m_scrollOffset;
}

void VEngine::UserInput::setMousePos(float x, float y)
{
	if (m_width != 0 || m_height != 0)
	{
		x += m_offsetX;
		y += m_offsetY;
	}

	m_window.setCursorPos(x, y);
}

const char *VEngine::UserInput::getClipboardText() const
{
	return m_window.getClipboardText();
}

void VEngine::UserInput::setClipboardText(const char *text)
{
	m_window.setClipboardText(text);
}

bool VEngine::UserInput::isKeyPressed(InputKey key, bool ignoreRepeated) const
{
	size_t pos = static_cast<size_t>(key);
	return pos < m_repeatedKeys.size() && pos < m_repeatedKeys.size() && m_pressedKeys[pos] && (!ignoreRepeated || !m_repeatedKeys[pos]);
}

bool VEngine::UserInput::isMouseButtonPressed(InputMouse mouseButton) const
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

	const auto keyIndex = static_cast<size_t>(key);

	switch (action)
	{
	case InputAction::RELEASE:
		if (keyIndex < m_pressedKeys.size() && keyIndex < m_repeatedKeys.size())
		{
			m_pressedKeys.set(static_cast<size_t>(key), false);
			m_repeatedKeys.set(static_cast<size_t>(key), false);
		}
		break;
	case InputAction::PRESS:
		if (keyIndex < m_pressedKeys.size())
		{
			m_pressedKeys.set(static_cast<size_t>(key), true);
		}
		break;
	case InputAction::REPEAT:
		if (keyIndex < m_repeatedKeys.size())
		{
			m_repeatedKeys.set(static_cast<size_t>(key), true);
		}
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

	if (m_width != 0 || m_height != 0)
	{
		m_mousePos.x -= m_offsetX;
		m_mousePos.y -= m_offsetY;
	}
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
