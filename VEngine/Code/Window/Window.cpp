#include "Window.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Utility/Utility.h"
#include "Utility/ContainerUtility.h"
#include "Input/IInputListener.h"

void VEngine::curserPosCallback(GLFWwindow *window, double xPos, double yPos);
void VEngine::scrollCallback(GLFWwindow *window, double xOffset, double yOffset);
void VEngine::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void VEngine::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
void VEngine::charCallback(GLFWwindow *window, unsigned int codepoint);

VEngine::Window::Window(unsigned int width, unsigned int height, const char * title)
	:m_width(width),
	m_height(height),
	m_title(title)
{
}

VEngine::Window::~Window()
{
	glfwDestroyWindow(m_windowHandle);
	glfwTerminate();
}

void VEngine::Window::init()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_windowHandle = glfwCreateWindow(m_width, m_height, m_title, nullptr, nullptr);

	if (!m_windowHandle)
	{
		glfwTerminate();
		Utility::fatalExit("Failed to create GLFW window", -1);
		return;
	}

	glfwSetCursorPosCallback(m_windowHandle, curserPosCallback);
	glfwSetScrollCallback(m_windowHandle, scrollCallback);
	glfwSetMouseButtonCallback(m_windowHandle, mouseButtonCallback);
	glfwSetKeyCallback(m_windowHandle, keyCallback);
	glfwSetCharCallback(m_windowHandle, charCallback);

	glfwSetWindowUserPointer(m_windowHandle, this);
}
void VEngine::Window::pollEvents() const
{
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

void VEngine::Window::addInputListener(IInputListener *listener)
{
	m_inputListeners.push_back(listener);
}

void VEngine::Window::removeInputListener(IInputListener *listener)
{
	ContainerUtility::remove(m_inputListeners, listener);
}

// callback functions

void VEngine::curserPosCallback(GLFWwindow *window, double xPos, double yPos)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onMouseMove(xPos, yPos);
	}
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