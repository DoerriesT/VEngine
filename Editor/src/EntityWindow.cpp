#include "EntityWindow.h"
#include <Graphics/imgui/imgui.h>
#include <Engine.h>
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
#include <Utility/ContainerUtility.h>
#include <Input/InputTokens.h>

using namespace VEngine;

VEditor::EntityWindow::EntityWindow(VEngine::Engine *engine)
	:m_engine(engine),
	m_selectedEntity(entt::null),
	m_toDeleteEntity(entt::null),
	m_visible()
{
}

void VEditor::EntityWindow::draw()
{
	ImGui::Begin("Entities");

	auto &scene = m_engine->getScene();
	auto &entityRegistry = m_engine->getEntityRegistry();

	if (ImGui::Button("Add Entity"))
	{
		ImGui::OpenPopup("add_entity_popup");
	}

	if (ImGui::BeginPopup("add_entity_popup"))
	{
		if (ImGui::Selectable("Empty Entity"))
		{
			auto entity = entityRegistry.create();
			scene.m_entities.push_back({ "Entity", entity });
			m_selectedEntity = entity;
		}
		if (ImGui::Selectable("Directional Light"))
		{
			auto entity = entityRegistry.create();
			entityRegistry.assign<TransformationComponent>(entity);
			entityRegistry.assign<DirectionalLightComponent>(entity);
			entityRegistry.assign<RenderableComponent>(entity);
			scene.m_entities.push_back({ "Directional Light", entity });
			m_selectedEntity = entity;
		}
		if (ImGui::Selectable("Point Light"))
		{
			auto entity = entityRegistry.create();
			entityRegistry.assign<TransformationComponent>(entity);
			entityRegistry.assign<PointLightComponent>(entity);
			entityRegistry.assign<RenderableComponent>(entity);
			scene.m_entities.push_back({ "Point Light", entity });
			m_selectedEntity = entity;
		}
		if (ImGui::Selectable("Spot Light"))
		{
			auto entity = entityRegistry.create();
			entityRegistry.assign<TransformationComponent>(entity);
			entityRegistry.assign<SpotLightComponent>(entity);
			entityRegistry.assign<RenderableComponent>(entity);
			scene.m_entities.push_back({ "Spot Light", entity });
			m_selectedEntity = entity;
		}

		ImGui::EndPopup();
	}

	ImGui::Separator();

	{
		entt::entity selected = entt::null;

		ImGui::BeginChild("Child1");
		{
			ImGuiTreeNodeFlags treeBaseFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

			uint32_t entityCount = 0;
			for (auto &entity : scene.m_entities)
			{
				ImGuiTreeNodeFlags nodeFlags = treeBaseFlags;

				if (entity.second == m_selectedEntity)
				{
					nodeFlags |= ImGuiTreeNodeFlags_Selected;
				}

				nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; // ImGuiTreeNodeFlags_Bullet
				ImGui::TreeNodeEx((void *)(intptr_t)entityCount, nodeFlags, entity.first.c_str());
				if (ImGui::IsItemClicked())
				{
					selected = entity.second;
				}
				ImGui::PushID(&entity);
				if (ImGui::BeginPopupContextItem("delete_entity_context_menu"))
				{
					if (ImGui::Selectable("Delete"))
					{
						m_toDeleteEntity = entity.second;
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
				++entityCount;
			}
		}
		ImGui::EndChild();

		if (selected != entt::null)
		{
			// unselect entity
			if (selected == m_selectedEntity)
			{
				m_selectedEntity = entt::null;
			}
			// select different entity
			else
			{
				m_selectedEntity = selected;
			}
		}

		// delete entity by pressing delete key
		if (m_selectedEntity != entt::null && m_toDeleteEntity == entt::null)
		{
			if (ImGui::IsKeyPressed((int)InputKey::DELETE, false))
			m_toDeleteEntity = m_selectedEntity;
		}

		if (m_toDeleteEntity != entt::null)
		{
			ImGui::OpenPopup("Delete Entity?");
		}

		if (ImGui::BeginPopupModal("Delete Entity?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Delete Entity.\nThis operation cannot be undone!\n\n");
			ImGui::Separator();

			if (ImGui::Button("OK", ImVec2(120, 0)))
			{
				entityRegistry.destroy(m_toDeleteEntity);
				size_t index = 0;
				bool found = true;
				for (; index < scene.m_entities.size(); ++index)
				{
					if (scene.m_entities[index].second == m_toDeleteEntity)
					{
						found = true;
						break;
					}
				}
				ContainerUtility::remove(scene.m_entities, scene.m_entities[index]);
				m_selectedEntity = m_selectedEntity == m_toDeleteEntity ? entt::null : m_selectedEntity;
				m_toDeleteEntity = entt::null;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				m_toDeleteEntity = entt::null;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	ImGui::End();
}

void VEditor::EntityWindow::setVisible(bool visible)
{
	m_visible = visible;
}

bool VEditor::EntityWindow::isVisible() const
{
	return m_visible;
}

entt::entity VEditor::EntityWindow::getSelectedEntity() const
{
	return m_selectedEntity;
}
