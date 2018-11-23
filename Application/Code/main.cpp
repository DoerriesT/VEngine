#include <cstdlib>
#include <Engine.h>
#include <IGameLogic.h>

class DummyLogic : public VEngine::IGameLogic
{
public:
	void init() {};
	void input(double time, double timeDelta) {};
	void update(double time, double timeDelta) {};
	void render() {};
};

int main()
{
	DummyLogic logic;
	VEngine::Engine engine("Vulkan", logic);
	engine.start();
	
	return EXIT_SUCCESS;
}