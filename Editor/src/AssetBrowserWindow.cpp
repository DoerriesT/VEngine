#include "AssetBrowserWindow.h"
#include <Graphics/imgui/imgui.h>
#include <Engine.h>

using namespace VEngine;

VEditor::AssetBrowserWindow::AssetBrowserWindow(VEngine::Engine *engine)
	:m_engine(engine),
	m_selectedTexture2D(),
	m_selectedTexture2DName(),
	m_visible()
{
}

void VEditor::AssetBrowserWindow::draw()
{
	ImGui::Begin("Assets");

	auto &scene = m_engine->getScene();

	Texture2DHandle selectedTexture = {};

	// left
	ImGui::BeginChild("left pane", ImVec2(ImGui::GetWindowSize().x / 3, 0), true);
	for (auto &texture : scene.m_textures)
	{
		if (ImGui::Selectable(texture.first.c_str(), texture.second.m_handle == m_selectedTexture2D.m_handle))
		{
			selectedTexture = texture.second;
			m_selectedTexture2DName = texture.first.c_str();
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();

	if (selectedTexture.m_handle != 0)
	{
		// unselect texture
		if (selectedTexture.m_handle == m_selectedTexture2D.m_handle)
		{
			m_selectedTexture2D = {};
			m_selectedTexture2DName = nullptr;
		}
		// select different texture
		else
		{
			m_selectedTexture2D = selectedTexture;
		}
	}

	// right
	ImGui::BeginGroup();
	ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us
	ImGui::Text(m_selectedTexture2DName ? m_selectedTexture2DName : "No Texture selected");
	ImGui::Separator();
	if (m_selectedTexture2D.m_handle != 0)
	{
		ImGui::Image((ImTextureID)(size_t)m_selectedTexture2D.m_handle, ImVec2(256.0f, 256.0f));
	}
	ImGui::EndChild();
	ImGui::EndGroup();

	ImGui::End();
}

void VEditor::AssetBrowserWindow::setVisible(bool visible)
{
	m_visible = visible;
}

bool VEditor::AssetBrowserWindow::isVisible() const
{
	return m_visible;
}
