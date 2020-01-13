#include "EntityDetailsWindow.h"
#include "Graphics/imgui/imgui.h"
#include "Graphics/imgui/ImGuizmo.h"
#include "Components/TransformationComponent.h"
#include "Components/RenderableComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine.h"
#include "Graphics/Camera/Camera.h"
#include <glm/ext.hpp>

VEngine::EntityDetailsWindow::EntityDetailsWindow(Engine *engine)
	:m_engine(engine),
	m_translateRotateScaleMode(0),
	m_localTransformMode(false)
{
}

VEngine::EntityDetailsWindow::~EntityDetailsWindow()
{
}

void VEngine::EntityDetailsWindow::draw(uint32_t entity, uint32_t editorCameraEntity)
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

	// transformation
	if (auto tc = entityRegistry.try_get<TransformationComponent>(entity); tc != nullptr && ImGui::CollapsingHeader("Transformation"))
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
		ImGui::DragFloat("Scale", &tc->m_scale);

		ImGui::RadioButton("Translate", &m_translateRotateScaleMode, 0); ImGui::SameLine();
		ImGui::RadioButton("Rotate", &m_translateRotateScaleMode, 1); ImGui::SameLine();
		ImGui::RadioButton("Scale", &m_translateRotateScaleMode, 2);
		ImGui::Checkbox("Local Transform Mode", &m_localTransformMode);
	}

	// directional light
	if (auto dlc = entityRegistry.try_get<DirectionalLightComponent>(entity); dlc != nullptr && ImGui::CollapsingHeader("Directional Light"))
	{
		ImGui::ColorPicker3("Color", &dlc->m_color[0]);
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
	}

	// point light
	if (auto plc = entityRegistry.try_get<PointLightComponent>(entity); plc != nullptr && ImGui::CollapsingHeader("Point Light"))
	{
		ImGui::ColorPicker3("Color", &plc->m_color[0]);
		ImGui::DragFloat("Luminous Power", &plc->m_luminousPower, 1.0f, 0.0f, 130000.0f);
		ImGui::DragFloat("Radius", &plc->m_radius, 0.05f, 0.0f, 128.0f);
	}

	// point light
	if (auto slc = entityRegistry.try_get<SpotLightComponent>(entity); slc != nullptr && ImGui::CollapsingHeader("Spot Light"))
	{
		ImGui::ColorPicker3("Color", &slc->m_color[0]);
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
	}

	// camera
	if (auto cc = entityRegistry.try_get<CameraComponent>(entity); cc != nullptr && ImGui::CollapsingHeader("Camera"))
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
	}

	// renderable
	if (entityRegistry.try_get<RenderableComponent>(entity) != nullptr)
	{
		ImGui::CollapsingHeader("Renderable");
	}

	ImGui::End();

	if (showGuizmo)
	{
		ImGuizmo::OPERATION operation = static_cast<ImGuizmo::OPERATION>(m_translateRotateScaleMode);
		auto &tc = entityRegistry.get<TransformationComponent>(entity);
		auto &io = ImGui::GetIO();

		glm::mat4 transform;
		switch (operation)
		{
		case ImGuizmo::TRANSLATE:
			transform = glm::translate(tc.m_position);
			break;
		case ImGuizmo::ROTATE:
			transform = glm::mat4_cast(tc.m_orientation);
			break;
		case ImGuizmo::SCALE:
			transform = glm::scale(glm::vec3(tc.m_scale));
			break;
		case ImGuizmo::BOUNDS:
			break;
		default:
			break;
		}

		auto camC = entityRegistry.get<VEngine::CameraComponent>(editorCameraEntity);
		Camera camera(entityRegistry.get<VEngine::TransformationComponent>(editorCameraEntity), camC);

		auto viewMatrix = camera.getViewMatrix();
		auto projMatrix = glm::perspective(camC.m_fovy, camC.m_aspectRatio, camC.m_near, camC.m_far);

		ImGuizmo::SetRect((float)0.0f, (float)0.0f, (float)io.DisplaySize.x, (float)io.DisplaySize.y);

		ImGuizmo::Manipulate((float *)&viewMatrix, (float *)&projMatrix, operation, m_localTransformMode ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD, (float *)&transform);
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
			tc.m_scale = tc.m_scale != scale.x ? scale.x : tc.m_scale != scale.y ? scale.y : scale.z;
			break;
		case ImGuizmo::BOUNDS:
			break;
		default:
			break;
		}
	}
}
