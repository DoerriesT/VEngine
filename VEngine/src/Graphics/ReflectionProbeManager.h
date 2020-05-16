#pragma once
#include <entt/entity/registry.hpp>
#include <vector>
#include <glm/mat4x4.hpp>
#include "LightData.h"

namespace VEngine
{
	struct ViewRenderList;
	struct FrustumCullData;
	struct LightData;
	struct CommonRenderData;

	class ReflectionProbeManager
	{
	public:
		explicit ReflectionProbeManager(entt::registry &entityRegistry);
		void update(const CommonRenderData &commonData,
			LightData &lightData,
			std::vector<ViewRenderList> &renderLists,
			std::vector<FrustumCullData> &frustumCullData,
			glm::mat4 *&probeMatrices,
			uint32_t *&probeRenderIndices,
			uint32_t &probeRenderCount,
			uint32_t *&probeRelightIndices,
			uint32_t &probeRelightCount,
			glm::vec3 &probeShadowMapCenter, 
			float &probeShadowMapRadius);
		const ReflectionProbeRelightData *getRelightData() const;

	private:
		entt::registry &m_entityRegistry;
		uint32_t m_renderProbeIndex;
		uint32_t m_relightProbeIndex;
		std::vector<glm::mat4> m_probeMatrices;
		std::vector<uint32_t> m_freeCacheSlots;
		std::vector<ReflectionProbeRelightData> m_probeRelightData;

		void addInternalComponent(entt::registry &registry, entt::entity entity);
		void removeInternalComponent(entt::registry &registry, entt::entity entity);
	};
}