#pragma once
#include <IGameLogic.h>
#include <entt/entity/registry.hpp>

namespace VEditor
{
	class EntityDetailWindow;

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
		EntityDetailWindow *m_entityDetailWindow;
	};
}