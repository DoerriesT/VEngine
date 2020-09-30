#include <cstdlib>
#include "App.h"
#include <Engine.h>
#include <VEditor.h>

int main()
{
	App logic;
	//VEditor::VEditor editor(logic);
	VEngine::Engine engine("Real-Time Volumetric Lighting", logic);
	engine.start();

	return EXIT_SUCCESS;
}