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
#include <Components/DirectionalLightComponent.h>
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
		entityRegistry.assign<VEngine::TransformationComponent>(cameraEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(-12.0f, 1.8f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(90.0f), 0.0f)));
		entityRegistry.assign<VEngine::CameraComponent>(cameraEntity, VEngine::CameraComponent::ControllerType::FPS, VEngine::g_windowWidth / (float)VEngine::g_windowHeight, glm::radians(60.0f), 0.1f, 300.0f);
		m_engine->getRenderSystem().setCameraEntity(cameraEntity);

		VEngine::Scene scene = {};
		scene.load(m_engine->getRenderSystem(), "Resources/Models/bistro_e");
		entt::entity exteriorEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(exteriorEntity, VEngine::TransformationComponent::Mobility::STATIC);
		entityRegistry.assign<VEngine::MeshComponent>(exteriorEntity, scene.m_meshInstances["Resources/Models/bistro_e"]);
		entityRegistry.assign<VEngine::RenderableComponent>(exteriorEntity);
		
		scene.load(m_engine->getRenderSystem(), "Resources/Models/bistro_i");
		entt::entity interiorEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(interiorEntity, VEngine::TransformationComponent::Mobility::STATIC);
		entityRegistry.assign<VEngine::MeshComponent>(interiorEntity, scene.m_meshInstances["Resources/Models/bistro_i"]);
		entityRegistry.assign<VEngine::RenderableComponent>(interiorEntity);

		//scene.load(m_engine->getRenderSystem(), "Resources/Models/mori_knob");
		//entt::entity knobEntity = entityRegistry.create();
		//entityRegistry.assign<VEngine::TransformationComponent>(knobEntity, VEngine::TransformationComponent::Mobility::STATIC);
		//entityRegistry.assign<VEngine::MeshComponent>(knobEntity, scene.m_meshInstances["Resources/Models/mori_knob"]);
		//entityRegistry.assign<VEngine::RenderableComponent>(knobEntity);

		entt::entity sunLightEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::DirectionalLightComponent>(sunLightEntity, glm::vec3(100.0f), glm::vec3(0.1f, 3.0f, -1.0f), true);
		entityRegistry.assign<VEngine::RenderableComponent>(sunLightEntity);

		std::default_random_engine e;
		std::uniform_real_distribution<float> px(-14.0f, 14.0f);
		std::uniform_real_distribution<float> py(0.0f, 10.0f);
		std::uniform_real_distribution<float> pz(-8.0f, 8.0f);
		std::uniform_real_distribution<float> c(0.0f, 1.0f);

		for (size_t i = 0; i < 0; ++i)
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
		VEngine::g_TAAEnabled = input.isKeyPressed(InputKey::T);
		VEngine::g_FXAAEnabled = input.isKeyPressed(InputKey::F);
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