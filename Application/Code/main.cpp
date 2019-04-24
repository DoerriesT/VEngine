#include <cstdlib>
#include <Engine.h>
#include <IGameLogic.h>
#include <entt/entity/registry.hpp>
#include <Graphics/RenderSystem.h>
#include <Components/TransformationComponent.h>
#include <Components/MeshComponent.h>
#include <Components/RenderableComponent.h>
#include <Components/CameraComponent.h>
#include <Components/PointLightComponent.h>
#include <iostream>
#include <GlobalVar.h>
#include <Scene.h>
#include <random>
#include <Input/UserInput.h>
#include <GlobalVar.h>

class DummyLogic : public VEngine::IGameLogic
{
public:
	void initialize(VEngine::Engine *engine) override
	{
		m_engine = engine;

		auto &entityRegistry = m_engine->getEntityRegistry();
		entt::entity cameraEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(cameraEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(12.0f, 1.8f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(-90.0f), 0.0f)));
		entityRegistry.assign<VEngine::CameraComponent>(cameraEntity, VEngine::CameraComponent::ControllerType::FPS, VEngine::g_windowWidth / (float)VEngine::g_windowHeight, glm::radians(60.0f), 0.1f, 300.0f);
		m_engine->getRenderSystem().setCameraEntity(cameraEntity);

		VEngine::Scene scene = {};
		scene.load(m_engine->getRenderSystem(), "Resources/Models/sponza.mat");
		entt::entity objectEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(objectEntity, VEngine::TransformationComponent::Mobility::STATIC);
		entityRegistry.assign<VEngine::MeshComponent>(objectEntity, scene.m_meshes["Resources/Models/sponza.mat"]);
		entityRegistry.assign<VEngine::RenderableComponent>(objectEntity);

		scene.load(m_engine->getRenderSystem(), "Resources/Models/aventador.mat");
		entt::entity carEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(carEntity, VEngine::TransformationComponent::Mobility::DYNAMIC);
		entityRegistry.assign<VEngine::MeshComponent>(carEntity, scene.m_meshes["Resources/Models/aventador.mat"]);
		entityRegistry.assign<VEngine::RenderableComponent>(carEntity);

		std::default_random_engine e;
		std::uniform_real_distribution<float> px(-14.0f, 14.0f);
		std::uniform_real_distribution<float> py(0.0f, 10.0f);
		std::uniform_real_distribution<float> pz(-8.0f, 8.0f);
		std::uniform_real_distribution<float> c(0.0f, 1.0f);

		for (size_t i = 0; i < 2048; ++i)
		{
			entt::entity lightEntity = entityRegistry.create();
			entityRegistry.assign<VEngine::TransformationComponent>(lightEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(px(e), py(e), pz(e)));
			entityRegistry.assign<VEngine::PointLightComponent>(lightEntity, glm::vec3(c(e), c(e), c(e)), 100.0f, 1.0f);
			entityRegistry.assign<VEngine::RenderableComponent>(lightEntity);
		}
	}

	void update(float timeDelta) override 
	{
		auto &input = m_engine->getUserInput();
		VEngine::g_TAAEnabled = input.isKeyPressed(InputKey::SPACE);
	};

	void shutdown() override
	{
	}

private:
	VEngine::Engine *m_engine;
};

int main()
{
	DummyLogic logic;
	VEngine::Engine engine("Vulkan", logic);
	engine.start();

	return EXIT_SUCCESS;
}