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
#include <Components/SpotLightComponent.h>
#include <Components/DirectionalLightComponent.h>
#include <Components/ParticipatingMediumComponent.h>
#include <Components/BoundingBoxComponent.h>
#include <Components/ReflectionProbeComponent.h>
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
#include <Utility/Utility.h>

float g_fogAlbedo[3] = { 0.01f, 0.01f, 0.01f };
float g_fogExtinction = 0.01f;
float g_fogEmissiveColor[3] = { 1.0f, 1.0f, 1.0f };
float g_fogEmissiveIntensity = 0.0f;
float g_fogPhase = 0.0f;

bool g_fogJittering = true;
bool g_fogDithering = true;
bool g_fogLookupDithering = false;
bool g_fogClamping = true;
bool g_fogPrevFrameCombine = true;
bool g_fogHistoryCombine = true;
bool g_fogDoubleSample = true;
float g_fogHistoryAlpha = 0.05f;

extern float g_ssrBias;

uint32_t g_dirLightEntity;
uint32_t g_debugVoxelCascadeIndex = 0;
uint32_t g_giVoxelDebugMode = 0;
uint32_t g_allocatedBricks = 0;
bool g_forceVoxelization = false;
bool g_voxelizeOnDemand = false;

bool g_volumetricShadow = true;

entt::entity g_globalFogEntity = 0;
entt::entity g_localFogEntity = 0;

class DummyLogic : public VEngine::IGameLogic
{
public:
	void initialize(VEngine::Engine *engine) override
	{
		m_engine = engine;

		auto &entityRegistry = m_engine->getEntityRegistry();
		//entt::entity cameraEntity = entityRegistry.create();
		//entityRegistry.assign<VEngine::TransformationComponent>(cameraEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(-12.0f, 1.8f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(90.0f), 0.0f)));
		//entityRegistry.assign<VEngine::CameraComponent>(cameraEntity, VEngine::CameraComponent::ControllerType::FPS, VEngine::g_windowWidth / (float)VEngine::g_windowHeight, glm::radians(60.0f), 0.1f, 300.0f);
		//m_engine->getRenderSystem().setCameraEntity(cameraEntity);

		VEngine::Scene scene = {};
		scene.load(m_engine->getRenderSystem(), "Resources/Models/sponza");
		entt::entity sponzaEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(sponzaEntity, VEngine::TransformationComponent::Mobility::STATIC);
		entityRegistry.assign<VEngine::MeshComponent>(sponzaEntity, scene.m_meshInstances["Resources/Models/sponza"]);
		entityRegistry.assign<VEngine::RenderableComponent>(sponzaEntity);

		scene.load(m_engine->getRenderSystem(), "Resources/Models/test_orb");
		entt::entity orbEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(orbEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(), glm::quat(), glm::vec3(3.0f));
		entityRegistry.assign<VEngine::MeshComponent>(orbEntity, scene.m_meshInstances["Resources/Models/test_orb"]);
		//entityRegistry.assign<VEngine::RenderableComponent>(orbEntity);

		/*scene.load(m_engine->getRenderSystem(), "Resources/Models/gihouse");
		entt::entity giHouseEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(giHouseEntity, VEngine::TransformationComponent::Mobility::STATIC);
		entityRegistry.assign<VEngine::MeshComponent>(giHouseEntity, scene.m_meshInstances["Resources/Models/gihouse"]);
		entityRegistry.assign<VEngine::RenderableComponent>(giHouseEntity);

		scene.load(m_engine->getRenderSystem(), "Resources/Models/quad");
		entt::entity quadEntity = entityRegistry.create();
		auto &quadTc = entityRegistry.assign<VEngine::TransformationComponent>(quadEntity, VEngine::TransformationComponent::Mobility::STATIC);
		quadTc.m_scale = 60.0f;
		entityRegistry.assign<VEngine::MeshComponent>(quadEntity, scene.m_meshInstances["Resources/Models/quad"]);
		entityRegistry.assign<VEngine::RenderableComponent>(quadEntity);*/


		/*scene.load(m_engine->getRenderSystem(), "Resources/Models/bistro_e");
		entt::entity exteriorEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(exteriorEntity, VEngine::TransformationComponent::Mobility::STATIC);
		entityRegistry.assign<VEngine::MeshComponent>(exteriorEntity, scene.m_meshInstances["Resources/Models/bistro_e"]);
		entityRegistry.assign<VEngine::RenderableComponent>(exteriorEntity);

		scene.load(m_engine->getRenderSystem(), "Resources/Models/bistro_i");
		entt::entity interiorEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(interiorEntity, VEngine::TransformationComponent::Mobility::STATIC);
		entityRegistry.assign<VEngine::MeshComponent>(interiorEntity, scene.m_meshInstances["Resources/Models/bistro_i"]);
		entityRegistry.assign<VEngine::RenderableComponent>(interiorEntity);

		scene.load(m_engine->getRenderSystem(), "Resources/Models/quad");
		entt::entity quadEntity = entityRegistry.create();
		auto &quadTc = entityRegistry.assign<VEngine::TransformationComponent>(quadEntity, VEngine::TransformationComponent::Mobility::STATIC);
		quadTc.m_scale = 60.0f;
		entityRegistry.assign<VEngine::MeshComponent>(quadEntity, scene.m_meshInstances["Resources/Models/quad"]);
		entityRegistry.assign<VEngine::RenderableComponent>(quadEntity);*/

		//scene.load(m_engine->getRenderSystem(), "Resources/Models/mori_knob");
		//entt::entity knobEntity = entityRegistry.create();
		//entityRegistry.assign<VEngine::TransformationComponent>(knobEntity, VEngine::TransformationComponent::Mobility::STATIC);
		//entityRegistry.assign<VEngine::MeshComponent>(knobEntity, scene.m_meshInstances["Resources/Models/mori_knob"]);
		//entityRegistry.assign<VEngine::RenderableComponent>(knobEntity);


		auto createReflectionProbe = [&](const glm::vec3 &bboxMin, const glm::vec3 &bboxMax, bool manualOffset = false, const glm::vec3 &capturePos = glm::vec3(0.0f))
		{
			glm::vec3 probeCenter = (bboxMin + bboxMax) * 0.5f;
			glm::vec3 captureOffset = manualOffset ? capturePos - probeCenter : glm::vec3(0.0f);

			auto reflectionProbeEntity = entityRegistry.create();
			entityRegistry.assign<VEngine::TransformationComponent>(reflectionProbeEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, probeCenter, glm::quat(), bboxMax - probeCenter);
			entityRegistry.assign<VEngine::LocalReflectionProbeComponent>(reflectionProbeEntity, captureOffset, 0.0f);
			entityRegistry.assign<VEngine::RenderableComponent>(reflectionProbeEntity);
			return reflectionProbeEntity;
		};

		// center
		createReflectionProbe(glm::vec3(-9.5, -0.01, -2.4), glm::vec3(9.5, 13.0, 2.4), true, glm::vec3(0.0f, 2.0f, 0.0f));

		// lower halls
		createReflectionProbe(glm::vec3(-9.5, -0.01, 2.4), glm::vec3(9.5, 3.9, 6.1));
		createReflectionProbe(glm::vec3(-9.5, -0.01, -6.1), glm::vec3(9.5, 3.9, -2.4));

		// lower end
		createReflectionProbe(glm::vec3(-13.7, -0.01, -6.1), glm::vec3(-9.5, 3.9, 6.1));
		createReflectionProbe(glm::vec3(9.5, -0.01, -6.1), glm::vec3(13.65, 3.9, 6.1));

		// upper halls
		createReflectionProbe(glm::vec3(-9.8, 4.15, 2.8), glm::vec3(9.8, 8.7, 6.15));
		createReflectionProbe(glm::vec3(-9.8, 4.15, -6.1), glm::vec3(9.8, 8.7, -2.8));

		// upper ends
		createReflectionProbe(glm::vec3(-13.7, 4.15, -6.1), glm::vec3(-9.8, 8.7, 6.15));
		createReflectionProbe(glm::vec3(9.8, 4.15, -6.1), glm::vec3(13.65, 8.7, 6.15));



		g_globalFogEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::GlobalParticipatingMediumComponent>(g_globalFogEntity, glm::vec3(1.0f), 0.0001f, glm::vec3(1.0f), 0.0f, 0.0f);
		entityRegistry.assign<VEngine::RenderableComponent>(g_globalFogEntity);


		g_localFogEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(g_localFogEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(0.0f, 1.0f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(45.0f), 0.0f)));
		entityRegistry.assign<VEngine::BoundingBoxComponent>(g_localFogEntity, glm::vec3(1.0f, 1.0f, 1.0f));
		entityRegistry.assign<VEngine::LocalParticipatingMediumComponent>(g_localFogEntity, glm::vec3(222, 184, 135) / 255.0f, 3.0f, glm::vec3(1.0f), 0.0f, 0.0f);
		entityRegistry.assign<VEngine::RenderableComponent>(g_localFogEntity);


		g_dirLightEntity = m_sunLightEntity = entityRegistry.create();
		entityRegistry.assign<VEngine::TransformationComponent>(m_sunLightEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(), glm::quat(glm::vec3(glm::radians(-18.5f), 0.0f, 0.0f)));
		auto &dlc = entityRegistry.assign<VEngine::DirectionalLightComponent>(m_sunLightEntity, VEngine::Utility::colorTemperatureToColor(4000.0f), 100.0f, true, 4u, 130.0f, 0.9f);
		dlc.m_depthBias[0] = 5.0f;
		dlc.m_depthBias[1] = 5.0f;
		dlc.m_depthBias[2] = 5.0f;
		dlc.m_depthBias[3] = 5.0f;
		dlc.m_normalOffsetBias[0] = 2.0f;
		dlc.m_normalOffsetBias[1] = 2.0f;
		dlc.m_normalOffsetBias[2] = 2.0f;
		dlc.m_normalOffsetBias[3] = 2.0f;
		entityRegistry.assign<VEngine::RenderableComponent>(m_sunLightEntity);

		std::default_random_engine e;
		std::uniform_real_distribution<float> px(-14.0f, 14.0f);
		std::uniform_real_distribution<float> py(0.0f, 10.0f);
		std::uniform_real_distribution<float> pz(-8.0f, 8.0f);
		std::uniform_real_distribution<float> c(0.0f, 1.0f);
		std::uniform_real_distribution<float> r(0.0f, glm::two_pi<float>());

		//for (size_t i = 0; i < 16; ++i)
		//{
		//	entt::entity lightEntity = entityRegistry.create();
		//	entityRegistry.assign<VEngine::TransformationComponent>(lightEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(px(e), py(e), pz(e)));
		//	entityRegistry.assign<VEngine::PointLightComponent>(lightEntity, glm::vec3(c(e), c(e), c(e)), 1000.0f, 8.0f, true);
		//	entityRegistry.assign<VEngine::RenderableComponent>(lightEntity);
		//}

		//entt::entity pointLightEntity = entityRegistry.create();
		//entityRegistry.assign<VEngine::TransformationComponent>(pointLightEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(10.0f, 1.0f, 0.0f));
		//entityRegistry.assign<VEngine::PointLightComponent>(pointLightEntity, glm::vec3(c(e), c(e), c(e)), 1000.0f, 8.0f);
		//entityRegistry.assign<VEngine::RenderableComponent>(pointLightEntity);
		entt::entity spotLightEntity = entityRegistry.create();
		m_spotLightEntity = spotLightEntity;
		//entityRegistry.assign<VEngine::TransformationComponent>(m_spotLightEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(0.0f, 1.0f, 0.0f));
		//entityRegistry.assign<VEngine::PointLightComponent>(m_spotLightEntity, glm::vec3(c(e), c(e), c(e)), 1000.0f, 8.0f, true);
		//entityRegistry.assign<VEngine::RenderableComponent>(m_spotLightEntity);
		entityRegistry.assign<VEngine::TransformationComponent>(spotLightEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(0.0f, 1.0f, 0.0f));
		entityRegistry.assign<VEngine::SpotLightComponent>(spotLightEntity, VEngine::Utility::colorTemperatureToColor(3000.0f), 4000.0f, 8.0f, glm::radians(45.0f), glm::radians(15.0f), true);
		//entityRegistry.assign<VEngine::PointLightComponent>(spotLightEntity, VEngine::Utility::colorTemperatureToColor(3000.0f), 4000.0f, 8.0f, true);
		entityRegistry.assign<VEngine::RenderableComponent>(spotLightEntity);

		//for (size_t i = 0; i < 64; ++i)
		//{
		//	entt::entity lightEntity = entityRegistry.create();
		//	entityRegistry.assign<VEngine::TransformationComponent>(lightEntity, VEngine::TransformationComponent::Mobility::DYNAMIC, glm::vec3(px(e), py(e), pz(e)), glm::quat(glm::vec3(r(e), r(e), r(e))));
		//	entityRegistry.assign<VEngine::SpotLightComponent>(lightEntity, glm::vec3(c(e), c(e), c(e)), 1000.0f, 8.0f, glm::radians(79.0f), glm::radians(15.0f), true);
		//	entityRegistry.assign<VEngine::RenderableComponent>(lightEntity);
		//}
	}

	void update(float timeDelta) override
	{
		auto &entityRegistry = m_engine->getEntityRegistry();

		static int entityIdx = 0;

		ImGui::Begin("Volumetric Fog");
		{
			//ImGui::DragFloat("SSR Bias", &g_ssrBias, 0.01f, 0.0f, 1.0f);

			ImGui::RadioButton("Spot Light", &entityIdx, 0); ImGui::SameLine();
			ImGui::RadioButton("Sun Light", &entityIdx, 1); ImGui::SameLine();
			ImGui::RadioButton("Local Fog Volume", &entityIdx, 2);

			ImGui::NewLine();

			ImGui::Checkbox("Fog Volume Jittering", &g_fogJittering);
			ImGui::Checkbox("Fog Lookup Dithering", &g_fogLookupDithering);
			ImGui::Checkbox("Fog Volume Dithering", &g_fogDithering);
			ImGui::Checkbox("Fog Neighborhood Clamping", &g_fogClamping);
			ImGui::Checkbox("Fog Prev Frame Combine", &g_fogPrevFrameCombine);
			ImGui::Checkbox("Fog History Combine", &g_fogHistoryCombine);
			ImGui::Checkbox("Fog 2x Sample", &g_fogDoubleSample);
			ImGui::DragFloat("Fog History Alpha", &g_fogHistoryAlpha, 0.01f, 0.0f, 1.0f, "%.7f");

			ImGui::NewLine();
			ImGui::Separator();
			ImGui::NewLine();
			ImGui::ColorEdit3("Albedo", g_fogAlbedo);
			ImGui::DragFloat("Extinction", &g_fogExtinction, 0.001f, 0.0f, FLT_MAX, "%.7f");
			ImGui::DragFloat("Phase", &g_fogPhase, 0.001f, -0.9f, 0.9f, "%.7f");
			ImGui::ColorEdit3("Emissive Color", g_fogEmissiveColor);
			ImGui::DragFloat("Emissive Intensity", &g_fogEmissiveIntensity, 0.001f, 0.0f, FLT_MAX, "%.7f");

			auto &mediaC = entityRegistry.get<VEngine::GlobalParticipatingMediumComponent>(g_globalFogEntity);
			mediaC.m_albedo = glm::vec3(g_fogAlbedo[0], g_fogAlbedo[1], g_fogAlbedo[2]);
			mediaC.m_extinction = g_fogExtinction;
			mediaC.m_emissiveColor = glm::vec3(g_fogEmissiveColor[0], g_fogEmissiveColor[1], g_fogEmissiveColor[2]);
			mediaC.m_emissiveIntensity = g_fogEmissiveIntensity;
			mediaC.m_phaseAnisotropy = g_fogPhase;
		}
		ImGui::End();

		entt::entity entities[] = { m_spotLightEntity, m_sunLightEntity, g_localFogEntity };

		auto cameraEntity = m_engine->getRenderSystem().getCameraEntity();
		auto camC = entityRegistry.get<VEngine::CameraComponent>(cameraEntity);
		VEngine::Camera camera(entityRegistry.get<VEngine::TransformationComponent>(cameraEntity), camC);

		auto viewMatrix = camera.getViewMatrix();
		auto projMatrix = glm::perspective(camC.m_fovy, camC.m_aspectRatio, camC.m_near, camC.m_far);

		auto &tansC = entityRegistry.get<VEngine::TransformationComponent>(entities[entityIdx]);

		auto &io = ImGui::GetIO();

		static ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
		static ImGuizmo::MODE mode = ImGuizmo::MODE::WORLD;

		auto &input = m_engine->getUserInput();
		if (input.isKeyPressed(InputKey::ONE))
		{
			operation = ImGuizmo::OPERATION::TRANSLATE;
		}
		else if (input.isKeyPressed(InputKey::TWO))
		{
			operation = ImGuizmo::OPERATION::ROTATE;
		}

		if (input.isKeyPressed(InputKey::F1))
		{
			mode = ImGuizmo::MODE::WORLD;
		}
		else if (input.isKeyPressed(InputKey::F2))
		{
			mode = ImGuizmo::MODE::LOCAL;
		}

		if (input.isKeyPressed(InputKey::V))
		{
			g_volumetricShadow = true;
		}
		else if (input.isKeyPressed(InputKey::B))
		{
			g_volumetricShadow = false;
		}

		glm::mat4 orientation = glm::translate(tansC.m_position) * glm::mat4_cast(tansC.m_orientation);

		ImGuizmo::SetRect((float)0.0f, (float)0.0f, (float)io.DisplaySize.x, (float)io.DisplaySize.y);
		ImGuizmo::Manipulate((float *)&viewMatrix, (float *)&projMatrix, operation, mode, (float *)&orientation);
		glm::vec3 position;
		glm::vec3 eulerAngles;
		glm::vec3 scale;
		ImGuizmo::DecomposeMatrixToComponents((float *)&orientation, (float *)&position, (float *)&eulerAngles, (float *)&scale);

		if (operation == ImGuizmo::OPERATION::TRANSLATE)
		{
			tansC.m_position = position;
		}
		else if (operation == ImGuizmo::OPERATION::ROTATE)
		{
			tansC.m_orientation = glm::quat(glm::radians(eulerAngles));
		}
	};

	void shutdown() override
	{
	}

private:
	VEngine::Engine *m_engine;
	entt::entity m_sunLightEntity;
	entt::entity m_spotLightEntity;
};

int main()
{
	DummyLogic logic;
	VEngine::Engine engine("Vulkan", logic);
	engine.start();

	return EXIT_SUCCESS;
}