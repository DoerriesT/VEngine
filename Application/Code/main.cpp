#include <cstdlib>
#include <Engine.h>
#include <IGameLogic.h>
#include <ECS/EntityManager.h>
#include <ECS/SystemManager.h>
#include <ECS/System/RenderSystem.h>
#include <ECS/Component/TransformationComponent.h>
#include <ECS/Component/CameraComponent.h>
#include <iostream>
#include <GlobalVar.h>

namespace VEngine
{
	extern GlobalVar<unsigned int> g_windowWidth;
	extern GlobalVar<unsigned int> g_windowHeight;
}

class DummyLogic : public VEngine::IGameLogic
{
public:
	void init();
	void input(double time, double timeDelta) {};
	void update(double time, double timeDelta) {};
	void render() {};
};

DummyLogic logic;
VEngine::Engine *e; 

int main()
{
	VEngine::Engine engine("Vulkan", logic);
	e = &engine;
	engine.start();

	return EXIT_SUCCESS;
}

void DummyLogic::init()
{
	VEngine::EntityManager &em = e->getEntityManager();
	const VEngine::Entity *cameraEntity = em.createEntity();
	em.addComponent<VEngine::TransformationComponent>(cameraEntity, VEngine::TransformationComponent::Mobility::DYNAMIC);
	em.addComponent<VEngine::CameraComponent>(cameraEntity, VEngine::g_windowWidth / (float)VEngine::g_windowHeight, glm::radians(60.0f), 0.1f, 300.0f);
	e->getSystemManager().getSystem<VEngine::RenderSystem>()->setCameraEntity(cameraEntity);
}
