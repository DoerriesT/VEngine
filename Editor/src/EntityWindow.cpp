#include "EntityWindow.h"
#include <Graphics/imgui/imgui.h>
#include <Engine.h>

VEditor::EntityWindow::EntityWindow(VEngine::Engine *engine)
	:m_engine(engine),
	m_selectedEntity(entt::null),
	m_visible()
{
}

void VEditor::EntityWindow::draw()
{
	ImGui::Begin("Entities");

	auto &scene = m_engine->getScene();

	{
		ImGuiTreeNodeFlags treeBaseFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

		entt::entity selected = entt::null;
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
			++entityCount;
		}

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
