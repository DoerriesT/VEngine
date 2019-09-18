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
#include <Graphics/imgui/ImGuizmo.h>
#include <Graphics/Camera/Camera.h>
#include <glm/ext.hpp>
#include <Graphics/imgui/imgui.h>

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
		scene.load(m_engine->getRenderSystem(), "Resources/Models/sponza");
		entt::entity sponzaEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(sponzaEntity, VEngine::TransformationComponent::Mobility::STATIC);
		entityRegistry.assign<VEngine::MeshComponent>(sponzaEntity, scene.m_meshInstances["Resources/Models/sponza"]);
		entityRegistry.assign<VEngine::RenderableComponent>(sponzaEntity);

		scene.load(m_engine->getRenderSystem(), "Resources/Models/monkey");
		m_monkeyEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(m_monkeyEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(0.0f, 1.8f, 0.0f), glm::quat(), 0.5f);
		entityRegistry.assign<VEngine::MeshComponent>(m_monkeyEntity, scene.m_meshInstances["Resources/Models/monkey"]);
		entityRegistry.assign<VEngine::RenderableComponent>(m_monkeyEntity);

		//scene.load(m_engine->getRenderSystem(), "Resources/Models/bistro_e");
		//entt::entity exteriorEntity = entityRegistry.create();
		//entityRegistry.assign<VEngine::TransformationComponent>(exteriorEntity, VEngine::TransformationComponent::Mobility::STATIC);
		//entityRegistry.assign<VEngine::MeshComponent>(exteriorEntity, scene.m_meshInstances["Resources/Models/bistro_e"]);
		//entityRegistry.assign<VEngine::RenderableComponent>(exteriorEntity);
		//
		//scene.load(m_engine->getRenderSystem(), "Resources/Models/bistro_i");
		//entt::entity interiorEntity = entityRegistry.create();
		//entityRegistry.assign<VEngine::TransformationComponent>(interiorEntity, VEngine::TransformationComponent::Mobility::STATIC);
		//entityRegistry.assign<VEngine::MeshComponent>(interiorEntity, scene.m_meshInstances["Resources/Models/bistro_i"]);
		//entityRegistry.assign<VEngine::RenderableComponent>(interiorEntity);

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
		VEngine::g_ssaoEnabled = input.isKeyPressed(InputKey::G);

		auto &entityRegistry = m_engine->getEntityRegistry();

		auto cameraEntity = m_engine->getRenderSystem().getCameraEntity();
		auto camC = entityRegistry.get<VEngine::CameraComponent>(cameraEntity);
		VEngine::Camera camera(entityRegistry.get<VEngine::TransformationComponent>(cameraEntity), camC);

		auto viewMatrix = camera.getViewMatrix();
		auto projMatrix = glm::perspective(camC.m_fovy, camC.m_aspectRatio, camC.m_near, camC.m_far);

		auto &monkeyTransC = entityRegistry.get<VEngine::TransformationComponent>(m_monkeyEntity);

		auto &io = ImGui::GetIO();

		static ImGuizmo::OPERATION op = ImGuizmo::OPERATION::TRANSLATE;

		if (input.isKeyPressed(InputKey::ONE))
		{
			op = ImGuizmo::OPERATION::TRANSLATE;
		}
		else if (input.isKeyPressed(InputKey::TWO))
		{
			op = ImGuizmo::OPERATION::ROTATE;
		}
		else if (input.isKeyPressed(InputKey::THREE))
		{
			op = ImGuizmo::OPERATION::SCALE;
		}

		ImGuizmo::SetRect((float)0.0f, (float)0.0f, (float)io.DisplaySize.x, (float)io.DisplaySize.y);
		ImGuizmo::Manipulate((float *)&viewMatrix, (float *)&projMatrix, op, ImGuizmo::MODE::WORLD, (float *)&monkeyTransC.m_transformation);
		glm::vec3 position;
		glm::vec3 eulerAngles;
		glm::vec3 scale;
		ImGuizmo::DecomposeMatrixToComponents((float*)&monkeyTransC.m_transformation, (float*)&position, (float *)&eulerAngles, (float *)&scale);
		
		switch (op)
		{
		case ImGuizmo::TRANSLATE:
			monkeyTransC.m_position = position;
			break;
		case ImGuizmo::ROTATE:
			monkeyTransC.m_orientation = glm::quat(glm::radians(eulerAngles));
			break;
		case ImGuizmo::SCALE:
			monkeyTransC.m_scale = scale.x;
			break;
		case ImGuizmo::BOUNDS:
			break;
		default:
			break;
		}
	};

	void shutdown() override
	{
	}

private:
	VEngine::Engine *m_engine;
	entt::entity m_monkeyEntity;
};

int main()
{
	DummyLogic logic;
	VEngine::Engine engine("Vulkan", logic);
	engine.start();

	return EXIT_SUCCESS;
}