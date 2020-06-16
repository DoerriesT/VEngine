#include <cstdlib>
#include "App.h"
#include <Engine.h>
#include <VEditor.h>

int main()
{
	App logic;
	VEditor::VEditor editor(logic);
	VEngine::Engine engine("Vulkan", editor);
	engine.start();

	return EXIT_SUCCESS;
}