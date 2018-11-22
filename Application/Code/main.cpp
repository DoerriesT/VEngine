#include <cstdlib>
#include <Window/Window.h>
#include <Graphics/Vulkan/VKContext.h>
#include <Graphics/Vulkan/VKRenderer.h>

extern VEngine::VKContext g_context;

int main()
{
	VEngine::Window window(800, 600, "Vulkan");
	window.init();

	
	g_context.init(window.getWindowHandle());
	VEngine::VKRenderer renderer;
	renderer.init(800, 600);

	while (!window.shouldClose())
	{
		renderer.update();
		renderer.render();
		glfwPollEvents();
	}
	return EXIT_SUCCESS;
}