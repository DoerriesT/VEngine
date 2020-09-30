#pragma once
#include <IGameLogic.h>
#include <entt/entity/registry.hpp>
#include "GUI.h"
#include <vector>

struct ProfilingData
{
	double fomDirDepth;
	double fomDir;
	double fomLocal;

	// merged
	double scatter;

	// split
	double vbufferOnly;
	double scatterOnly;

	// merged cb
	double scatterCB;

	// split cb
	double vbufferOnlyCB;
	double scatterOnlyCB;

	double filter;
	double integrate;

	double depthDownsample;
	double raymarch;
	double apply;
};

enum class ProfilingStage
{
	DEFAULT, MERGED, CHECKER_BOARD, CHECKER_BOARD_MERGED, 
	DEFAULT_SHADOWS, MERGED_SHADOWS, CHECKER_BOARD_SHADOWS, CHECKER_BOARD_MERGED_SHADOWS
};

class App : public VEngine::IGameLogic
{
public:
	void initialize(VEngine::Engine *engine) override;
	void update(float timeDelta) override;
	void shutdown() override;

private:
	VEngine::Engine *m_engine;
	entt::entity m_cameraEntity;
	GUI *m_gui;
	bool m_currentlyProfiling = false;
	uint32_t m_profiledFrames = 0;
	ProfilingStage m_currentProfilingStage;

	ProfilingData m_profilingData[8];

	bool m_showTerrain = false;
	entt::entity m_planeEntity;
	entt::entity m_terrainEntity;
	std::vector<entt::entity> m_randomLights;
	int m_activeRandomLightCount = 0;

	void profile();
};