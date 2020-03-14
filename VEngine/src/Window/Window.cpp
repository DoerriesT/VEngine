#include "Window.h"
#include <GLFW/glfw3.h>
#include "Utility/Utility.h"
#include "Utility/ContainerUtility.h"
#include "Input/IInputListener.h"

void VEngine::windowSizeCallback(GLFWwindow *window, int width, int height);
void VEngine::curserPosCallback(GLFWwindow *window, double xPos, double yPos);
void VEngine::curserEnterCallback(GLFWwindow *window, int entered);
void VEngine::scrollCallback(GLFWwindow *window, double xOffset, double yOffset);
void VEngine::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void VEngine::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
void VEngine::charCallback(GLFWwindow *window, unsigned int codepoint);
void VEngine::joystickCallback(int joystickId, int event);

VEngine::Window::Window(unsigned int width, unsigned int height, const std::string &title)
	:m_windowHandle(),
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

	glfwSetFramebufferSizeCallback(m_windowHandle, windowSizeCallback);
	glfwSetCursorPosCallback(m_windowHandle, curserPosCallback);
	glfwSetCursorEnterCallback(m_windowHandle, curserEnterCallback);
	glfwSetScrollCallback(m_windowHandle, scrollCallback);
	glfwSetMouseButtonCallback(m_windowHandle, mouseButtonCallback);
	glfwSetKeyCallback(m_windowHandle, keyCallback);
	glfwSetCharCallback(m_windowHandle, charCallback);
	glfwSetJoystickCallback(joystickCallback);

	glfwSetWindowUserPointer(m_windowHandle, this);
}

VEngine::Window::~Window()
{
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

unsigned int VEngine::Window::getWidth() const
{
	return m_width;
}

unsigned int VEngine::Window::getHeight() const
{
	return m_height;
}

bool VEngine::Window::shouldClose() const
{
	return glfwWindowShouldClose(m_windowHandle);
}

bool VEngine::Window::configurationChanged() const
{
	return m_configurationChanged;
}

void VEngine::Window::grabMouse(bool grabMouse)
{
	if (grabMouse)
	{
		glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	else
	{
		glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
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

// callback functions

void VEngine::windowSizeCallback(GLFWwindow *window, int width, int height)
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
