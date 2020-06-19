#include "Window.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "Utility/Utility.h"
#include "Utility/ContainerUtility.h"
#include "Input/IInputListener.h"

void VEngine::windowSizeCallback(GLFWwindow *window, int width, int height);
void VEngine::framebufferSizeCallback(GLFWwindow *window, int width, int height);
void VEngine::curserPosCallback(GLFWwindow *window, double xPos, double yPos);
void VEngine::curserEnterCallback(GLFWwindow *window, int entered);
void VEngine::scrollCallback(GLFWwindow *window, double xOffset, double yOffset);
void VEngine::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void VEngine::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
void VEngine::charCallback(GLFWwindow *window, unsigned int codepoint);
void VEngine::joystickCallback(int joystickId, int event);

VEngine::Window::Window(unsigned int width, unsigned int height, const std::string &title)
	:m_windowHandle(),
	m_windowWidth(width),
	m_windowHeight(height),
	m_width(width),
	m_height(height),
	m_title(title),
	m_configurationChanged()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_windowHandle = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);

	if (!m_windowHandle)
	{
		glfwTerminate();
		Utility::fatalExit("Failed to create GLFW window", EXIT_FAILURE);
		return;
	}

	glfwSetWindowSizeCallback(m_windowHandle, windowSizeCallback);
	glfwSetFramebufferSizeCallback(m_windowHandle, framebufferSizeCallback);
	glfwSetCursorPosCallback(m_windowHandle, curserPosCallback);
	glfwSetCursorEnterCallback(m_windowHandle, curserEnterCallback);
	glfwSetScrollCallback(m_windowHandle, scrollCallback);
	glfwSetMouseButtonCallback(m_windowHandle, mouseButtonCallback);
	glfwSetKeyCallback(m_windowHandle, keyCallback);
	glfwSetCharCallback(m_windowHandle, charCallback);
	glfwSetJoystickCallback(joystickCallback);

	glfwSetWindowUserPointer(m_windowHandle, this);

	// get actual width/height
	{
		int w = 0;
		int h = 0;

		glfwGetWindowSize(m_windowHandle, &w, &h);
		m_windowWidth = static_cast<uint32_t>(w);
		m_windowHeight = static_cast<uint32_t>(h);

		glfwGetFramebufferSize(m_windowHandle, &w, &h);
		m_width = static_cast<uint32_t>(w);
		m_height = static_cast<uint32_t>(h);
	}

	// create cursors
	m_cursors[(size_t)MouseCursor::ARROW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
	m_cursors[(size_t)MouseCursor::TEXT] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
	m_cursors[(size_t)MouseCursor::RESIZE_ALL] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);   // FIXME: GLFW doesn't have this.
	m_cursors[(size_t)MouseCursor::RESIZE_VERTICAL] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
	m_cursors[(size_t)MouseCursor::RESIZE_HORIZONTAL] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
	m_cursors[(size_t)MouseCursor::RESIZE_TRBL] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);  // FIXME: GLFW doesn't have this.
	m_cursors[(size_t)MouseCursor::RESIZE_TLBR] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);  // FIXME: GLFW doesn't have this.
	m_cursors[(size_t)MouseCursor::HAND] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
	m_cursors[(size_t)MouseCursor::CROSSHAIR] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
}

VEngine::Window::~Window()
{
	for (size_t i = 0; i < (sizeof(m_cursors) / sizeof(m_cursors[0])); ++i)
	{
		glfwDestroyCursor(m_cursors[i]);
	}

	glfwDestroyWindow(m_windowHandle);
	glfwTerminate();
}

void VEngine::Window::pollEvents()
{
	m_configurationChanged = false;
	glfwPollEvents();
}

void *VEngine::Window::getWindowHandle() const
{
	return static_cast<void *>(m_windowHandle);
}

void *VEngine::Window::getWindowHandleRaw() const
{
#ifdef _WIN32
	return glfwGetWin32Window(m_windowHandle);
#else
	return nullptr;
#endif
}

unsigned int VEngine::Window::getWidth() const
{
	return m_width;
}

unsigned int VEngine::Window::getHeight() const
{
	return m_height;
}

unsigned int VEngine::Window::getWindowWidth() const
{
	return m_windowWidth;
}

unsigned int VEngine::Window::getWindowHeight() const
{
	return m_windowHeight;
}

bool VEngine::Window::shouldClose() const
{
	return glfwWindowShouldClose(m_windowHandle);
}

bool VEngine::Window::configurationChanged() const
{
	return m_configurationChanged;
}

void VEngine::Window::setMouseCursorMode(MouseCursorMode mode)
{
	switch (mode)
	{
	case MouseCursorMode::NORMAL:
		glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		break;
	case  MouseCursorMode::HIDDEN:
		glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
		break;
	case  MouseCursorMode::DISABLED:
		glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		break;
	default:
		assert(false);
		break;
	}

	m_mouseCursorMode = mode;
}

VEngine::Window::MouseCursorMode VEngine::Window::getMouseCursorMode() const
{
	return m_mouseCursorMode;
}

void VEngine::Window::setTitle(const std::string &title)
{
	m_title = title;
	glfwSetWindowTitle(m_windowHandle, m_title.c_str());
}

void VEngine::Window::addInputListener(IInputListener *listener)
{
	m_inputListeners.push_back(listener);
}

void VEngine::Window::removeInputListener(IInputListener *listener)
{
	ContainerUtility::remove(m_inputListeners, listener);
}

const char *VEngine::Window::getClipboardText() const
{
	return glfwGetClipboardString(m_windowHandle);
}

void VEngine::Window::setClipboardText(const char *text)
{
	glfwSetClipboardString(m_windowHandle, text);
}

void VEngine::Window::setMouseCursor(MouseCursor cursor)
{
	assert((size_t)cursor < (sizeof(m_cursors) / sizeof(m_cursors[0])));
	glfwSetCursor(m_windowHandle, m_cursors[(size_t)cursor]);
}

void VEngine::Window::setCursorPos(float x, float y)
{
	glfwSetCursorPos(m_windowHandle, x, y);
}

InputAction VEngine::Window::getMouseButton(InputMouse mouseButton)
{
	return (InputAction)glfwGetMouseButton(m_windowHandle, (int)mouseButton);
}

bool VEngine::Window::isFocused()
{
	return glfwGetWindowAttrib(m_windowHandle, GLFW_FOCUSED) != 0;
}

// callback functions

void VEngine::windowSizeCallback(GLFWwindow *window, int width, int height)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	windowFramework->m_windowWidth = width;
	windowFramework->m_windowHeight = height;
	windowFramework->m_configurationChanged = true;
}

void VEngine::framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	windowFramework->m_width = width;
	windowFramework->m_height = height;
	windowFramework->m_configurationChanged = true;
}

void VEngine::curserPosCallback(GLFWwindow *window, double xPos, double yPos)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onMouseMove(xPos, yPos);
	}
}

void VEngine::curserEnterCallback(GLFWwindow *window, int entered)
{
}

void VEngine::scrollCallback(GLFWwindow *window, double xOffset, double yOffset)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onMouseScroll(xOffset, yOffset);
	}
}

void VEngine::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onMouseButton(static_cast<InputMouse>(button), static_cast<InputAction>(action));
	}
}

void VEngine::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onKey(static_cast<InputKey>(key), static_cast<InputAction>(action));
	}
}

void VEngine::charCallback(GLFWwindow *window, unsigned int codepoint)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onChar(codepoint);
	}
}

void VEngine::joystickCallback(int joystickId, int event)
{
	if (event == GLFW_CONNECTED)
	{

	}
	else if (event == GLFW_DISCONNECTED)
	{

	}
}
