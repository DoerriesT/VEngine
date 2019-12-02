#include "RenderSystem.h"
#include "Components/MeshComponent.h"
#include "Components/TransformationComponent.h"
#include "Components/RenderableComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/CameraComponent.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "GlobalVar.h"
#include <glm/ext.hpp>
#include <glm/vec3.hpp>
#include <algorithm>
#include "Utility/Utility.h"
#include "Mesh.h"
#include "ViewRenderList.h"

VEngine::RenderSystem::RenderSystem(entt::registry &entityRegistry, void *windowHandle)
	:m_entityRegistry(entityRegistry),
	m_cameraEntity(entt::null),
	m_materialBatchAssignment(std::make_unique<uint8_t[]>(RendererConsts::MAX_MATERIALS)),
	m_aabbs(std::make_unique<AxisAlignedBoundingBox[]>(RendererConsts::MAX_SUB_MESHES)),
	m_boundingSpheres(std::make_unique<glm::vec4[]>(RendererConsts::MAX_SUB_MESHES))
{
	for (size_t i = 0; i < RendererConsts::MAX_TAA_HALTON_SAMPLES; ++i)
	{
		m_haltonX[i] = Utility::halton(i + 1, 2) * 2.0f - 1.0f;
		m_haltonY[i] = Utility::halton(i + 1, 3) * 2.0f - 1.0f;
	}

	m_renderer = std::make_unique<VKRenderer>(g_windowWidth, g_windowHeight, windowHandle);
	memset(&m_commonRenderData, 0, sizeof(m_commonRenderData));
}

void VEngine::RenderSystem::update(float timeDelta)
{
	m_transformData.clear();
	m_subMeshInstanceData.clear();
	m_shadowMatrices.clear();

	m_lightData.m_directionalLightData.clear();
	m_lightData.m_pointLightData.clear();
	m_lightData.m_spotLightData.clear();

	std::vector<ViewRenderList> renderLists;
	std::vector<SubMeshInstanceData> allInstanceData;

	struct CullingData
	{
		enum
		{
			STATIC_LIGHTS_CONTENT_TYPE_BIT = 1 << 0,
			STATIC_OPAQUE_CONTENT_TYPE_BIT = 1 << 1,
			STATIC_ALPHA_TESTED_CONTENT_TYPE_BIT = 1 << 2,
			STATIC_TRANSPARENT_CONTENT_TYPE_BIT = 1 << 3,
			DYNAMIC_LIGHTS_CONTENT_TYPE_BIT = 1 << 4,
			DYNAMIC_OPAQUE_CONTENT_TYPE_BIT = 1 << 5,
			DYNAMIC_ALPHA_TESTED_CONTENT_TYPE_BIT = 1 << 6,
			DYNAMIC_TRANSPARENT_CONTENT_TYPE_BIT = 1 << 7,
			ALL_CONTENT_TYPE_BIT = (DYNAMIC_TRANSPARENT_CONTENT_TYPE_BIT << 1) - 1,
		};

		glm::vec4 m_planes[6] = {};
		uint32_t m_planeCount;
		uint32_t m_renderListIndex;
		uint32_t m_contentTypeFlags;
		float m_depthRange;

		CullingData() = default;
		explicit CullingData(const glm::mat4 &matrix, uint32_t planeCount, uint32_t renderListIndex, uint32_t flags, float depthRange)
			:m_planeCount(planeCount),
			m_renderListIndex(renderListIndex),
			m_contentTypeFlags(flags),
			m_depthRange(depthRange)
		{
			glm::mat4 proj = glm::transpose(matrix);
			m_planes[0] = (proj[3] + proj[0]);	// left
			m_planes[1] = (proj[3] - proj[0]);	// right
			m_planes[2] = (proj[3] + proj[1]);	// bottom
			m_planes[3] = (proj[3] - proj[1]);	// top
			m_planes[4] = (proj[3] - proj[2]);	// far
			m_planes[5] = (proj[2]);			// near

			for (size_t i = 0; i < m_planeCount; ++i)
			{
				float length = sqrtf(m_planes[i].x * m_planes[i].x + m_planes[i].y * m_planes[i].y + m_planes[i].z * m_planes[i].z);
				m_planes[i] /= length;
			}
		}
	};

	std::vector<CullingData> frustumCullData;

	std::vector<uint64_t> drawCallKeys;

	if (m_cameraEntity != entt::null)
	{
		// update render params
		{
			CameraComponent &cameraComponent = m_entityRegistry.get<CameraComponent>(m_cameraEntity);
			TransformationComponent &transformationComponent = m_entityRegistry.get<TransformationComponent>(m_cameraEntity);

			glm::mat4 viewMatrix = cameraComponent.m_viewMatrix;
			glm::mat4 projectionMatrix = cameraComponent.m_projectionMatrix;
			glm::mat4 jitterMatrix = g_TAAEnabled ?
				glm::translate(glm::vec3(m_haltonX[m_commonRenderData.m_frame % RendererConsts::MAX_TAA_HALTON_SAMPLES] / g_windowWidth,
					m_haltonY[m_commonRenderData.m_frame % RendererConsts::MAX_TAA_HALTON_SAMPLES] / g_windowHeight,
					0.0f))
				: glm::mat4();

			m_commonRenderData.m_time += m_commonRenderData.m_timeDelta;
			m_commonRenderData.m_fovy = cameraComponent.m_fovy;
			m_commonRenderData.m_nearPlane = cameraComponent.m_near;
			m_commonRenderData.m_farPlane = cameraComponent.m_far;
			m_commonRenderData.m_prevViewMatrix = m_commonRenderData.m_viewMatrix;
			m_commonRenderData.m_prevProjectionMatrix = m_commonRenderData.m_projectionMatrix;
			m_commonRenderData.m_prevViewProjectionMatrix = m_commonRenderData.m_viewProjectionMatrix;
			m_commonRenderData.m_prevInvViewMatrix = m_commonRenderData.m_invViewMatrix;
			m_commonRenderData.m_prevInvProjectionMatrix = m_commonRenderData.m_invProjectionMatrix;
			m_commonRenderData.m_prevInvViewProjectionMatrix = m_commonRenderData.m_invViewProjectionMatrix;
			m_commonRenderData.m_prevJitteredProjectionMatrix = m_commonRenderData.m_jitteredProjectionMatrix;
			m_commonRenderData.m_prevJitteredViewProjectionMatrix = m_commonRenderData.m_jitteredViewProjectionMatrix;
			m_commonRenderData.m_prevInvJitteredProjectionMatrix = m_commonRenderData.m_invJitteredProjectionMatrix;
			m_commonRenderData.m_prevInvJitteredViewProjectionMatrix = m_commonRenderData.m_invJitteredViewProjectionMatrix;
			m_commonRenderData.m_viewMatrix = viewMatrix;
			m_commonRenderData.m_projectionMatrix = projectionMatrix;
			m_commonRenderData.m_viewProjectionMatrix = projectionMatrix * viewMatrix;
			m_commonRenderData.m_invViewMatrix = glm::inverse(viewMatrix);
			m_commonRenderData.m_invProjectionMatrix = glm::inverse(projectionMatrix);
			m_commonRenderData.m_invViewProjectionMatrix = glm::inverse(m_commonRenderData.m_viewProjectionMatrix);
			m_commonRenderData.m_jitteredProjectionMatrix = jitterMatrix * m_commonRenderData.m_projectionMatrix;
			m_commonRenderData.m_jitteredViewProjectionMatrix = jitterMatrix * m_commonRenderData.m_viewProjectionMatrix;
			m_commonRenderData.m_invJitteredProjectionMatrix = glm::inverse(m_commonRenderData.m_jitteredProjectionMatrix);
			m_commonRenderData.m_invJitteredViewProjectionMatrix = glm::inverse(m_commonRenderData.m_jitteredViewProjectionMatrix);
			m_commonRenderData.m_cameraPosition = glm::vec4(transformationComponent.m_position, 1.0f);
			m_commonRenderData.m_cameraDirection = -glm::vec4(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2], 1.0f);
			m_commonRenderData.m_width = g_windowWidth;
			m_commonRenderData.m_height = g_windowHeight;
			m_commonRenderData.m_curResIdx = m_commonRenderData.m_frame % RendererConsts::FRAMES_IN_FLIGHT;
			m_commonRenderData.m_prevResIdx = (m_commonRenderData.m_frame + RendererConsts::FRAMES_IN_FLIGHT - 1) % RendererConsts::FRAMES_IN_FLIGHT;
			m_commonRenderData.m_timeDelta = static_cast<float>(timeDelta);
		}

		// extract view frustum plane equations from matrix
		{
			CullingData cullData(m_commonRenderData.m_jitteredViewProjectionMatrix, 6, 0, CullingData::ALL_CONTENT_TYPE_BIT, m_commonRenderData.m_farPlane - m_commonRenderData.m_nearPlane);
			frustumCullData.push_back(cullData);
			renderLists.push_back({});
		}

		// generate light data
		{
			// directional lights
			{
				auto view = m_entityRegistry.view<TransformationComponent, DirectionalLightComponent, RenderableComponent>();

				view.each([&](TransformationComponent &transformationComponent, DirectionalLightComponent &directionalLightComponent, RenderableComponent&)
				{
					assert(directionalLightComponent.m_cascadeCount <= DirectionalLightComponent::MAX_CASCADES);

					const glm::vec3 direction = transformationComponent.m_orientation * glm::vec3(0.0f, 1.0f, 0.0f);

					DirectionalLightData directionalLightData
					{
						glm::vec4(directionalLightComponent.m_color, 1.0f),
						m_commonRenderData.m_viewMatrix * glm::vec4(direction, 0.0f),
						(static_cast<uint32_t>(m_shadowMatrices.size()) << 16) | (directionalLightComponent.m_cascadeCount & 0xFFFF)
					};

					m_lightData.m_directionalLightData.push_back(directionalLightData);

					// calculate shadow matrices
					if (directionalLightComponent.m_shadows)
					{
						glm::mat4 shadowMatrices[DirectionalLightComponent::MAX_CASCADES];

						calculateCascadeViewProjectionMatrices(direction,
							directionalLightComponent.m_maxShadowDistance,
							directionalLightComponent.m_splitLambda,
							2048.0f,
							directionalLightComponent.m_cascadeCount,
							shadowMatrices);

						m_shadowMatrices.reserve(m_shadowMatrices.size() + directionalLightComponent.m_cascadeCount);

						for (size_t i = 0; i < directionalLightComponent.m_cascadeCount; ++i)
						{
							m_shadowMatrices.push_back(shadowMatrices[i]);

							// extract view frustum plane equations from matrix
							{
								uint32_t contentTypeFlags = CullingData::STATIC_OPAQUE_CONTENT_TYPE_BIT
									| CullingData::STATIC_ALPHA_TESTED_CONTENT_TYPE_BIT
									| CullingData::DYNAMIC_OPAQUE_CONTENT_TYPE_BIT
									| CullingData::DYNAMIC_ALPHA_TESTED_CONTENT_TYPE_BIT;
								CullingData cullData(shadowMatrices[i], 5, static_cast<uint32_t>(renderLists.size()), contentTypeFlags, 300.0f);

								frustumCullData.push_back(cullData);
								renderLists.push_back({});
							}
						}
					}
				});
			}

			// point lights
			{
				auto view = m_entityRegistry.view<TransformationComponent, PointLightComponent, RenderableComponent>();

				view.each([&](TransformationComponent &transformationComponent, PointLightComponent &pointLightComponent, RenderableComponent&)
				{
					const float intensity = pointLightComponent.m_luminousPower * (1.0f / (4.0f * glm::pi<float>()));

					PointLightData pointLightData
					{
						glm::vec4(glm::vec3(m_commonRenderData.m_viewMatrix * glm::vec4(transformationComponent.m_position, 1.0f)), pointLightComponent.m_radius),
						glm::vec4(pointLightComponent.m_color * intensity, 1.0f / (pointLightComponent.m_radius * pointLightComponent.m_radius))
					};

					// frustum cull
					for (size_t i = 0; i < frustumCullData[0].m_planeCount; ++i)
					{
						if (glm::dot(glm::vec4(transformationComponent.m_position, 1.0f), frustumCullData[0].m_planes[i]) <= -pointLightComponent.m_radius)
						{
							return;
						}
					}

					m_lightData.m_pointLightData.push_back(pointLightData);
				});

				// sort by distance to camera
				std::sort(m_lightData.m_pointLightData.begin(), m_lightData.m_pointLightData.end(), [](const PointLightData &lhs, const PointLightData &rhs)
				{
					return (-lhs.m_positionRadius.z - lhs.m_positionRadius.w) < (-rhs.m_positionRadius.z - rhs.m_positionRadius.w);
				});

				// clear bins
				for (size_t i = 0; i < m_lightData.m_zBins.size(); ++i)
				{
					const uint32_t emptyBin = ((~0u & 0xFFFFu) << 16u);
					m_lightData.m_zBins[i] = emptyBin;
				}

				// assign lights to bins
				for (size_t i = 0; i < m_lightData.m_pointLightData.size(); ++i)
				{
					glm::vec4 &posRadius = m_lightData.m_pointLightData[i].m_positionRadius;
					float nearestPoint = -posRadius.z - posRadius.w;
					float furthestPoint = -posRadius.z + posRadius.w;

					size_t minBin = glm::min(static_cast<size_t>(glm::max(nearestPoint / RendererConsts::Z_BIN_DEPTH, 0.0f)), size_t(RendererConsts::Z_BINS - 1));
					size_t maxBin = glm::min(static_cast<size_t>(glm::max(furthestPoint / RendererConsts::Z_BIN_DEPTH, 0.0f)), size_t(RendererConsts::Z_BINS - 1));

					for (size_t j = minBin; j <= maxBin; ++j)
					{
						uint32_t &val = m_lightData.m_zBins[j];
						uint32_t minIndex = (val & 0xFFFF0000) >> 16;
						uint32_t maxIndex = val & 0xFFFF;
						minIndex = std::min(minIndex, static_cast<uint32_t>(i));
						maxIndex = std::max(maxIndex, static_cast<uint32_t>(i));
						val = ((minIndex & 0xFFFF) << 16) | (maxIndex & 0xFFFF);
					}
				}
			}
		}

		// update all transformations and generate draw lists
		{
			auto view = m_entityRegistry.view<MeshComponent, TransformationComponent, RenderableComponent>();

			view.each([&](MeshComponent &meshComponent, TransformationComponent &transformationComponent, RenderableComponent&)
			{
				const glm::mat4 rotationMatrix = glm::mat4_cast(transformationComponent.m_orientation);
				const glm::mat4 scaleMatrix = glm::scale(glm::vec3(transformationComponent.m_scale));
				transformationComponent.m_previousTransformation = transformationComponent.m_transformation;
				transformationComponent.m_transformation = glm::translate(transformationComponent.m_position) * rotationMatrix * scaleMatrix;

				const uint32_t transformIndex = static_cast<uint32_t>(m_transformData.size());

				m_transformData.push_back(transformationComponent.m_transformation);

				for (const auto &p : meshComponent.m_subMeshMaterialPairs)
				{
					SubMeshInstanceData instanceData;
					instanceData.m_subMeshIndex = p.first;
					instanceData.m_transformIndex = transformIndex;
					instanceData.m_materialIndex = p.second;

					const bool staticMobility = transformationComponent.m_mobility == TransformationComponent::Mobility::STATIC;
					uint32_t contentTypeMask = 0;

					switch (m_materialBatchAssignment[p.second])
					{
					case 0: // Opaque
						contentTypeMask = staticMobility ? CullingData::STATIC_OPAQUE_CONTENT_TYPE_BIT : CullingData::DYNAMIC_OPAQUE_CONTENT_TYPE_BIT;
						allInstanceData.push_back(instanceData);
						break;
					case 1: // Alpha tested
						contentTypeMask = staticMobility ? CullingData::STATIC_ALPHA_TESTED_CONTENT_TYPE_BIT : CullingData::DYNAMIC_ALPHA_TESTED_CONTENT_TYPE_BIT;
						allInstanceData.push_back(instanceData);
						break;
					case 2: // transparent
						contentTypeMask = staticMobility ? CullingData::STATIC_TRANSPARENT_CONTENT_TYPE_BIT : CullingData::DYNAMIC_TRANSPARENT_CONTENT_TYPE_BIT;
						break;
					default:
						assert(false);
						break;
					}

					const glm::vec4 boundingSphere = m_boundingSpheres[p.first];
					const glm::vec3 boundingSpherePos = transformationComponent.m_position + transformationComponent.m_orientation * (transformationComponent.m_scale * glm::vec3(boundingSphere));
					const float boundingSphereRadius = boundingSphere.w * transformationComponent.m_scale;

					for (const auto &cullData : frustumCullData)
					{
						// only add draw call to list if content type matches
						if ((cullData.m_contentTypeFlags & contentTypeMask) == contentTypeMask)
						{
							bool culled = false;
							for (size_t i = 0; i < cullData.m_planeCount; ++i)
							{
								if (glm::dot(glm::vec4(boundingSpherePos, 1.0f), cullData.m_planes[i]) <= -boundingSphereRadius)
								{
									culled = true;
									break;
								}
							}
							if (culled)
							{
								continue;
							}

							switch (m_materialBatchAssignment[p.second])
							{
							case 0: // Opaque
								++renderLists[cullData.m_renderListIndex].m_opaqueCount;
								break;
							case 1: // Alpha tested
								++renderLists[cullData.m_renderListIndex].m_maskedCount;
								break;
							case 2: // transparent
								++renderLists[cullData.m_renderListIndex].m_transparentCount;
								break;
							default:
								assert(false);
								break;
							}
							
							auto createMask = [](uint32_t size) {return size == 64 ? ~uint64_t() : (uint64_t(1) << size) - 1; };

							const uint32_t depth = 0;// (0.0f / planes.m_depthRange) * createMask(22);

							// key:
							// [drawListIdx 8][type 2][depth 22][instanceIdx 32]
							uint64_t drawCallKey = 0;
							drawCallKey |= (cullData.m_renderListIndex & createMask(8)) << 56;
							drawCallKey |= (m_materialBatchAssignment[p.second] & createMask(2)) << 54;
							drawCallKey |= (depth & createMask(22)) << 32;
							drawCallKey |= static_cast<uint32_t>(m_subMeshInstanceData.size());
							
							drawCallKeys.push_back(drawCallKey);

							m_subMeshInstanceData.push_back(instanceData);
						}
					}
				}
			});

			// sort draw call keys
			std::sort(drawCallKeys.begin(), drawCallKeys.end());

			// calculate the correct offsets into the *sorted* data
			uint32_t currentOffset = 0;
			for (auto &list : renderLists)
			{
				list.m_opaqueOffset = currentOffset;
				currentOffset += list.m_opaqueCount;
				list.m_maskedOffset = currentOffset;
				currentOffset += list.m_maskedCount;
				list.m_transparentOffset = currentOffset;
				currentOffset += list.m_transparentCount;
			}
		}

		assert(drawCallKeys.size() == m_subMeshInstanceData.size());

		m_commonRenderData.m_directionalLightCount = static_cast<uint32_t>(m_lightData.m_directionalLightData.size());
		m_commonRenderData.m_pointLightCount = static_cast<uint32_t>(m_lightData.m_pointLightData.size());

		RenderData renderData;
		renderData.m_transformDataCount = static_cast<uint32_t>(m_transformData.size());
		renderData.m_transformData = m_transformData.data();
		renderData.m_shadowMatrixCount = static_cast<uint32_t>(m_shadowMatrices.size());
		renderData.m_shadowMatrices = m_shadowMatrices.data();
		renderData.m_subMeshInstanceDataCount = static_cast<uint32_t>(m_subMeshInstanceData.size());
		renderData.m_subMeshInstanceData = m_subMeshInstanceData.data();
		renderData.m_drawCallKeys = drawCallKeys.data();
		renderData.m_mainViewRenderListIndex = 0;
		renderData.m_shadowCascadeViewRenderListOffset = 1;
		renderData.m_shadowCascadeViewRenderListCount = static_cast<uint32_t>(m_shadowMatrices.size());
		renderData.m_renderLists = renderLists.data();
		renderData.m_allInstanceDataCount = allInstanceData.size();
		renderData.m_allInstanceData = allInstanceData.data();

		m_renderer->render(m_commonRenderData, renderData, m_lightData);
		++m_commonRenderData.m_frame;
	}
}

VEngine::TextureHandle VEngine::RenderSystem::createTexture(const char *filepath)
{
	return m_renderer->loadTexture(filepath);
}

void VEngine::RenderSystem::destroyTexture(TextureHandle handle)
{
	m_renderer->freeTexture(handle);
}

void VEngine::RenderSystem::updateTextureData()
{
	m_renderer->updateTextureData();
}

void VEngine::RenderSystem::createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	m_renderer->createMaterials(count, materials, handles);
	updateMaterialBatchAssigments(count, materials, handles);
}

void VEngine::RenderSystem::updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	m_renderer->updateMaterials(count, materials, handles);
	updateMaterialBatchAssigments(count, materials, handles);
}

void VEngine::RenderSystem::destroyMaterials(uint32_t count, MaterialHandle *handles)
{
	m_renderer->destroyMaterials(count, handles);
}

void VEngine::RenderSystem::createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles)
{
	m_renderer->createSubMeshes(count, subMeshes, handles);
	for (uint32_t i = 0; i < count; ++i)
	{
		const glm::vec3 minC = subMeshes[i].m_minCorner;
		const glm::vec3 maxC = subMeshes[i].m_maxCorner;
		m_aabbs[handles[i]] = { minC, maxC };
		const glm::vec3 center = minC + (maxC - minC) * 0.5f;
		const float radius = glm::distance(center, maxC);
		m_boundingSpheres[handles[i]] = glm::vec4(center, radius);
	}
}

void VEngine::RenderSystem::destroySubMeshes(uint32_t count, SubMeshHandle *handles)
{
	m_renderer->destroySubMeshes(count, handles);
}

void VEngine::RenderSystem::setCameraEntity(entt::entity cameraEntity)
{
	m_cameraEntity = cameraEntity;
}

entt::entity VEngine::RenderSystem::getCameraEntity() const
{
	return m_cameraEntity;
}

const uint32_t * VEngine::RenderSystem::getLuminanceHistogram() const
{
	return m_renderer->getLuminanceHistogram();
}

std::vector<VEngine::VKMemoryBlockDebugInfo> VEngine::RenderSystem::getMemoryAllocatorDebugInfo() const
{
	return m_renderer->getMemoryAllocatorDebugInfo();
}

void VEngine::RenderSystem::getTimingInfo(size_t *count, const PassTimingInfo **data) const
{
	m_renderer->getTimingInfo(count, data);
}

void VEngine::RenderSystem::getOcclusionCullingStats(uint32_t &draws, uint32_t &totalDraws) const
{
	m_renderer->getOcclusionCullingStats(draws, totalDraws);
}

void VEngine::RenderSystem::updateMaterialBatchAssigments(size_t count, const Material *materials, MaterialHandle *handles)
{
	for (size_t i = 0; i < count; ++i)
	{
		m_materialBatchAssignment[handles[i]] = static_cast<uint8_t>(materials[i].m_alpha);
	}
}

void VEngine::RenderSystem::calculateCascadeViewProjectionMatrices(const glm::vec3 &lightDir, float maxShadowDistance, float splitLambda, float shadowTextureSize, size_t cascadeCount, glm::mat4 *viewProjectionMatrices)
{
	const float aspectRatio = m_commonRenderData.m_width * (1.0f / m_commonRenderData.m_height);
	const float tanFovX = glm::tan(0.5f * m_commonRenderData.m_fovy * aspectRatio);
	const float tanFovY = glm::tan(0.5f * m_commonRenderData.m_fovy);
	constexpr size_t maxShadowCascades = 8;
	float splits[maxShadowCascades];

	assert(cascadeCount <= maxShadowCascades);

	// compute split distances
	const float nearPlane = m_commonRenderData.m_nearPlane;
	const float farPlane = glm::min(m_commonRenderData.m_farPlane, maxShadowDistance);
	const float range = farPlane - nearPlane;
	const float ratio = farPlane / nearPlane;
	const float invCascadeCount = 1.0f / cascadeCount;
	for (size_t i = 0; i < cascadeCount; ++i)
	{
		float p = (i + 1) * invCascadeCount;
		float log = nearPlane * std::pow(ratio, p);
		float uniform = nearPlane + range * p;
		splits[i] = splitLambda * (log - uniform) + uniform;
	}

	const glm::vec3 &camPos = m_commonRenderData.m_cameraPosition;
	const glm::vec3 &camDir = m_commonRenderData.m_cameraDirection;
	const glm::vec3 camRight = glm::vec3(m_commonRenderData.m_viewMatrix[0][0], m_commonRenderData.m_viewMatrix[1][0], m_commonRenderData.m_viewMatrix[2][0]);
	const glm::vec3 camUp = glm::vec3(m_commonRenderData.m_viewMatrix[0][1], m_commonRenderData.m_viewMatrix[1][1], m_commonRenderData.m_viewMatrix[2][1]);

	const glm::mat4 vulkanCorrection =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.5f, 0.0f },
		{ 0.0f, 0.0f, 0.5f, 1.0f }
	};

	float previousSplit = nearPlane;
	for (size_t i = 0; i < cascadeCount; ++i)
	{
		// calculate split center and split bounding sphere radius
		const float splitCenterDistance = previousSplit + (splits[i] - previousSplit) * 0.5f;
		const glm::vec3 center = camPos + camDir * splitCenterDistance;
		const glm::vec3 cornerPoint = camPos + (camRight * tanFovX + camUp * tanFovY + camDir) * splits[i];
		const float radius = glm::distance(center, cornerPoint);

		const glm::vec3 cornerPoint1 = camPos + (-camRight * tanFovX + -camUp * tanFovY + camDir) * previousSplit;
		const glm::vec3 center1 = (cornerPoint + cornerPoint1) * 0.5f;
		const float radius1 = glm::distance(cornerPoint, cornerPoint1) * 0.5f;

		// https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html
		const float n = previousSplit;
		const float f = splits[i];
		float aspect = m_commonRenderData.m_width / static_cast<float>(m_commonRenderData.m_height);
		const float k = glm::sqrt(1.0f + aspect * aspect) * glm::tan(m_commonRenderData.m_fovy * 0.5f);
		
		glm::vec3 center2;
		float radius2;
		const float k2 = k * k;
		const float fnSum = f + n;
		const float fnDiff = f - n;
		const float fnSum2 = fnSum * fnSum;
		const float fnDiff2 = fnDiff * fnDiff;
		if (k2 >= fnDiff / fnSum)
		{
			center2 = glm::vec3(0.0f, 0.0f, -f);
			radius2 = f * k;
		}
		else
		{
			center2 = glm::vec3(0.0f, 0.0f, -0.5f * fnSum * (1.0f + k2));
			radius2 = 0.5f * sqrt(fnDiff2 + 2 * (f * f + n * n) * k2 + fnSum2 * k2 * k2);
		}
		glm::vec4 tmp = m_commonRenderData.m_invViewMatrix * glm::vec4(center2, 1.0f);
		center2 = glm::vec3(tmp / tmp.w);

		glm::vec3 upDir(0.0f, 1.0f, 0.0f);

		// choose different up vector if light direction would be linearly dependent otherwise
		if (abs(lightDir.x) < 0.001 && abs(lightDir.z) < 0.001)
		{
			upDir = glm::vec3(1.0f, 1.0f, 0.0f);
		}

		glm::mat4 lightView = glm::lookAt(center2 + lightDir * 150.0f, center2, upDir);

		// snap to shadow map texel to avoid shimmering
		lightView[3].x -= fmodf(lightView[3].x, (radius2 / static_cast<float>(shadowTextureSize)) * 2.0f);
		lightView[3].y -= fmodf(lightView[3].y, (radius2 / static_cast<float>(shadowTextureSize)) * 2.0f);

		viewProjectionMatrices[i] = vulkanCorrection * glm::ortho(-radius2, radius2, -radius2, radius2, 00.0f, 300.0f) * lightView;

		previousSplit = splits[i];
	}
}