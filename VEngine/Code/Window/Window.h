#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace VEngine
{
	class Window
	{
	public:
		explicit Window(unsigned int width, unsigned int height, const char *title);
		~Window();
		void init();
		GLFWwindow *getWindowHandle() const;
		unsigned int getWidth() const;
		unsigned int getHeight() const;
		bool shouldClose() const;
	private:
		GLFWwindow *m_windowHandle;
		unsigned int m_width;
		unsigned int m_height;
		const char *m_title;
	};
}