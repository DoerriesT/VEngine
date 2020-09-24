#include "App.h"
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
#include <Components/ParticleEmitterComponent.h>
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
#include <Window/Window.h>

using namespace VEngine;

bool g_fogJittering = true;
float g_fogHistoryAlpha = 0.2f;
bool g_raymarchedFog = false;
int g_volumetricShadow = 0;

extern bool g_volumetricFogCheckerBoard;
extern bool g_volumetricFogMergedPasses;

void App::initialize(Engine *engine)
{
	m_engine = engine;

	auto &scene = m_engine->getScene();

	auto &entityRegistry = m_engine->getEntityRegistry();
	m_cameraEntity = entityRegistry.create();
	entityRegistry.assign<TransformationComponent>(m_cameraEntity, TransformationComponent::Mobility::DYNAMIC, glm::vec3(0.0f, 1.8f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(-90.0f), 0.0f)));
	entityRegistry.assign<CameraComponent>(m_cameraEntity, CameraComponent::ControllerType::FPS, m_engine->getWidth() / (float)m_engine->getHeight(), glm::radians(60.0f), 0.1f, 300.0f);
	m_engine->getRenderSystem().setCameraEntity(m_cameraEntity);
	scene.m_entities.push_back({ "Camera", m_cameraEntity });

	scene.load(m_engine->getRenderSystem(), "Resources/Models/sponza");
	entt::entity sponzaEntity = entityRegistry.create();
	entityRegistry.assign<TransformationComponent>(sponzaEntity, TransformationComponent::Mobility::STATIC);
	entityRegistry.assign<MeshComponent>(sponzaEntity, scene.m_meshInstances["Resources/Models/sponza"]);
	entityRegistry.assign<RenderableComponent>(sponzaEntity);
	scene.m_entities.push_back({ "Sponza", sponzaEntity });

	auto createReflectionProbe = [&](const glm::vec3 &bboxMin, const glm::vec3 &bboxMax, bool manualOffset = false, const glm::vec3 &capturePos = glm::vec3(0.0f))
	{
		glm::vec3 probeCenter = (bboxMin + bboxMax) * 0.5f;
		glm::vec3 captureOffset = manualOffset ? capturePos - probeCenter : glm::vec3(0.0f);

		auto reflectionProbeEntity = entityRegistry.create();
		entityRegistry.assign<TransformationComponent>(reflectionProbeEntity, TransformationComponent::Mobility::DYNAMIC, probeCenter, glm::quat(), bboxMax - probeCenter);
		entityRegistry.assign<LocalReflectionProbeComponent>(reflectionProbeEntity, captureOffset);
		entityRegistry.assign<RenderableComponent>(reflectionProbeEntity);
		scene.m_entities.push_back({ "Reflection Probe", reflectionProbeEntity });
		return reflectionProbeEntity;
	};

	// center
	createReflectionProbe(glm::vec3(-9.5, -0.05, -2.4), glm::vec3(9.5, 13.0, 2.4), true, glm::vec3(0.0f, 2.0f, 0.0f));

	// lower halls
	createReflectionProbe(glm::vec3(-9.5, -0.05, 2.4), glm::vec3(9.5, 3.9, 6.1));
	createReflectionProbe(glm::vec3(-9.5, -0.05, -6.1), glm::vec3(9.5, 3.9, -2.4));

	// lower end
	createReflectionProbe(glm::vec3(-13.7, -0.05, -6.1), glm::vec3(-9.5, 3.9, 6.1));
	createReflectionProbe(glm::vec3(9.5, -0.05, -6.1), glm::vec3(13.65, 3.9, 6.1));

	// upper halls
	createReflectionProbe(glm::vec3(-9.8, 4.15, 2.8), glm::vec3(9.8, 8.7, 6.15));
	createReflectionProbe(glm::vec3(-9.8, 4.15, -6.1), glm::vec3(9.8, 8.7, -2.8));

	// upper ends
	createReflectionProbe(glm::vec3(-13.7, 4.15, -6.1), glm::vec3(-9.8, 8.7, 6.15));
	createReflectionProbe(glm::vec3(9.8, 4.15, -6.1), glm::vec3(13.65, 8.7, 6.15));


	auto perlinNoiseTexture = m_engine->getRenderSystem().createTexture3D("Resources/Textures/perlinNoiseBC4.dds");
	auto smokeTexture = m_engine->getRenderSystem().createTexture("Resources/Textures/smoke_04.dds");
	scene.m_textures["Resources/Textures/smoke_04.dds"] = smokeTexture;
	m_engine->getRenderSystem().updateTexture3DData();
	m_engine->getRenderSystem().updateTextureData();


	auto globalFogEntity = entityRegistry.create();
	auto &gpmc = entityRegistry.assign<GlobalParticipatingMediumComponent>(globalFogEntity);
	gpmc.m_albedo = glm::vec3(1.0f);
	gpmc.m_extinction = 0.1f;
	gpmc.m_emissiveColor = glm::vec3(1.0f);
	gpmc.m_emissiveIntensity = 0.0f;
	gpmc.m_phaseAnisotropy = 0.7f;
	gpmc.m_heightFogEnabled = true;
	gpmc.m_heightFogStart = 0.0f;
	gpmc.m_heightFogFalloff = 0.1f;
	gpmc.m_maxHeight = 100.0f;
	entityRegistry.assign<RenderableComponent>(globalFogEntity);
	scene.m_entities.push_back({ "Global Fog", globalFogEntity });


	auto localFogEntity = entityRegistry.create();
	entityRegistry.assign<TransformationComponent>(localFogEntity, TransformationComponent::Mobility::DYNAMIC, glm::vec3(0.0f, 1.0f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(0.0f), 0.0f)), glm::vec3(1.0f, 1.0f, 1.0f));
	entityRegistry.assign<LocalParticipatingMediumComponent>(localFogEntity, glm::vec3(1.0f), 3.0f, glm::vec3(1.0f), 0.0f, 0.0f, false, 0.0f, 12.0f, perlinNoiseTexture, glm::vec3(18.0f, 2.0f, 11.0f) / 8.0f, glm::vec3(), true);
	entityRegistry.assign<RenderableComponent>(localFogEntity);
	scene.m_entities.push_back({ "Local Fog", localFogEntity });


	auto sunLightEntity = entityRegistry.create();
	entityRegistry.assign<TransformationComponent>(sunLightEntity, TransformationComponent::Mobility::DYNAMIC, glm::vec3(), glm::quat(glm::vec3(glm::radians(-18.5f), 0.0f, 0.0f)));
	auto &dlc = entityRegistry.assign<DirectionalLightComponent>(sunLightEntity, Utility::colorTemperatureToColor(4000.0f), 100.0f, true, 4u, 130.0f, 0.9f);
	dlc.m_depthBias[0] = 5.0f;
	dlc.m_depthBias[1] = 5.0f;
	dlc.m_depthBias[2] = 5.0f;
	dlc.m_depthBias[3] = 5.0f;
	dlc.m_normalOffsetBias[0] = 2.0f;
	dlc.m_normalOffsetBias[1] = 2.0f;
	dlc.m_normalOffsetBias[2] = 2.0f;
	dlc.m_normalOffsetBias[3] = 2.0f;
	entityRegistry.assign<RenderableComponent>(sunLightEntity);
	scene.m_entities.push_back({ "Sun Light", sunLightEntity });

	auto particleEmitter = entityRegistry.create();
	entityRegistry.assign<TransformationComponent>(particleEmitter, TransformationComponent::Mobility::DYNAMIC, glm::vec3(-4.0f, 0.0f, 0.0f));
	ParticleEmitterComponent emitterC{};
	emitterC.m_direction = glm::vec3(0.0f, 5.0f, 0.0f);
	emitterC.m_particleCount = 32;
	emitterC.m_particleLifetime = 10.0f;
	emitterC.m_velocityMagnitude = 1.0f;
	emitterC.m_spawnType = ParticleEmitterComponent::DISK;
	emitterC.m_spawnAreaSize = 0.2f;
	emitterC.m_spawnRate = 2.0f;
	emitterC.m_particleSize = 0.4f;
	emitterC.m_particleFinalSize = 2.0f;
	emitterC.m_rotation = 0.5f;
	emitterC.m_FOMOpacityMult = 0.5f;
	emitterC.m_textureHandle = smokeTexture;

	entityRegistry.assign<ParticleEmitterComponent>(particleEmitter, emitterC);

	entityRegistry.assign<RenderableComponent>(particleEmitter);
	scene.m_entities.push_back({ "Particle Emitter", particleEmitter });

	std::default_random_engine e;
	std::uniform_real_distribution<float> px(-14.0f, 14.0f);
	std::uniform_real_distribution<float> py(0.0f, 10.0f);
	std::uniform_real_distribution<float> pz(-8.0f, 8.0f);
	std::uniform_real_distribution<float> c(0.0f, 1.0f);
	std::uniform_real_distribution<float> r(0.0f, glm::two_pi<float>());

	entt::entity spotLightEntity = entityRegistry.create();
	entityRegistry.assign<TransformationComponent>(spotLightEntity, TransformationComponent::Mobility::DYNAMIC, glm::vec3(0.0f, 1.0f, 0.0f));
	entityRegistry.assign<PointLightComponent>(spotLightEntity, Utility::colorTemperatureToColor(3000.0f), 4000.0f, 8.0f, true, true);
	entityRegistry.assign<RenderableComponent>(spotLightEntity);
	scene.m_entities.push_back({ "Local Light", spotLightEntity });

	{
		glm::vec3 hangingLightPositions[]
		{
			glm::vec3(-5.560f, 1.500f, 1.789f),
			glm::vec3(-5.610f, 1.500f, -1.841f),
			glm::vec3(5.497f, 1.500f, 1.789f),
			glm::vec3(5.497f, 1.500f, -1.841f),
		};

		for (size_t i = 0; i < 4; ++i)
		{
			entt::entity lightEntity = entityRegistry.create();
			entityRegistry.assign<TransformationComponent>(lightEntity, TransformationComponent::Mobility::DYNAMIC, hangingLightPositions[i]);
			entityRegistry.assign<PointLightComponent>(lightEntity, Utility::colorTemperatureToColor(3000.0f), 2000.0f, 4.0f, true, true);
			entityRegistry.assign<RenderableComponent>(lightEntity);
			scene.m_entities.push_back({ "Hanging Light " + std::to_string(i), lightEntity });
		}

		glm::vec3 cornerLightPositions[]
		{
			glm::vec3(11.795f, 2.000f, 4.326f),
			glm::vec3(11.795f, 2.000f, -4.083f),
			glm::vec3(-11.391f, 2.000f, 4.376f),
			glm::vec3(-11.391f, 2.000f, -4.083f),
		};

		for (size_t i = 0; i < 4; ++i)
		{
			entt::entity lightEntity = entityRegistry.create();
			entityRegistry.assign<TransformationComponent>(lightEntity, TransformationComponent::Mobility::DYNAMIC, cornerLightPositions[i]);
			entityRegistry.assign<PointLightComponent>(lightEntity, Utility::colorTemperatureToColor(6000.0f), 2000.0f, 8.0f, true, true);
			entityRegistry.assign<RenderableComponent>(lightEntity);
			scene.m_entities.push_back({ "Corner Light " + std::to_string(i), lightEntity });
		}
	}

}

void App::update(float timeDelta)
{
	auto &entityRegistry = m_engine->getEntityRegistry();

	ImGui::Begin("Volumetric Fog");
	{
		Window *window = m_engine->getWindow();
		bool fullscreen = window->getWindowMode() == Window::WindowMode::FULL_SCREEN;
		if (ImGui::Checkbox("Fullscreen", &fullscreen))
		{
			window->setWindowMode(fullscreen ? Window::WindowMode::FULL_SCREEN : Window::WindowMode::WINDOWED);
		}

		bool vsync = g_VSyncEnabled;
		ImGui::Checkbox("VSync", &vsync);
		g_VSyncEnabled.set(vsync);

		int volumeWidth = g_VolumetricFogVolumeWidth;
		ImGui::DragInt("Volume Width", &volumeWidth, 1.0f, 32, 256);
		g_VolumetricFogVolumeWidth = std::min(std::max((uint32_t)volumeWidth, 32u), 256u);

		int volumeHeight = g_VolumetricFogVolumeHeight;
		ImGui::DragInt("Volume Height", &volumeHeight, 1.0f, 32, 256);
		g_VolumetricFogVolumeHeight = std::min(std::max((uint32_t)volumeHeight, 32u), 256u);

		int volumeDepth = g_VolumetricFogVolumeDepth;
		ImGui::DragInt("Volume Depth", &volumeDepth, 1.0f, 32, 256);
		g_VolumetricFogVolumeDepth = std::min(std::max((uint32_t)volumeDepth, 32u), 256u);

		bool volumetricShadow = g_volumetricShadow;

		ImGui::Checkbox("Volumetric Shadow", &volumetricShadow);
		g_volumetricShadow = volumetricShadow;
		ImGui::Checkbox("Checker Board", &g_volumetricFogCheckerBoard);
		ImGui::Checkbox("Merged Passes", &g_volumetricFogMergedPasses);

		ImGui::Checkbox("Raymarched Fog", &g_raymarchedFog);
		ImGui::Checkbox("Fog Volume Jittering", &g_fogJittering);
		ImGui::DragFloat("Fog History Alpha", &g_fogHistoryAlpha, 0.01f, 0.0f, 1.0f, "%.7f");
	}
	ImGui::End();

	// simulate wind
	static float time = 0.0f;
	time += timeDelta;

	entityRegistry.view<GlobalParticipatingMediumComponent>().each(
		[&](GlobalParticipatingMediumComponent &mediumComponent)
		{
			mediumComponent.m_textureBias = glm::vec3(time, 0.0f, time * 0.5f) * 0.2f;
		});

	entityRegistry.view<LocalParticipatingMediumComponent>().each(
		[&](LocalParticipatingMediumComponent &mediumComponent)
		{
			mediumComponent.m_textureBias = glm::vec3(time, 0.0f, time * 0.5f) * 0.2f;
		});

	entityRegistry.get<CameraComponent>(m_cameraEntity).m_aspectRatio = std::max(m_engine->getWidth(), 1u) / (float)std::max(m_engine->getHeight(), 1u);
}

void App::shutdown()
{
}
