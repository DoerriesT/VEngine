#pragma once
#include <entt/entity/registry.hpp>
#include <Engine.h>

class GUI
{
public:
	explicit GUI(VEngine::Engine *engine, entt::entity cameraEntity);
	void draw();

private:
	VEngine::Engine *m_engine;
	entt::entity m_cameraEntity;
	entt::entity m_entity;
	entt::entity m_lastDisplayedEntity;
	int m_translateRotateScaleMode;
	bool m_localTransformMode;
};