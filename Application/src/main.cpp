#include <cstdlib>
#include "App.h"
#include <Engine.h>

int main()
{
	App logic;
	VEngine::Engine engine("Real-Time Volumetric Lighting", logic);
	engine.start();

	return EXIT_SUCCESS;
}