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
#include <ECS/Component/PointLightComponent.h>
#include <iostream>
#include <GlobalVar.h>
#include <Scene.h>
#include <random>
#include <Input/UserInput.h>
#include <GlobalVar.h>

class DummyLogic : public VEngine::IGameLogic
{
public:
	void init();
	void input(double time, double timeDelta);
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

	std::default_random_engine e;
	std::uniform_real_distribution<float> px(-14.0f, 14.0f);
	std::uniform_real_distribution<float> py(0.0f, 10.0f);
	std::uniform_real_distribution<float> pz(-8.0f, 8.0f);
	std::uniform_real_distribution<float> c(0.0f, 1.0f);

	for (size_t i = 0; i < 2048; ++i)
	{
		const VEngine::Entity *lightEntity = em.createEntity();
		em.addComponent<VEngine::TransformationComponent>(lightEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(px(e), py(e), pz(e)));
		em.addComponent<VEngine::PointLightComponent>(lightEntity, glm::vec3(c(e), c(e), c(e)), 100.0f, 1.0f);
		em.addComponent<VEngine::RenderableComponent>(lightEntity);
	}
	
	//{
	//	const VEngine::Entity *lightEntity0 = em.createEntity();
	//	em.addComponent<VEngine::TransformationComponent>(lightEntity0, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(-5.0f, 1.0f, 0.0f));
	//	em.addComponent<VEngine::PointLightComponent>(lightEntity0, glm::vec3(1.0f, 0.0f, 0.0f), 1000.0f, 2.0f);
	//	em.addComponent<VEngine::RenderableComponent>(lightEntity0);
	//}
	//
	//{
	//	const VEngine::Entity *lightEntity1 = em.createEntity();
	//	em.addComponent<VEngine::TransformationComponent>(lightEntity1, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(5.0f, 1.0f, 0.0f));
	//	em.addComponent<VEngine::PointLightComponent>(lightEntity1, glm::vec3(0.0f, 1.0f, 0.0f), 1000.0f, 2.0f);
	//	em.addComponent<VEngine::RenderableComponent>(lightEntity1);
	//}
	
}

void DummyLogic::input(double time, double timeDelta)
{
	auto &input = e->getUserInput();
	VEngine::g_TAAEnabled = input.isKeyPressed(InputKey::SPACE);
}
