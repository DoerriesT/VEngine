#include "Window.h"

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

	glfwSetWindowUserPointer(m_windowHandle, this);
}

GLFWwindow *VEngine::Window::getWindowHandle() const
{
	return m_windowHandle;
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
