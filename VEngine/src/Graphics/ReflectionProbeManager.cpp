#include "ReflectionProbeManager.h"
#include "Components/TransformationComponent.h"
#include "Components/ReflectionProbeComponent.h"
#include "Components/RenderableComponent.h"
#include <glm/ext.hpp>
#include "LightData.h"
#include "ViewRenderList.h"
#include "FrustumCullData.h"
#include "LightData.h"
#include "RendererConsts.h"
#include "RenderData.h"

namespace
{
	struct ReflectionProbeInternalDataComponent
	{
		uint32_t m_cacheSlot = UINT32_MAX;
		uint32_t m_lastLit = UINT32_MAX;
		bool m_rendered = false;
	};
}

VEngine::ReflectionProbeManager::ReflectionProbeManager(entt::registry &entityRegistry)
	:m_entityRegistry(entityRegistry)
{
	m_entityRegistry.on_assign<LocalReflectionProbeComponent>().connect<&ReflectionProbeManager::addInternalComponent>(this);
	m_entityRegistry.on_remove<LocalReflectionProbeComponent>().connect<&ReflectionProbeManager::removeInternalComponent>(this);

	m_freeCacheSlots.reserve(RendererConsts::REFLECTION_PROBE_CACHE_SIZE);
	for (uint32_t i = RendererConsts::REFLECTION_PROBE_CACHE_SIZE; i > 0; --i)
	{
		m_freeCacheSlots.push_back(i - 1);
	}
	m_probeRelightData.resize(RendererConsts::REFLECTION_PROBE_CACHE_SIZE);
}

void VEngine::ReflectionProbeManager::update(const CommonRenderData &commonData,
	LightData &lightData,
	std::vector<ViewRenderList> &renderLists,
	std::vector<FrustumCullData> &frustumCullData,
	glm::mat4 *&probeMatrices,
	uint32_t *&probeRenderIndices,
	uint32_t &probeRenderCount,
	uint32_t *&probeRelightIndices,
	uint32_t &probeRelightCount,
	glm::vec3 &probeShadowMapCenter,
	float &probeShadowMapRadius)
{
	m_probeMatrices.clear();
	probeRenderCount = 0;

	auto view = m_entityRegistry.view<TransformationComponent, LocalReflectionProbeComponent, ReflectionProbeInternalDataComponent, RenderableComponent>();

	// update cache
	{
		struct ProbeSortData
		{
			float m_cameraDistance2;
			glm::vec3 m_capturePosition;
			ReflectionProbeInternalDataComponent *m_internalData;
		};

		std::vector<ProbeSortData> sortData;

		view.each([&](entt::entity entity, TransformationComponent &transformationComponent, LocalReflectionProbeComponent &probeComponent, ReflectionProbeInternalDataComponent &internalComponent, RenderableComponent &)
			{
				internalComponent.m_lastLit = (internalComponent.m_lastLit == UINT32_MAX) ? UINT32_MAX : internalComponent.m_lastLit + 1;

				ProbeSortData probeSortData{};
				probeSortData.m_cameraDistance2 = glm::length2(glm::vec3(commonData.m_cameraPosition) - transformationComponent.m_position);
				probeSortData.m_capturePosition = transformationComponent.m_position + probeComponent.m_captureOffset;
				probeSortData.m_internalData = &internalComponent;

				sortData.push_back(probeSortData);
			});

		// sort by distance to camera
		std::sort(sortData.begin(), sortData.end(), [&](const ProbeSortData &lhs, const ProbeSortData &rhs)
			{
				return lhs.m_cameraDistance2 < lhs.m_cameraDistance2;
			});

		// remove far away probes from cache
		for (size_t i = RendererConsts::REFLECTION_PROBE_CACHE_SIZE; i < sortData.size(); ++i)
		{
			auto &data = sortData[i];
			const uint32_t cacheSlot = data.m_internalData->m_cacheSlot;

			// probe occupies cache slot
			if (cacheSlot != UINT32_MAX)
			{
				// free slot in cache
				m_freeCacheSlots.push_back(cacheSlot);
				// reset internal data
				*data.m_internalData = { UINT32_MAX, UINT32_MAX, false };
			}
		}

		const float invFurthestDistance2 = 1.0f / (sortData.empty() ? 0.0f : sortData[std::min(sortData.size(), (size_t)RendererConsts::REFLECTION_PROBE_CACHE_SIZE) - 1].m_cameraDistance2);
		uint32_t relightProbeIndex = UINT32_MAX;
		uint32_t renderProbeIndex = UINT32_MAX;
		float bestRelightScore = -1.0f;

		// add new close probes to the cache and find probe to relight
		for (size_t i = 0; i < sortData.size() && i < RendererConsts::REFLECTION_PROBE_CACHE_SIZE; ++i)
		{
			auto &data = sortData[i];
			const uint32_t cacheSlot = data.m_internalData->m_cacheSlot;

			// probe not yet in cache
			if (cacheSlot == UINT32_MAX)
			{
				// allocate cache slot for probe
				assert(!m_freeCacheSlots.empty());
				const uint32_t slot = m_freeCacheSlots.back();
				data.m_internalData->m_cacheSlot = slot;
				m_probeRelightData[slot] = { data.m_capturePosition, 0.1f, 20.0f }; // TODO: compute proper near/far planes
				m_freeCacheSlots.pop_back();
			}

			// evaluate relighting score
			const float distanceScore = (1.0f - (data.m_cameraDistance2 * invFurthestDistance2)) * 0.5f + 0.5f;
			const float timeScore = static_cast<float>((data.m_internalData->m_lastLit == UINT32_MAX) ? RendererConsts::REFLECTION_PROBE_CACHE_SIZE : data.m_internalData->m_lastLit);
			const float relightScore = distanceScore * timeScore;

			if (relightScore > bestRelightScore)
			{
				bestRelightScore = relightScore;
				relightProbeIndex = static_cast<uint32_t>(i);
			}

			// render gbuffer of closest unrendered probe
			if (renderProbeIndex == UINT32_MAX && !data.m_internalData->m_rendered)
			{
				renderProbeIndex = static_cast<uint32_t>(i);
			}
		}

		// if we need to render a probe, we also light it directly
		relightProbeIndex = renderProbeIndex != UINT32_MAX ? renderProbeIndex : relightProbeIndex;

		// reset the last_lit counter
		if (relightProbeIndex != UINT32_MAX)
		{
			sortData[relightProbeIndex].m_internalData->m_lastLit = 0;
		}

		// schedule rendering a probe gbuffer
		if (renderProbeIndex != UINT32_MAX)
		{
			auto &data = sortData[renderProbeIndex];
			data.m_internalData->m_rendered = true;
			++probeRenderCount;

			const glm::mat4 vulkanCorrection =
			{
				{ 1.0f, 0.0f, 0.0f, 0.0f },
				{ 0.0f, -1.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.5f, 0.0f },
				{ 0.0f, 0.0f, 0.5f, 1.0f }
			};

			glm::mat4 projection = vulkanCorrection * glm::perspectiveLH(glm::radians(90.0f), 1.0f, 0.1f, 20.0f);
			glm::mat4 matrices[6];
			matrices[0] = projection * glm::lookAtLH(data.m_capturePosition, data.m_capturePosition + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			matrices[1] = projection * glm::lookAtLH(data.m_capturePosition, data.m_capturePosition + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			matrices[2] = projection * glm::lookAtLH(data.m_capturePosition, data.m_capturePosition + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
			matrices[3] = projection * glm::lookAtLH(data.m_capturePosition, data.m_capturePosition + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			matrices[4] = projection * glm::lookAtLH(data.m_capturePosition, data.m_capturePosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			matrices[5] = projection * glm::lookAtLH(data.m_capturePosition, data.m_capturePosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			m_probeMatrices.reserve(m_probeMatrices.size() + 6);

			for (size_t i = 0; i < 6; ++i)
			{
				// extract view frustum plane equations from matrix
				{
					uint32_t contentTypeFlags = FrustumCullData::STATIC_OPAQUE_CONTENT_TYPE_BIT | FrustumCullData::STATIC_ALPHA_TESTED_CONTENT_TYPE_BIT;
					FrustumCullData cullData(matrices[i], 5, static_cast<uint32_t>(renderLists.size()), contentTypeFlags, 20.0f);

					frustumCullData.push_back(cullData);
					renderLists.push_back({});
				}
				m_probeMatrices.push_back(matrices[i]);
			}
		}

		probeShadowMapCenter = sortData.empty() ? glm::vec3(0.0f) : sortData[relightProbeIndex].m_capturePosition;
		probeShadowMapRadius = 50.0f; // TODO

		m_renderProbeIndex = renderProbeIndex;
		m_relightProbeIndex = relightProbeIndex;

		probeMatrices = m_probeMatrices.data();
		probeRenderIndices = &m_renderProbeIndex;
		probeRelightIndices = &m_relightProbeIndex;
		probeRelightCount = relightProbeIndex != UINT32_MAX ? 1 : 0;
	}

	// build probe list for frame
	{
		struct ProbeDepthSortData
		{
			float m_zDist;
			float m_closest;
			float m_furthest;
		};

		std::vector<ProbeDepthSortData> depthSortData;

		// build list of probes for the current frame
		view.each([&](TransformationComponent &transformationComponent, LocalReflectionProbeComponent &probeComponent, ReflectionProbeInternalDataComponent &internalComponent, RenderableComponent &)
			{
				// only add cached and rendered probes to the list. (every rendered probe is guaranteed to have a lit result in the cache)
				if (internalComponent.m_cacheSlot != UINT32_MAX && internalComponent.m_rendered)
				{
					const float radius = glm::length(transformationComponent.m_scale);
					glm::vec4 boundingSphere = glm::vec4(transformationComponent.m_position, radius);

					// frustum cull
					for (size_t i = 0; i < frustumCullData[0].m_planeCount; ++i)
					{
						if (glm::dot(glm::vec4(glm::vec3(boundingSphere), 1.0f), frustumCullData[0].m_planes[i]) <= -boundingSphere.w)
						{
							return;
						}
					}

					glm::vec3 probePosition = transformationComponent.m_position + probeComponent.m_captureOffset;

					glm::mat4 worldToLocalTransposed =
						glm::transpose(glm::scale(1.0f / transformationComponent.m_scale)
							* glm::mat4_cast(glm::inverse(transformationComponent.m_orientation))
							* glm::translate(-transformationComponent.m_position));

					LocalReflectionProbe probe{};
					probe.m_worldToLocal0 = worldToLocalTransposed[0];
					probe.m_worldToLocal1 = worldToLocalTransposed[1];
					probe.m_worldToLocal2 = worldToLocalTransposed[2];
					probe.m_capturePosition = probePosition;
					probe.m_arraySlot = internalComponent.m_cacheSlot;

					ProbeDepthSortData probeSortData{};
					probeSortData.m_zDist = glm::dot(glm::vec4(commonData.m_viewMatrix[0][2], commonData.m_viewMatrix[1][2], commonData.m_viewMatrix[2][2], commonData.m_viewMatrix[3][2]), glm::vec4(transformationComponent.m_position, 1.0f));
					probeSortData.m_closest = probeSortData.m_zDist + radius;
					probeSortData.m_furthest = probeSortData.m_zDist - radius;

					depthSortData.push_back(probeSortData);
					lightData.m_localReflectionProbes.push_back(probe);
					lightData.m_localReflectionProbeTransforms.push_back(glm::translate(transformationComponent.m_position) * glm::mat4_cast(transformationComponent.m_orientation) * glm::scale(transformationComponent.m_scale));
					lightData.m_localReflectionProbeOrder.push_back(static_cast<uint32_t>(lightData.m_localReflectionProbeOrder.size()));
				}
			});

		// sort by distance to camera
		std::sort(lightData.m_localReflectionProbeOrder.begin(), lightData.m_localReflectionProbeOrder.end(), [&](const uint32_t &lhs, const uint32_t &rhs)
			{
				return -depthSortData[lhs].m_zDist < -depthSortData[rhs].m_zDist;
			});

		// clear bins
		for (size_t i = 0; i < lightData.m_localReflectionProbeDepthBins.size(); ++i)
		{
			constexpr uint32_t emptyBin = ((~0u & 0xFFFFu) << 16u);
			lightData.m_localReflectionProbeDepthBins[i] = emptyBin;
		}

		// assign probes to bins
		for (size_t i = 0; i < depthSortData.size(); ++i)
		{
			const auto &data = depthSortData[lightData.m_localReflectionProbeOrder[i]];
			float nearestPoint = -data.m_closest;
			float furthestPoint = -data.m_furthest;

			size_t minBin = glm::min(static_cast<size_t>(glm::max(nearestPoint / RendererConsts::Z_BIN_DEPTH, 0.0f)), size_t(RendererConsts::Z_BINS - 1));
			size_t maxBin = glm::min(static_cast<size_t>(glm::max(furthestPoint / RendererConsts::Z_BIN_DEPTH, 0.0f)), size_t(RendererConsts::Z_BINS - 1));

			for (size_t j = minBin; j <= maxBin; ++j)
			{
				uint32_t &val = lightData.m_localReflectionProbeDepthBins[j];
				uint32_t minIndex = (val & 0xFFFF0000) >> 16;
				uint32_t maxIndex = val & 0xFFFF;
				minIndex = std::min(minIndex, static_cast<uint32_t>(i));
				maxIndex = std::max(maxIndex, static_cast<uint32_t>(i));
				val = ((minIndex & 0xFFFF) << 16) | (maxIndex & 0xFFFF);
			}
		}
	}
}

const VEngine::ReflectionProbeRelightData *VEngine::ReflectionProbeManager::getRelightData() const
{
	return m_probeRelightData.data();
}

void VEngine::ReflectionProbeManager::addInternalComponent(entt::registry &registry, entt::entity entity)
{
	registry.assign<ReflectionProbeInternalDataComponent>(entity);
}

void VEngine::ReflectionProbeManager::removeInternalComponent(entt::registry &registry, entt::entity entity)
{
	auto data = registry.get<ReflectionProbeInternalDataComponent>(entity);
	// free cache slot before destroying the probe
	if (data.m_cacheSlot != UINT32_MAX)
	{
		m_freeCacheSlots.push_back(data.m_cacheSlot);
	}
	registry.remove<ReflectionProbeInternalDataComponent>(entity);
}
