#include <cstdlib>
#include <Engine.h>
#include <IGameLogic.h>
#include <ECS/EntityManager.h>
#include <ECS/SystemManager.h>
#include <ECS/System/RenderSystem.h>
#include <ECS/Component/TransformationComponent.h>
#include <ECS/Component/MeshComponent.h>
#include <ECS/Component/RenderableComponent.h>
#include <ECS/Component/CameraComponent.h>
#include <iostream>
#include <GlobalVar.h>
#include <Scene.h>

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
	em.addComponent<VEngine::TransformationComponent>(cameraEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(12.0f, 1.8f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(-90.0f), 0.0f)));
	em.addComponent<VEngine::CameraComponent>(cameraEntity, VEngine::CameraComponent::ControllerType::FPS, VEngine::g_windowWidth / (float)VEngine::g_windowHeight, glm::radians(60.0f), 0.1f, 300.0f);
	e->getSystemManager().getSystem<VEngine::RenderSystem>()->setCameraEntity(cameraEntity);

	VEngine::Scene scene = {};
	scene.load(*e->getSystemManager().getSystem<VEngine::RenderSystem>(), "Resources/Models/sponza.mat");
	const VEngine::Entity *objectEntity = em.createEntity();
	em.addComponent<VEngine::TransformationComponent>(objectEntity, VEngine::TransformationComponent::Mobility::STATIC);
	em.addComponent<VEngine::MeshComponent>(objectEntity, scene.m_meshes["Resources/Models/sponza.mat"]);
	em.addComponent<VEngine::RenderableComponent>(objectEntity);
}
