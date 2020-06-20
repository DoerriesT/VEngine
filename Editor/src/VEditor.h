#pragma once
#include <IGameLogic.h>
#include <entt/entity/registry.hpp>

struct ImGuiContext;

namespace VEngine
{
	class ImGuiInputAdapter;
	class UserInput;
}

namespace VEditor
{
	class EntityDetailWindow;
	class EntityWindow;
	class AssetBrowserWindow;

	class VEditor : public VEngine::IGameLogic
	{
	public:
		explicit VEditor(VEngine::IGameLogic &gameLogic);
		void initialize(VEngine::Engine *engine) override;
		void update(float timeDelta) override;
		void shutdown() override;

	private:
		IGameLogic &m_gameLogic;
		VEngine::Engine *m_engine;
		entt::entity m_editorCameraEntity;
		entt::entity m_lastGameCameraEntity = entt::null;
		ImGuiContext *m_editorImGuiContext;
		VEngine::ImGuiInputAdapter *m_imguiInputAdapter;
		VEngine::UserInput *m_userInput;
		EntityDetailWindow *m_entityDetailWindow;
		EntityWindow *m_entityWindow;
		AssetBrowserWindow *m_assetBrowserWindow;
		bool m_editorMode = true;
		bool m_newEditorMode = true;
	};
}