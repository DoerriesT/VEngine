#pragma once
#include <IGameLogic.h>
#include <entt/entity/registry.hpp>

class App : public VEngine::IGameLogic
{
public:
	void initialize(VEngine::Engine *engine) override;
	void update(float timeDelta) override;
	void shutdown() override;

private:
	VEngine::Engine *m_engine;
	entt::entity m_cameraEntity;
};