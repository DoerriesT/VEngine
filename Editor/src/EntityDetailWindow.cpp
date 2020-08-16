#include "EntityDetailWindow.h"
#include <Graphics/imgui/imgui.h>
#include <Graphics/imgui/ImGuizmo.h>
#include <Components/TransformationComponent.h>
#include <Components/RenderableComponent.h>
#include <Components/DirectionalLightComponent.h>
#include <Components/CameraComponent.h>
#include <Components/PointLightComponent.h>
#include <Components/SpotLightComponent.h>
#include <Components/ParticipatingMediumComponent.h>
#include <Components/ParticleEmitterComponent.h>
#include <Components/ReflectionProbeComponent.h>
#include <Engine.h>
#include <Graphics/Camera/Camera.h>
#include <Graphics/RenderSystem.h>
#include <glm/ext.hpp>

using namespace VEngine;

template <typename T>
static bool beginComponent(entt::registry &registry, entt::entity entity, const char *title, T *&component)
{
	ImGui::PushID((void *)title);
	component = registry.try_get<T>(entity);
	bool result = component != nullptr;

	bool open = true;
	if (result)
	{
		result = ImGui::CollapsingHeader(title, &open, ImGuiTreeNodeFlags_DefaultOpen);

		if (!open)
		{
			ImGui::OpenPopup("Delete Component?");
		}
		if (ImGui::BeginPopupModal("Delete Component?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Delete %s Component.\nThis operation cannot be undone!\n\n", title);
			ImGui::Separator();

			if (ImGui::Button("OK", ImVec2(120, 0)))
			{
				registry.remove<T>(entity);
				result = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	ImGui::PopID();

	return result;
}

template<typename T>
static void selectableAddComponent(entt::registry &registry, entt::entity entity, const char *title)
{
	if (!registry.has<T>(entity) && ImGui::Selectable(title))
	{
		registry.assign<T>(entity);
	}
}

static void drawSphereGeometry(RenderSystem &renderSystem, const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded)
{
	const size_t segmentCount = 32;

	glm::mat4 transform = glm::translate(position) * glm::mat4_cast(rotation) * glm::scale(scale);

	for (size_t i = 0; i < segmentCount; ++i)
	{
		float angle0 = i / (float)segmentCount * (2.0f * glm::pi<float>());
		float angle1 = (i + 1) / (float)segmentCount * (2.0f * glm::pi<float>());
		float x0 = cosf(angle0);
		float x1 = cosf(angle1);
		float y0 = sinf(angle0);
		float y1 = sinf(angle1);

		glm::vec3 p0, p1;

		// horizontal
		p0 = transform * glm::vec4(x0, 0.0f, y0, 1.0f);
		p1 = transform * glm::vec4(x1, 0.0f, y1, 1.0f);
		renderSystem.drawDebugLineVisible(p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			renderSystem.drawDebugLineHidden(p0, p1, occludedColor, occludedColor);
		}

		// vertical z
		p0 = transform * glm::vec4(0.0f, x0, y0, 1.0f);
		p1 = transform * glm::vec4(0.0f, x1, y1, 1.0f);
		renderSystem.drawDebugLineVisible(p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			renderSystem.drawDebugLineHidden(p0, p1, occludedColor, occludedColor);
		}

		// vertical x
		p0 = transform * glm::vec4(x0, y0, 0.0f, 1.0f);
		p1 = transform * glm::vec4(x1, y1, 0.0f, 1.0f);
		renderSystem.drawDebugLineVisible(p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			renderSystem.drawDebugLineHidden(p0, p1, occludedColor, occludedColor);
		}
	}
}

static void drawBoxGeometry(RenderSystem &renderSystem, const glm::mat4 &transform, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded)
{
	glm::vec3 corners[2][2][2];

	for (int x = 0; x < 2; ++x)
	{
		for (int y = 0; y < 2; ++y)
		{
			for (int z = 0; z < 2; ++z)
			{
				corners[x][y][z] = transform * glm::vec4(glm::vec3(x, y, z) * 2.0f - 1.0f, 1.0f);
			}
		}
	}

	renderSystem.drawDebugLineVisible(corners[0][0][0], corners[1][0][0], visibleColor, visibleColor);
	renderSystem.drawDebugLineVisible(corners[0][0][1], corners[1][0][1], visibleColor, visibleColor);
	renderSystem.drawDebugLineVisible(corners[0][0][0], corners[0][0][1], visibleColor, visibleColor);
	renderSystem.drawDebugLineVisible(corners[1][0][0], corners[1][0][1], visibleColor, visibleColor);
	
	renderSystem.drawDebugLineVisible(corners[0][1][0], corners[1][1][0], visibleColor, visibleColor);
	renderSystem.drawDebugLineVisible(corners[0][1][1], corners[1][1][1], visibleColor, visibleColor);
	renderSystem.drawDebugLineVisible(corners[0][1][0], corners[0][1][1], visibleColor, visibleColor);
	renderSystem.drawDebugLineVisible(corners[1][1][0], corners[1][1][1], visibleColor, visibleColor);
	
	renderSystem.drawDebugLineVisible(corners[0][0][0], corners[0][1][0], visibleColor, visibleColor);
	renderSystem.drawDebugLineVisible(corners[0][0][1], corners[0][1][1], visibleColor, visibleColor);
	renderSystem.drawDebugLineVisible(corners[1][0][0], corners[1][1][0], visibleColor, visibleColor);
	renderSystem.drawDebugLineVisible(corners[1][0][1], corners[1][1][1], visibleColor, visibleColor);

	if (drawOccluded)
	{
		renderSystem.drawDebugLineHidden(corners[0][0][0], corners[1][0][0], occludedColor, occludedColor);
		renderSystem.drawDebugLineHidden(corners[0][0][1], corners[1][0][1], occludedColor, occludedColor);
		renderSystem.drawDebugLineHidden(corners[0][0][0], corners[0][0][1], occludedColor, occludedColor);
		renderSystem.drawDebugLineHidden(corners[1][0][0], corners[1][0][1], occludedColor, occludedColor);

		renderSystem.drawDebugLineHidden(corners[0][1][0], corners[1][1][0], occludedColor, occludedColor);
		renderSystem.drawDebugLineHidden(corners[0][1][1], corners[1][1][1], occludedColor, occludedColor);
		renderSystem.drawDebugLineHidden(corners[0][1][0], corners[0][1][1], occludedColor, occludedColor);
		renderSystem.drawDebugLineHidden(corners[1][1][0], corners[1][1][1], occludedColor, occludedColor);

		renderSystem.drawDebugLineHidden(corners[0][0][0], corners[0][1][0], occludedColor, occludedColor);
		renderSystem.drawDebugLineHidden(corners[0][0][1], corners[0][1][1], occludedColor, occludedColor);
		renderSystem.drawDebugLineHidden(corners[1][0][0], corners[1][1][0], occludedColor, occludedColor);
		renderSystem.drawDebugLineHidden(corners[1][0][1], corners[1][1][1], occludedColor, occludedColor);
	}
}

VEditor::EntityDetailWindow::EntityDetailWindow(VEngine::Engine *engine)
	:m_engine(engine),
	m_lastDisplayedEntity(entt::null),
	m_translateRotateScaleMode(),
	m_localTransformMode(),
	m_visible()
{
}

void VEditor::EntityDetailWindow::draw(entt::entity entity, entt::entity editorCameraEntity, ImGuiContext *gameContext)
{
	if (entity != m_lastDisplayedEntity)
	{
		m_lastDisplayedEntity = entity;
		m_translateRotateScaleMode = 0;
		m_localTransformMode = false;
	}
	auto &entityRegistry = m_engine->getEntityRegistry();

	bool showGuizmo = false;

	ImGui::Begin("Details");

	if (entity != entt::null)
	{
		char entityName[128] = "";
		size_t entityIndex = 0;
		// look up name of selected entity
		{
			auto &scene = m_engine->getScene();
			bool found = false;
			for (auto &e : scene.m_entities)
			{
				if (e.second == entity)
				{
					strcpy_s(entityName, e.first.c_str());
					found = true;
					break;
				}
				++entityIndex;
			}
			assert(found);
		}

		if (ImGui::InputText("Entity Name", entityName, IM_ARRAYSIZE(entityName), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			auto &scene = m_engine->getScene();
			scene.m_entities[entityIndex].first = entityName;
		}

		if (ImGui::Button("Add Component"))
		{
			ImGui::OpenPopup("add_component_popup");
		}

		if (ImGui::BeginPopup("add_component_popup"))
		{

			ImGui::Text("Component to add:");
			ImGui::Separator();

			selectableAddComponent<TransformationComponent>(entityRegistry, entity, "Transformation");
			selectableAddComponent<DirectionalLightComponent>(entityRegistry, entity, "Directional Light");
			selectableAddComponent<PointLightComponent>(entityRegistry, entity, "Point Light");
			selectableAddComponent<SpotLightComponent>(entityRegistry, entity, "Spot Light");
			selectableAddComponent<CameraComponent>(entityRegistry, entity, "Camera");
			selectableAddComponent<LocalParticipatingMediumComponent>(entityRegistry, entity, "Local Participating Medium");
			selectableAddComponent<GlobalParticipatingMediumComponent>(entityRegistry, entity, "Global Participating Medium");
			selectableAddComponent<ParticleEmitterComponent>(entityRegistry, entity, "Particle Emitter");
			selectableAddComponent<LocalReflectionProbeComponent>(entityRegistry, entity, "Local Reflection Probe");
			selectableAddComponent<RenderableComponent>(entityRegistry, entity, "Renderable");

			ImGui::EndPopup();
		}

		ImGui::Separator();
		ImGui::NewLine();

		ImGui::BeginChild("Child1");
		{
			const glm::vec4 visibleLightDebugColor = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
			const glm::vec4 occludedLightDebugColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);

			TransformationComponent *tc = nullptr;

			// transformation
			if (beginComponent(entityRegistry, entity, "Transformation", tc))
			{
				showGuizmo = true;

				const char *mobilityItems[] = { "STATIC", "DYNAMIC" };
				int currentMobility = static_cast<int>(tc->m_mobility);
				if (ImGui::Combo("Mobility", &currentMobility, mobilityItems, IM_ARRAYSIZE(mobilityItems)))
				{
					tc->m_mobility = static_cast<TransformationComponent::Mobility>(currentMobility);
				}
				ImGui::DragFloat3("Position", &tc->m_position[0]);
				glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(tc->m_orientation));
				if (ImGui::DragFloat3("Orientation", &eulerAngles[0]))
				{
					tc->m_orientation = glm::quat(glm::radians(eulerAngles));
				}
				ImGui::DragFloat3("Scale", &tc->m_scale[0]);

				ImGui::RadioButton("Translate", &m_translateRotateScaleMode, 0); ImGui::SameLine();
				ImGui::RadioButton("Rotate", &m_translateRotateScaleMode, 1); ImGui::SameLine();
				ImGui::RadioButton("Scale", &m_translateRotateScaleMode, 2);
				ImGui::Checkbox("Local Transform Mode", &m_localTransformMode);
			}

			// directional light
			if (DirectionalLightComponent *dlc = nullptr; beginComponent(entityRegistry, entity, "Directional Light", dlc))
			{
				ImGui::ColorEdit3("Color", &dlc->m_color[0]);
				ImGui::DragFloat("Intensity", &dlc->m_intensity, 1.0f, 0.0f, 130000.0f);
				ImGui::Checkbox("Shadows", &dlc->m_shadows);
				if (ImGui::InputInt("Cascades", (int *)&dlc->m_cascadeCount))
				{
					dlc->m_cascadeCount = glm::min(dlc->m_cascadeCount, static_cast<uint32_t>(DirectionalLightComponent::MAX_CASCADES));
				}
				if (ImGui::DragFloat("Max. Shadow Distance", &dlc->m_maxShadowDistance))
				{
					dlc->m_maxShadowDistance = glm::max(dlc->m_maxShadowDistance, 0.0f);
				}
				if (ImGui::DragFloat("Cascade Split Lambda", &dlc->m_splitLambda, 0.05f, 0.0f, 1.0f))
				{
					dlc->m_splitLambda = glm::clamp(dlc->m_splitLambda, 0.0f, 1.0f);
				}
				for (uint32_t i = 0; i < dlc->m_cascadeCount; ++i)
				{
					char cascadeLabel[] = "Cascade X";
					cascadeLabel[sizeof(cascadeLabel) - 2] = '0' + i;
					ImGui::Text(cascadeLabel);
					ImGui::PushID(&dlc->m_depthBias[i]);
					if (ImGui::DragFloat("Depth Bias", &dlc->m_depthBias[i], 0.05f, 0.0f, 20.0f))
					{
						dlc->m_depthBias[i] = glm::clamp(dlc->m_depthBias[i], 0.0f, 20.0f);
					}
					if (ImGui::DragFloat("Normal Offset Bias", &dlc->m_normalOffsetBias[i], 0.05f, 0.0f, 20.0f))
					{
						dlc->m_normalOffsetBias[i] = glm::clamp(dlc->m_normalOffsetBias[i], 0.0f, 20.0f);
					}
					ImGui::PopID();
				}

				// draw directional light debug geometry
				if (tc && entityRegistry.has<RenderableComponent>(entity))
				{
					auto &renderSystem = m_engine->getRenderSystem();

					const size_t segmentCount = 32;

					glm::mat3 rotation = glm::mat3_cast(tc->m_orientation);

					glm::vec3 arrowHead = rotation * glm::vec3(0.0f, 1.0f, 0.0f) + tc->m_position;
					glm::vec3 arrowTail = rotation * glm::vec3(0.0f, 4.0f, 0.0f) + tc->m_position;
					glm::vec3 arrowHeadX0 = rotation * glm::vec3(0.25f, 1.25f, 0.0f) + tc->m_position;
					glm::vec3 arrowHeadX1 = rotation * glm::vec3(-0.25f, 1.25f, 0.0f) + tc->m_position;
					glm::vec3 arrowHeadZ0 = rotation * glm::vec3(0.0f, 1.25f, 0.25f) + tc->m_position;
					glm::vec3 arrowHeadZ1 = rotation * glm::vec3(0.0f, 1.25f, -0.25f) + tc->m_position;

					renderSystem.drawDebugLine(arrowHead, arrowTail, visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(arrowHead, arrowHeadX0, visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(arrowHead, arrowHeadX1, visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(arrowHead, arrowHeadZ0, visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(arrowHead, arrowHeadZ1, visibleLightDebugColor, visibleLightDebugColor);
				}
			}

			// point light
			if (PointLightComponent *plc = nullptr; beginComponent(entityRegistry, entity, "Point Light", plc))
			{
				ImGui::ColorEdit3("Color", &plc->m_color[0]);
				ImGui::DragFloat("Luminous Power", &plc->m_luminousPower, 1.0f, 0.0f, 130000.0f);
				ImGui::DragFloat("Radius", &plc->m_radius, 0.05f, 0.0f, 128.0f);
				ImGui::Checkbox("Shadows", &plc->m_shadows);
				if (plc->m_shadows)
				{
					ImGui::Checkbox("Volumetric Shadows", &plc->m_volumetricShadows);
				}

				// draw point light debug geometry
				if (tc && entityRegistry.has<RenderableComponent>(entity))
				{
					auto &renderSystem = m_engine->getRenderSystem();
					drawSphereGeometry(renderSystem, tc->m_position, tc->m_orientation, glm::vec3(plc->m_radius), visibleLightDebugColor, occludedLightDebugColor, false);
				}
			}

			// spot light
			if (SpotLightComponent *slc = nullptr; beginComponent(entityRegistry, entity, "Spot Light", slc))
			{
				ImGui::ColorEdit3("Color", &slc->m_color[0]);
				ImGui::DragFloat("Luminous Power", &slc->m_luminousPower, 1.0f, 0.0f, 130000.0f);
				ImGui::DragFloat("Radius", &slc->m_radius, 0.05f, 0.0f, 128.0f);
				float outerAngleDegrees = glm::degrees(slc->m_outerAngle);
				float innerAngleDegrees = glm::degrees(slc->m_innerAngle);
				if (ImGui::DragFloat("Outer Angle", &outerAngleDegrees))
				{
					slc->m_outerAngle = glm::radians(glm::clamp(outerAngleDegrees, 0.0f, 89.0f));
				}
				if (ImGui::DragFloat("Inner Angle", &innerAngleDegrees))
				{
					slc->m_innerAngle = glm::radians(glm::clamp(innerAngleDegrees, 0.0f, outerAngleDegrees));
				}

				ImGui::Checkbox("Shadows", &slc->m_shadows);
				if (slc->m_shadows)
				{
					ImGui::Checkbox("Volumetric Shadows", &slc->m_volumetricShadows);
				}

				// draw point light debug geometry
				if (tc && entityRegistry.has<RenderableComponent>(entity))
				{
					auto &renderSystem = m_engine->getRenderSystem();

					const size_t segmentCount = 32;
					const size_t capSegmentCount = 16;

					glm::mat3 rotation = glm::mat3_cast(tc->m_orientation);

					// cone lines
					for (size_t i = 0; i < segmentCount; ++i)
					{
						float angle = i / (float)segmentCount * (2.0f * glm::pi<float>());
						float x = cosf(angle);
						float y = sinf(angle);

						float adjacent = cosf(slc->m_outerAngle * 0.5f) * slc->m_radius;
						float opposite = tanf(slc->m_outerAngle * 0.5f) * adjacent;
						x *= opposite;
						y *= opposite;

						glm::vec3 p0 = tc->m_position;
						glm::vec3 p1 = rotation * -glm::vec3(x, y, adjacent) + tc->m_position;

						renderSystem.drawDebugLineVisible(p0, p1, visibleLightDebugColor, visibleLightDebugColor);
						//renderSystem.drawDebugLineHidden(p0, p1, occludedLightDebugColor, occludedLightDebugColor);
					}

					// cap
					for (size_t i = 0; i < capSegmentCount; ++i)
					{
						const float offset = 0.5f * (glm::pi<float>() - slc->m_outerAngle);
						float angle0 = i / (float)capSegmentCount * slc->m_outerAngle + offset;
						float angle1 = (i + 1) / (float)capSegmentCount * slc->m_outerAngle + offset;
						float x0 = cosf(angle0) * slc->m_radius;
						float x1 = cosf(angle1) * slc->m_radius;
						float y0 = sinf(angle0) * slc->m_radius;
						float y1 = sinf(angle1) * slc->m_radius;

						glm::vec3 p0, p1;

						// horizontal
						p0 = rotation * -glm::vec3(x0, 0.0f, y0) + tc->m_position;
						p1 = rotation * -glm::vec3(x1, 0.0f, y1) + tc->m_position;
						renderSystem.drawDebugLineVisible(p0, p1, visibleLightDebugColor, visibleLightDebugColor);
						//renderSystem.drawDebugLineHidden(p0, p1, occludedLightDebugColor, occludedLightDebugColor);

						// vertical
						p0 = rotation * -glm::vec3(0.0f, x0, y0) + tc->m_position;
						p1 = rotation * -glm::vec3(0.0f, x1, y1) + tc->m_position;
						renderSystem.drawDebugLineVisible(p0, p1, visibleLightDebugColor, visibleLightDebugColor);
						//renderSystem.drawDebugLineHidden(p0, p1, occludedLightDebugColor, occludedLightDebugColor);
					}
				}
			}

			// camera
			if (CameraComponent *cc = nullptr; beginComponent(entityRegistry, entity, "Camera", cc))
			{
				if (ImGui::DragFloat("Aspect Ratio", &cc->m_aspectRatio))
				{
					cc->m_aspectRatio = glm::max(1e-6f, cc->m_aspectRatio);
				}
				float fovyDegrees = glm::degrees(cc->m_fovy);
				if (ImGui::DragFloat("Vertical FOV", &fovyDegrees))
				{
					cc->m_fovy = glm::radians(glm::clamp(fovyDegrees, 1.0f, 179.0f));
				}
				if (ImGui::DragFloat("Near Plane", &cc->m_near))
				{
					cc->m_near = glm::clamp(cc->m_near, 1e-6f, cc->m_far);
				}
				if (ImGui::DragFloat("Far Plane", &cc->m_far))
				{
					cc->m_far = glm::max(cc->m_far, cc->m_near);
				}

				// draw debug geometry
				if (tc)
				{
					auto &renderSystem = m_engine->getRenderSystem();

					glm::mat4 transform = glm::inverse(cc->m_projectionMatrix * cc->m_viewMatrix);

					glm::vec3 corners[2][2][2];

					for (int x = 0; x < 2; ++x)
					{
						for (int y = 0; y < 2; ++y)
						{
							for (int z = 0; z < 2; ++z)
							{
								glm::vec3 p3 = glm::vec3(glm::vec2(x, y) * 2.0f - 1.0f, z);
								glm::vec4 p = transform * glm::vec4(p3, 1.0f);
								corners[x][y][z] = p / p.w;
							}
						}
					}

					renderSystem.drawDebugLine(corners[0][0][0], corners[1][0][0], visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(corners[0][0][1], corners[1][0][1], visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(corners[0][0][0], corners[0][0][1], visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(corners[1][0][0], corners[1][0][1], visibleLightDebugColor, visibleLightDebugColor);

					renderSystem.drawDebugLine(corners[0][1][0], corners[1][1][0], visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(corners[0][1][1], corners[1][1][1], visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(corners[0][1][0], corners[0][1][1], visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(corners[1][1][0], corners[1][1][1], visibleLightDebugColor, visibleLightDebugColor);

					renderSystem.drawDebugLine(corners[0][0][0], corners[0][1][0], visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(corners[0][0][1], corners[0][1][1], visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(corners[1][0][0], corners[1][1][0], visibleLightDebugColor, visibleLightDebugColor);
					renderSystem.drawDebugLine(corners[1][0][1], corners[1][1][1], visibleLightDebugColor, visibleLightDebugColor);
				}
			}

			// local participating medium
			if (LocalParticipatingMediumComponent *lpmc = nullptr; beginComponent(entityRegistry, entity, "Local Participating Medium", lpmc))
			{
				ImGui::ColorEdit3("Albedo", &lpmc->m_albedo[0]);
				ImGui::DragFloat("Extinction", &lpmc->m_extinction, 0.001f, 0.0f, FLT_MAX, "%.7f");
				ImGui::DragFloat("Phase", &lpmc->m_phaseAnisotropy, 0.001f, -0.9f, 0.9f, "%.7f");
				ImGui::ColorEdit3("Emissive Color", &lpmc->m_emissiveColor[0]);
				ImGui::DragFloat("Emissive Intensity", &lpmc->m_emissiveIntensity, 0.001f, 0.0f, FLT_MAX, "%.7f");
				ImGui::Checkbox("Height Fog", &lpmc->m_heightFogEnabled);
				ImGui::DragFloat("Height Fog Start", &lpmc->m_heightFogStart, 0.1f);
				ImGui::DragFloat("Height Fog Falloff", &lpmc->m_heightFogFalloff, 0.1f);
				ImGui::Checkbox("Spherical", &lpmc->m_spherical);

				// draw point light debug geometry
				if (tc && entityRegistry.has<RenderableComponent>(entity))
				{
					auto &renderSystem = m_engine->getRenderSystem();

					glm::mat4 transform = glm::translate(tc->m_position) * glm::mat4_cast(tc->m_orientation) * glm::scale(tc->m_scale);

					if (lpmc->m_spherical)
					{
						drawSphereGeometry(renderSystem, tc->m_position, tc->m_orientation, tc->m_scale, visibleLightDebugColor, occludedLightDebugColor, false);
					}
					else
					{
						drawBoxGeometry(renderSystem, transform, visibleLightDebugColor, occludedLightDebugColor, false);
					}
				}
			}

			// global participating medium
			if (GlobalParticipatingMediumComponent *gpmc = nullptr; beginComponent(entityRegistry, entity, "Global Participating Medium", gpmc))
			{
				ImGui::ColorEdit3("Albedo", &gpmc->m_albedo[0]);
				ImGui::DragFloat("Extinction", &gpmc->m_extinction, 0.001f, 0.0f, FLT_MAX, "%.7f");
				ImGui::DragFloat("Phase", &gpmc->m_phaseAnisotropy, 0.001f, -0.9f, 0.9f, "%.7f");
				ImGui::ColorEdit3("Emissive Color", &gpmc->m_emissiveColor[0]);
				ImGui::DragFloat("Emissive Intensity", &gpmc->m_emissiveIntensity, 0.001f, 0.0f, FLT_MAX, "%.7f");
				ImGui::Checkbox("Height Fog", &gpmc->m_heightFogEnabled);
				ImGui::DragFloat("Height Fog Start", &gpmc->m_heightFogStart, 0.1f);
				ImGui::DragFloat("Height Fog Falloff", &gpmc->m_heightFogFalloff, 0.1f);
				ImGui::DragFloat("Max Height", &gpmc->m_maxHeight, 0.1f);
			}

			// particle emitter
			if (ParticleEmitterComponent *pec = nullptr; beginComponent(entityRegistry, entity, "Particle Emitter", pec))
			{
				ImGui::InputFloat3("Spawn Direction", &pec->m_direction[0]);
				int particleCount = static_cast<int>(pec->m_particleCount);
				ImGui::InputInt("Particle Count", &particleCount);
				pec->m_particleCount = particleCount;
				ImGui::DragFloat("Particle Lifetime", &pec->m_particleLifetime, 0.1f, 0.01f, 100.0f);
				ImGui::DragFloat("Particle Speed", &pec->m_velocityMagnitude, 0.1f, 0.01f, 100.0f);
				const char *spawnTypeItems[] = { "SPHERE", "CUBE", "DISK" };
				int currentSpawnType = static_cast<int>(pec->m_spawnType);
				if (ImGui::Combo("Spawn Type", &currentSpawnType, spawnTypeItems, IM_ARRAYSIZE(spawnTypeItems)))
				{
					pec->m_spawnType = static_cast<ParticleEmitterComponent::SpawnType>(currentSpawnType);
				}
				ImGui::DragFloat("Spawn Area Size", &pec->m_spawnAreaSize, 0.01f, 0.01f, 100.0f);
				ImGui::DragFloat("Spawn Rate", &pec->m_spawnRate, 1.0f, 0.01f, 100.0f);
				ImGui::DragFloat("Particle Initial Size", &pec->m_particleSize, 0.01f, 0.01f, 10.0f);
				ImGui::DragFloat("Particle Final Size", &pec->m_particleFinalSize, 0.01f, 0.01f, 10.0f);
				ImGui::DragFloat("Rotation", &pec->m_rotation, 0.01f, 0.0f, glm::pi<float>() * 2.0f);
				ImGui::DragFloat("FOM Opacity Mult", &pec->m_FOMOpacityMult, 0.01f, 0.0f, 1.0f);

				if (pec->m_textureHandle.m_handle ? ImGui::ImageButton((ImTextureID)(size_t)pec->m_textureHandle.m_handle, ImVec2(64.0f, 64.0f)) : ImGui::Button("Add Texture"))
				{
					ImGui::OpenPopup("select_particle_texture_popup");
				}
				ImGui::SameLine();
				ImGui::Text("Texture");


				if (ImGui::BeginPopup("select_particle_texture_popup"))
				{
					ImGui::Text("Particle Texture:");
					ImGui::Separator();

					auto &scene = m_engine->getScene();
					Texture2DHandle selectedTexture = {};

					for (auto &texture : scene.m_textures)
					{
						if (ImGui::Selectable(texture.first.c_str(), texture.second.m_handle == pec->m_textureHandle.m_handle))
						{
							selectedTexture = texture.second;
						}
						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::Image((ImTextureID)(size_t)texture.second.m_handle, ImVec2(128.0f, 128.0f));
							ImGui::EndTooltip();
						}
					}

					if (selectedTexture.m_handle != 0)
					{
						// unselect texture
						if (selectedTexture.m_handle == pec->m_textureHandle.m_handle)
						{
							pec->m_textureHandle = {};
						}
						// select different texture
						else
						{
							pec->m_textureHandle = selectedTexture;
						}
					}

					ImGui::EndPopup();
				}
			}

			// local reflection probe
			if (LocalReflectionProbeComponent *lrpc = nullptr; beginComponent(entityRegistry, entity, "Local Reflection Probe", lrpc))
			{
				ImGui::DragFloat3("Capture Offset", &lrpc->m_captureOffset[0], 0.1f);
				ImGui::DragFloat("Transition Distance", &lrpc->m_transitionDistance, 0.05f);

				// draw probe debug geometry
				if (tc && entityRegistry.has<RenderableComponent>(entity))
				{
					auto &renderSystem = m_engine->getRenderSystem();
					glm::mat4 transform = glm::translate(tc->m_position) * glm::mat4_cast(tc->m_orientation) * glm::scale(tc->m_scale);
					drawBoxGeometry(renderSystem, transform, visibleLightDebugColor, visibleLightDebugColor, true);
				}
			}

			// renderable
			if (RenderableComponent *rc = nullptr; beginComponent(entityRegistry, entity, "Renderable", rc))
			{
				// empty
			}
		}
		ImGui::EndChild();
	}

	ImGui::End();

	if (showGuizmo)
	{
		ImGuizmo::OPERATION operation = static_cast<ImGuizmo::OPERATION>(m_translateRotateScaleMode);
		auto &tc = entityRegistry.get<TransformationComponent>(entity);


		glm::mat4 transform = glm::translate(tc.m_position) * glm::mat4_cast(tc.m_orientation) * glm::scale(glm::vec3(tc.m_scale));

		auto camC = entityRegistry.get<VEngine::CameraComponent>(editorCameraEntity);
		Camera camera(entityRegistry.get<VEngine::TransformationComponent>(editorCameraEntity), camC);

		auto viewMatrix = camera.getViewMatrix();
		auto projMatrix = glm::perspective(camC.m_fovy, camC.m_aspectRatio, camC.m_near, camC.m_far);

		auto *editorContext = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext(gameContext);
		auto &io = ImGui::GetIO();

		ImGuizmo::SetRect((float)0.0f, (float)0.0f, (float)io.DisplaySize.x, (float)io.DisplaySize.y);

		ImGuizmo::Manipulate((float *)&viewMatrix, (float *)&projMatrix, operation, m_localTransformMode ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD, (float *)&transform);

		ImGui::SetCurrentContext(editorContext);

		glm::vec3 position;
		glm::vec3 eulerAngles;
		glm::vec3 scale;
		ImGuizmo::DecomposeMatrixToComponents((float *)&transform, (float *)&position, (float *)&eulerAngles, (float *)&scale);

		switch (operation)
		{
		case ImGuizmo::TRANSLATE:
			tc.m_position = position;
			break;
		case ImGuizmo::ROTATE:
			tc.m_orientation = glm::quat(glm::radians(eulerAngles));
			break;
		case ImGuizmo::SCALE:
			tc.m_scale = scale;
			break;
		case ImGuizmo::BOUNDS:
			break;
		default:
			break;
		}
	}
}

void VEditor::EntityDetailWindow::setVisible(bool visible)
{
	m_visible = visible;
}

bool VEditor::EntityDetailWindow::isVisible() const
{
	return m_visible;
}
