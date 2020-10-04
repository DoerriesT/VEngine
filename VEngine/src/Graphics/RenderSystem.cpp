#include "RenderSystem.h"
#include "Components/MeshComponent.h"
#include "Components/TransformationComponent.h"
#include "Components/RenderableComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/CameraComponent.h"
#include "Components/ParticipatingMediumComponent.h"
#include "Components/BoundingBoxComponent.h"
#include "Components/ReflectionProbeComponent.h"
#include "Components/BillboardComponent.h"
#include "Graphics/Renderer.h"
#include "GlobalVar.h"
#include <glm/ext.hpp>
#include <glm/vec3.hpp>
#include <algorithm>
#include "Utility/Utility.h"
#include "Mesh.h"
#include "ViewRenderList.h"
#include "Utility/QuadTreeAllocator.h"
#include "ReflectionProbeManager.h"
#include "FrustumCullData.h"

glm::vec3 g_sunDir;

VEngine::RenderSystem::RenderSystem(entt::registry &entityRegistry, void *windowHandle, uint32_t width, uint32_t height)
	:m_entityRegistry(entityRegistry),
	m_cameraEntity(entt::null),
	m_materialBatchAssignment(std::make_unique<uint8_t[]>(RendererConsts::MAX_MATERIALS)),
	m_aabbs(std::make_unique<AxisAlignedBoundingBox[]>(RendererConsts::MAX_SUB_MESHES)),
	m_texCoordScaleBias(std::make_unique<glm::vec4[]>(RendererConsts::MAX_SUB_MESHES)),
	m_boundingSpheres(std::make_unique<glm::vec4[]>(RendererConsts::MAX_SUB_MESHES)),
	m_width(width),
	m_height(height),
	m_swapChainWidth(width),
	m_swapChainHeight(height)
{
	for (size_t i = 0; i < RendererConsts::MAX_TAA_HALTON_SAMPLES; ++i)
	{
		m_haltonX[i] = Utility::halton(i + 1, 2) * 2.0f - 1.0f;
		m_haltonY[i] = Utility::halton(i + 1, 3) * 2.0f - 1.0f;
	}

	m_renderer = std::make_unique<Renderer>(m_width, m_height, windowHandle);
	m_reflectionProbeManager = std::make_unique<ReflectionProbeManager>(m_entityRegistry);
	m_particleEmitterManager = std::make_unique<ParticleEmitterManager>(m_entityRegistry);
	memset(&m_commonRenderData, 0, sizeof(m_commonRenderData));

	m_lightData.m_reflectionProbeRelightData = m_reflectionProbeManager->getRelightData();
}

void VEngine::RenderSystem::update(float timeDelta)
{
	static QuadTreeAllocator quadTreeAllocator(8192, 256, 1024);
	static QuadTreeAllocator fomQuadTreeAllocator(2048, 128, 256);
	quadTreeAllocator.freeAll();
	fomQuadTreeAllocator.freeAll();

	m_transformData.clear();
	m_subMeshInstanceData.clear();
	m_shadowMatrices.clear();
	m_shadowCascadeParams.clear();
	m_lightData.clear();

	std::vector<ViewRenderList> renderLists;

	std::vector<FrustumCullData> frustumCullData;

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
				glm::translate(glm::vec3(m_haltonX[m_commonRenderData.m_frame % RendererConsts::MAX_TAA_HALTON_SAMPLES] / m_width,
					m_haltonY[m_commonRenderData.m_frame % RendererConsts::MAX_TAA_HALTON_SAMPLES] / m_height,
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
			m_commonRenderData.m_width = m_width;
			m_commonRenderData.m_height = m_height;
			m_commonRenderData.m_swapChainWidth = m_swapChainWidth;
			m_commonRenderData.m_swapChainHeight = m_swapChainHeight;
			m_commonRenderData.m_curResIdx = m_commonRenderData.m_frame % RendererConsts::FRAMES_IN_FLIGHT;
			m_commonRenderData.m_prevResIdx = (m_commonRenderData.m_frame + RendererConsts::FRAMES_IN_FLIGHT - 1) % RendererConsts::FRAMES_IN_FLIGHT;
			m_commonRenderData.m_timeDelta = static_cast<float>(timeDelta);
		}

		// extract view frustum plane equations from matrix
		{
			FrustumCullData cullData(m_commonRenderData.m_jitteredViewProjectionMatrix, 6, 0, FrustumCullData::ALL_CONTENT_TYPE_BIT,
				glm::vec4(m_commonRenderData.m_viewMatrix[0][2], m_commonRenderData.m_viewMatrix[1][2], m_commonRenderData.m_viewMatrix[2][2], m_commonRenderData.m_viewMatrix[3][2]),
				m_commonRenderData.m_farPlane - m_commonRenderData.m_nearPlane);
			frustumCullData.push_back(cullData);
			renderLists.push_back({});
		}

		uint32_t probeShadowsRenderListOffset = 0;
		uint32_t probeShadowRenderListCount = 0;
		uint32_t shadowCascadeRenderListOffset = 0;
		uint32_t shadowCascadeRenderListCount = 0;

		const glm::vec4 viewMatDepthRow = glm::vec4(m_commonRenderData.m_viewMatrix[0][2], m_commonRenderData.m_viewMatrix[1][2], m_commonRenderData.m_viewMatrix[2][2], m_commonRenderData.m_viewMatrix[3][2]);


		// get list of reflection probes
		uint32_t probeDrawListOffset = static_cast<uint32_t>(renderLists.size());
		glm::mat4 *probeMatrices;
		uint32_t *probeRenderIndices;
		uint32_t probeRenderCount = 0;
		uint32_t *probeRelightIndices;
		uint32_t probeRelightCount;
		glm::vec3 probeShadowCenter;
		float probeShadowRadius;

		m_reflectionProbeManager->update(m_commonRenderData, m_lightData, renderLists, frustumCullData, probeMatrices, probeRenderIndices, probeRenderCount, probeRelightIndices,
			probeRelightCount, probeShadowCenter, probeShadowRadius);

		// generate light data
		{
			// directional lights
			{
				auto view = m_entityRegistry.view<TransformationComponent, DirectionalLightComponent, RenderableComponent>();

				// camera shadow render lists
				shadowCascadeRenderListOffset = static_cast<uint32_t>(renderLists.size());
				view.each([&](TransformationComponent &transformationComponent, DirectionalLightComponent &directionalLightComponent, RenderableComponent &)
					{
						assert(directionalLightComponent.m_cascadeCount <= DirectionalLightComponent::MAX_CASCADES);

						const glm::vec3 direction = transformationComponent.m_orientation * glm::vec3(0.0f, 1.0f, 0.0f);
						g_sunDir = direction;

						DirectionalLight directionalLight{};
						directionalLight.m_color = directionalLightComponent.m_color * directionalLightComponent.m_intensity;
						directionalLight.m_shadowOffset = static_cast<uint32_t>(m_shadowMatrices.size());
						directionalLight.m_direction = direction;
						directionalLight.m_shadowCount = directionalLightComponent.m_shadows ? directionalLightComponent.m_cascadeCount : 0;

						if (directionalLightComponent.m_shadows)
						{
							m_lightData.m_directionalLightsShadowed.push_back(directionalLight);

							// calculate shadow matrices
							glm::mat4 shadowMatrices[DirectionalLightComponent::MAX_CASCADES];
							glm::vec4 cascadeParams[DirectionalLightComponent::MAX_CASCADES];
							glm::vec4 depthRows[DirectionalLightComponent::MAX_CASCADES];

							for (uint32_t i = 0; i < directionalLightComponent.m_cascadeCount; ++i)
							{
								cascadeParams[i].x = directionalLightComponent.m_depthBias[i];
								cascadeParams[i].y = directionalLightComponent.m_normalOffsetBias[i];
							}

							calculateCascadeViewProjectionMatrices(direction,
								directionalLightComponent.m_maxShadowDistance,
								directionalLightComponent.m_splitLambda,
								2048.0f,
								directionalLightComponent.m_cascadeCount,
								shadowMatrices,
								cascadeParams,
								depthRows);

							m_shadowMatrices.reserve(m_shadowMatrices.size() + directionalLightComponent.m_cascadeCount);
							m_shadowCascadeParams.reserve(m_shadowCascadeParams.size() + directionalLightComponent.m_cascadeCount);

							for (size_t i = 0; i < directionalLightComponent.m_cascadeCount; ++i)
							{
								m_shadowMatrices.push_back(shadowMatrices[i]);
								m_shadowCascadeParams.push_back(cascadeParams[i]);

								// extract view frustum plane equations from matrix
								{
									uint32_t contentTypeFlags = FrustumCullData::STATIC_OPAQUE_CONTENT_TYPE_BIT
										| FrustumCullData::STATIC_ALPHA_TESTED_CONTENT_TYPE_BIT
										| FrustumCullData::DYNAMIC_OPAQUE_CONTENT_TYPE_BIT
										| FrustumCullData::DYNAMIC_ALPHA_TESTED_CONTENT_TYPE_BIT;
									FrustumCullData cullData(shadowMatrices[i], 5, static_cast<uint32_t>(renderLists.size()), contentTypeFlags, depthRows[i], 300.0f);

									frustumCullData.push_back(cullData);
									renderLists.push_back({});
									++shadowCascadeRenderListCount;
								}
							}
						}
						else
						{
							m_lightData.m_directionalLights.push_back(directionalLight);
						}
					});

				// probe shadow render lists
				probeShadowsRenderListOffset = static_cast<uint32_t>(renderLists.size());
				if (probeRelightCount != 0)
				{
					view.each([&](TransformationComponent &transformationComponent, DirectionalLightComponent &directionalLightComponent, RenderableComponent &)
						{
							if (directionalLightComponent.m_shadows)
							{
								const glm::vec3 direction = transformationComponent.m_orientation * glm::vec3(0.0f, 1.0f, 0.0f);

								DirectionalLight directionalLight{};
								directionalLight.m_color = directionalLightComponent.m_color * directionalLightComponent.m_intensity;
								directionalLight.m_shadowOffset = static_cast<uint32_t>(m_shadowMatrices.size());
								directionalLight.m_direction = direction;
								directionalLight.m_shadowCount = directionalLightComponent.m_shadows ? 1 : 0;


								m_lightData.m_directionalLightsShadowedProbe.push_back(directionalLight);

								// calculate shadow matrix
								glm::vec3 upDir(0.0f, 1.0f, 0.0f);
								// choose different up vector if light direction would be linearly dependent otherwise
								if (abs(direction.x) < 0.001f && abs(direction.z) < 0.001f)
								{
									upDir = glm::vec3(1.0f, 1.0f, 0.0f);
								}

								glm::mat4 lightView = glm::lookAt(direction * 150.0f + probeShadowCenter, probeShadowCenter, upDir);

								constexpr float shadowMapRes = 2048.0f;
								constexpr float depthRange = 300.0f;
								constexpr float precisionRange = 65536.0f;

								// snap to shadow map texel to avoid shimmering
								lightView[3].x -= fmodf(lightView[3].x, (probeShadowRadius / shadowMapRes) * 2.0f);
								lightView[3].y -= fmodf(lightView[3].y, (probeShadowRadius / shadowMapRes) * 2.0f);
								lightView[3].z -= fmodf(lightView[3].z, depthRange / precisionRange);

								glm::mat4 shadowMatrix = glm::ortho(-probeShadowRadius, probeShadowRadius, -probeShadowRadius, probeShadowRadius, 0.0f, depthRange) * lightView;

								m_shadowMatrices.push_back(shadowMatrix);


								// extract view frustum plane equations from matrix
								{
									uint32_t contentTypeFlags = FrustumCullData::STATIC_OPAQUE_CONTENT_TYPE_BIT | FrustumCullData::STATIC_ALPHA_TESTED_CONTENT_TYPE_BIT;
									FrustumCullData cullData(shadowMatrix, 5, static_cast<uint32_t>(renderLists.size()), contentTypeFlags, glm::vec4(lightView[0][2], lightView[1][2], lightView[2][2], lightView[3][2]), 300.0f);

									frustumCullData.push_back(cullData);
									renderLists.push_back({});
									++probeShadowRenderListCount;
								}
							}
						});
				}
			}

			// point lights
			{
				auto view = m_entityRegistry.view<TransformationComponent, PointLightComponent, RenderableComponent>();

				view.each([&](TransformationComponent &transformationComponent, PointLightComponent &pointLightComponent, RenderableComponent &)
					{
						const float intensity = pointLightComponent.m_luminousPower * (1.0f / (4.0f * glm::pi<float>()));

						PunctualLight punctualLight{};
						punctualLight.m_color = pointLightComponent.m_color * intensity;
						punctualLight.m_invSqrAttRadius = 1.0f / (pointLightComponent.m_radius * pointLightComponent.m_radius);
						punctualLight.m_position = transformationComponent.m_position;
						punctualLight.m_angleScale = -1.0f; // special value to mark this as a point light

						// frustum cull
						for (size_t i = 0; i < frustumCullData[0].m_planeCount; ++i)
						{
							if (glm::dot(glm::vec4(transformationComponent.m_position, 1.0f), frustumCullData[0].m_planes[i]) <= -pointLightComponent.m_radius)
							{
								return;
							}
						}

						// try to allocate space in the shadow atlas if light is shadowed
						ShadowAtlasDrawInfo atlasDrawInfo[6];
						bool shadowMapAllocationSucceeded = true;
						if ((renderLists.size() + 6) < 256 && pointLightComponent.m_shadows)
						{
							glm::vec3 tr = punctualLight.m_position + pointLightComponent.m_radius * glm::vec3(1.0f, 1.0f, 0.0f);
							tr.z = glm::min(tr.z, -m_commonRenderData.m_nearPlane);
							glm::vec3 bl = punctualLight.m_position - pointLightComponent.m_radius * glm::vec3(1.0f, 1.0f, 0.0f);
							bl.z = glm::min(bl.z, -m_commonRenderData.m_nearPlane);

							auto trSS = m_commonRenderData.m_projectionMatrix * glm::vec4(tr, 1.0f);
							trSS /= trSS.w;
							trSS.x = trSS.x * 0.5f + 0.5f;
							trSS.y = trSS.y * 0.5f + 0.5f;
							auto blSS = m_commonRenderData.m_projectionMatrix * glm::vec4(bl, 1.0f);
							blSS /= blSS.w;
							blSS.x = blSS.x * 0.5f + 0.5f;
							blSS.y = blSS.y * 0.5f + 0.5f;

							float sizeX = trSS.x - blSS.x;
							float sizeY = blSS.y - trSS.y;

							float scale = 0.5f / 6.0f;
							sizeX *= m_commonRenderData.m_width * scale;
							sizeY *= m_commonRenderData.m_height * scale;

							uint32_t screenSpaceSize = static_cast<uint32_t>(glm::max(sizeX, sizeY, 1.0f));

							size_t allocatedCount = 0;
							for (size_t i = 0; i < 6 && shadowMapAllocationSucceeded; ++i)
							{
								bool result = quadTreeAllocator.alloc(screenSpaceSize, atlasDrawInfo[allocatedCount].m_offsetX, atlasDrawInfo[allocatedCount].m_offsetY, atlasDrawInfo[allocatedCount].m_size);
								shadowMapAllocationSucceeded = shadowMapAllocationSucceeded && result;
								allocatedCount += result ? 1 : 0;
							}

							if (!shadowMapAllocationSucceeded)
							{
								// deallocate tiles
								for (size_t i = 0; i < allocatedCount; ++i)
								{
									quadTreeAllocator.free(atlasDrawInfo[i].m_offsetX, atlasDrawInfo[i].m_offsetY, atlasDrawInfo[i].m_size);
								}
							}
						}

						if ((renderLists.size() + 6) < 256 && pointLightComponent.m_shadows && shadowMapAllocationSucceeded)
						{
							glm::mat4 viewMatrices[6];
							viewMatrices[0] = glm::lookAt(transformationComponent.m_position, transformationComponent.m_position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
							viewMatrices[1] = glm::lookAt(transformationComponent.m_position, transformationComponent.m_position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
							viewMatrices[2] = glm::lookAt(transformationComponent.m_position, transformationComponent.m_position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
							viewMatrices[3] = glm::lookAt(transformationComponent.m_position, transformationComponent.m_position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
							viewMatrices[4] = glm::lookAt(transformationComponent.m_position, transformationComponent.m_position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
							viewMatrices[5] = glm::lookAt(transformationComponent.m_position, transformationComponent.m_position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

							PunctualLightShadowed punctualLightShadowed{ punctualLight };
							punctualLightShadowed.m_radius = pointLightComponent.m_radius;

							// volumetric shadows
							if (pointLightComponent.m_volumetricShadows)
							{
								FOMAtlasDrawInfo fomAtlasDrawInfo{};
								if (fomQuadTreeAllocator.alloc(128, fomAtlasDrawInfo.m_offsetX, fomAtlasDrawInfo.m_offsetY, fomAtlasDrawInfo.m_size))
								{
									punctualLightShadowed.m_fomShadowAtlasParams.x = (fomAtlasDrawInfo.m_size - 2) / 2048.0f;
									punctualLightShadowed.m_fomShadowAtlasParams.y = static_cast<float>(fomAtlasDrawInfo.m_offsetX + 1) / (fomAtlasDrawInfo.m_size - 2) * punctualLightShadowed.m_fomShadowAtlasParams.x;
									punctualLightShadowed.m_fomShadowAtlasParams.z = static_cast<float>(fomAtlasDrawInfo.m_offsetY + 1) / (fomAtlasDrawInfo.m_size - 2) * punctualLightShadowed.m_fomShadowAtlasParams.x;
									punctualLightShadowed.m_fomShadowAtlasParams.w = 1.0f;

									fomAtlasDrawInfo.m_lightPosition = transformationComponent.m_position;
									fomAtlasDrawInfo.m_lightRadius = pointLightComponent.m_radius;
									fomAtlasDrawInfo.m_pointLight = true;

									m_lightData.m_fomAtlasDrawInfos.push_back(fomAtlasDrawInfo);
								}
							}


							glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, pointLightComponent.m_radius);
							for (size_t i = 0; i < 6; ++i)
							{
								// make sure the shadow map has a small border to account for filtering
								constexpr uint32_t texelBorderSize = 4;
								const float scale = (atlasDrawInfo[i].m_size - texelBorderSize * 2) / static_cast<float>(atlasDrawInfo[i].m_size);
								glm::mat4 shadowMatrix = projection * viewMatrices[i];
								shadowMatrix = glm::scale(glm::vec3(scale, scale, 1.0f)) * shadowMatrix;

								atlasDrawInfo[i].m_shadowMatrixIdx = static_cast<uint32_t>(m_shadowMatrices.size());
								atlasDrawInfo[i].m_drawListIdx = static_cast<uint32_t>(renderLists.size());

								m_shadowMatrices.push_back(shadowMatrix);

								// extract view frustum plane equations from matrix
								{
									uint32_t contentTypeFlags = FrustumCullData::STATIC_OPAQUE_CONTENT_TYPE_BIT
										| FrustumCullData::STATIC_ALPHA_TESTED_CONTENT_TYPE_BIT
										| FrustumCullData::DYNAMIC_OPAQUE_CONTENT_TYPE_BIT
										| FrustumCullData::DYNAMIC_ALPHA_TESTED_CONTENT_TYPE_BIT;
									FrustumCullData cullData(shadowMatrix, 5, static_cast<uint32_t>(renderLists.size()), contentTypeFlags, glm::vec4(viewMatrices[i][0][2], viewMatrices[i][1][2], viewMatrices[i][2][2], viewMatrices[i][3][2]), pointLightComponent.m_radius);

									frustumCullData.push_back(cullData);
									renderLists.push_back({});
								}

								punctualLightShadowed.m_shadowAtlasParams[i].x = atlasDrawInfo[i].m_size * (1.0f / 8192.0f);
								punctualLightShadowed.m_shadowAtlasParams[i].y = atlasDrawInfo[i].m_offsetX / atlasDrawInfo[i].m_size * punctualLightShadowed.m_shadowAtlasParams[i].x;
								punctualLightShadowed.m_shadowAtlasParams[i].z = atlasDrawInfo[i].m_offsetY / atlasDrawInfo[i].m_size * punctualLightShadowed.m_shadowAtlasParams[i].x;
								punctualLightShadowed.m_shadowAtlasParams[i].w = scale;

								m_lightData.m_shadowAtlasDrawInfos.push_back(atlasDrawInfo[i]);
							}

							m_lightData.m_punctualLightsShadowed.push_back(punctualLightShadowed);
							m_lightData.m_punctualLightShadowedTransforms.push_back(glm::translate(transformationComponent.m_position) * glm::scale(glm::vec3(pointLightComponent.m_radius)));
							m_lightData.m_punctualLightShadowedOrder.push_back(static_cast<uint32_t>(m_lightData.m_punctualLightShadowedOrder.size()));
						}
						else
						{
							m_lightData.m_punctualLights.push_back(punctualLight);
							m_lightData.m_punctualLightTransforms.push_back(glm::translate(transformationComponent.m_position) * glm::scale(glm::vec3(pointLightComponent.m_radius)));
							m_lightData.m_punctualLightOrder.push_back(static_cast<uint32_t>(m_lightData.m_punctualLightOrder.size()));
						}
					});
			}

			// spot lights
			{
				auto view = m_entityRegistry.view<TransformationComponent, SpotLightComponent, RenderableComponent>();

				view.each([&](TransformationComponent &transformationComponent, SpotLightComponent &spotLightComponent, RenderableComponent &)
					{
						const float intensity = spotLightComponent.m_luminousPower * (1.0f / (glm::pi<float>()));
						const float angleScale = 1.0f / glm::max(0.001f, glm::cos(spotLightComponent.m_innerAngle * 0.5f) - glm::cos(spotLightComponent.m_outerAngle * 0.5f));
						const float angleOffset = -glm::cos(spotLightComponent.m_outerAngle * 0.5f) * angleScale;
						const glm::vec3 directionWS = transformationComponent.m_orientation * glm::vec3(0.0f, 0.0f, -1.0f);

						PunctualLight punctualLight{};
						punctualLight.m_color = spotLightComponent.m_color * intensity;
						punctualLight.m_invSqrAttRadius = 1.0f / (spotLightComponent.m_radius * spotLightComponent.m_radius);
						punctualLight.m_position = transformationComponent.m_position;
						punctualLight.m_angleScale = angleScale;
						punctualLight.m_direction = directionWS;
						punctualLight.m_angleOffset = angleOffset;

						// construct bounding sphere
						// https://bartwronski.com/2017/04/13/cull-that-cone/
						glm::vec4 boundingSphere = glm::vec4(0.0f);
						{
							const float angle = spotLightComponent.m_outerAngle;
							const float cosAngle = glm::cos(angle);
							const float radius = spotLightComponent.m_radius;
							const auto &origin = transformationComponent.m_position;
							if (angle > glm::pi<float>() * 0.25f)
							{
								const float sinAngle = glm::sqrt(1.0f - cosAngle * cosAngle);
								boundingSphere = glm::vec4(origin + cosAngle * radius * directionWS, sinAngle * radius);
							}
							else
							{
								boundingSphere = glm::vec4(origin + radius / (2.0f * cosAngle) * directionWS, radius / (2.0f * cosAngle));
							}
						}

						// frustum cull
						for (size_t i = 0; i < frustumCullData[0].m_planeCount; ++i)
						{
							if (glm::dot(glm::vec4(glm::vec3(boundingSphere), 1.0f), frustumCullData[0].m_planes[i]) <= -boundingSphere.w)
							{
								return;
							}
						}

						uint32_t screenSpaceSize = 1;

						if (renderLists.size() < 256 && spotLightComponent.m_shadows)
						{
							glm::vec3 bSphereVSPos = m_commonRenderData.m_viewMatrix * glm::vec4(glm::vec3(boundingSphere), 1.0f);
							glm::vec3 tr = bSphereVSPos + boundingSphere.w * glm::vec3(1.0f, 1.0f, 0.0f);
							tr.z = glm::min(tr.z, -m_commonRenderData.m_nearPlane);
							glm::vec3 bl = bSphereVSPos - boundingSphere.w * glm::vec3(1.0f, 1.0f, 0.0f);
							bl.z = glm::min(bl.z, -m_commonRenderData.m_nearPlane);

							auto trSS = m_commonRenderData.m_projectionMatrix * glm::vec4(tr, 1.0f);
							trSS /= trSS.w;
							trSS.x = trSS.x * 0.5f + 0.5f;
							trSS.y = trSS.y * 0.5f + 0.5f;
							auto blSS = m_commonRenderData.m_projectionMatrix * glm::vec4(bl, 1.0f);
							blSS /= blSS.w;
							blSS.x = blSS.x * 0.5f + 0.5f;
							blSS.y = blSS.y * 0.5f + 0.5f;

							float sizeX = trSS.x - blSS.x;
							float sizeY = blSS.y - trSS.y;

							float scale = 0.5f;
							sizeX *= m_commonRenderData.m_width * scale;
							sizeY *= m_commonRenderData.m_height * scale;

							screenSpaceSize = static_cast<uint32_t>(glm::max(sizeX, sizeY, 1.0f));
						}

						uint32_t tileOffsetX, tileOffsetY, tileSize;
						if (renderLists.size() < 256 && spotLightComponent.m_shadows && quadTreeAllocator.alloc(screenSpaceSize, tileOffsetX, tileOffsetY, tileSize))
						{
							// create shadow matrix
							glm::vec3 upDir(0.0f, 1.0f, 0.0f);
							// choose different up vector if light direction would be linearly dependent otherwise
							if (abs(directionWS.x) < 0.001f && abs(directionWS.z) < 0.001f)
							{
								upDir = glm::vec3(1.0f, 0.0f, 0.0f);
							}

							// make sure the shadow map has a small border to account for filtering
							constexpr uint32_t texelBorderSize = 4;
							const float scale = (tileSize - texelBorderSize * 2) / static_cast<float>(tileSize);

							glm::mat4 viewMat = glm::lookAt(transformationComponent.m_position, transformationComponent.m_position + directionWS, upDir);
							glm::mat4 shadowMatrix = glm::scale(glm::vec3(scale, scale, 1.0f)) * glm::perspective(spotLightComponent.m_outerAngle, 1.0f, 0.1f, spotLightComponent.m_radius) * viewMat;

							PunctualLightShadowed punctualLightShadowed{ punctualLight };
							punctualLightShadowed.m_shadowMatrix0 = { shadowMatrix[0][0], shadowMatrix[1][0], shadowMatrix[2][0], shadowMatrix[3][0] };
							punctualLightShadowed.m_shadowMatrix1 = { shadowMatrix[0][1], shadowMatrix[1][1], shadowMatrix[2][1], shadowMatrix[3][1] };
							punctualLightShadowed.m_shadowMatrix2 = { shadowMatrix[0][2], shadowMatrix[1][2], shadowMatrix[2][2], shadowMatrix[3][2] };
							punctualLightShadowed.m_shadowMatrix3 = { shadowMatrix[0][3], shadowMatrix[1][3], shadowMatrix[2][3], shadowMatrix[3][3] };
							punctualLightShadowed.m_shadowAtlasParams[0].x = tileSize * (1.0f / 8192.0f);
							punctualLightShadowed.m_shadowAtlasParams[0].y = tileOffsetX / tileSize * punctualLightShadowed.m_shadowAtlasParams[0].x;
							punctualLightShadowed.m_shadowAtlasParams[0].z = tileOffsetY / tileSize * punctualLightShadowed.m_shadowAtlasParams[0].x;
							punctualLightShadowed.m_radius = spotLightComponent.m_radius;

							// volumetric shadows
							if (spotLightComponent.m_volumetricShadows)
							{
								FOMAtlasDrawInfo fomAtlasDrawInfo{};
								if (fomQuadTreeAllocator.alloc(128, fomAtlasDrawInfo.m_offsetX, fomAtlasDrawInfo.m_offsetY, fomAtlasDrawInfo.m_size))
								{
									punctualLightShadowed.m_fomShadowAtlasParams.x = fomAtlasDrawInfo.m_size / 2048.0f;
									punctualLightShadowed.m_fomShadowAtlasParams.y = static_cast<float>(fomAtlasDrawInfo.m_offsetX) / fomAtlasDrawInfo.m_size * punctualLightShadowed.m_fomShadowAtlasParams.x;
									punctualLightShadowed.m_fomShadowAtlasParams.z = static_cast<float>(fomAtlasDrawInfo.m_offsetY) / fomAtlasDrawInfo.m_size * punctualLightShadowed.m_fomShadowAtlasParams.x;
									punctualLightShadowed.m_fomShadowAtlasParams.w = 1.0f;

									fomAtlasDrawInfo.m_lightPosition = transformationComponent.m_position;
									fomAtlasDrawInfo.m_lightRadius = spotLightComponent.m_radius;
									fomAtlasDrawInfo.m_shadowMatrix = shadowMatrix;
									fomAtlasDrawInfo.m_pointLight = false;

									m_lightData.m_fomAtlasDrawInfos.push_back(fomAtlasDrawInfo);
								}
							}


							ShadowAtlasDrawInfo atlasDrawInfo{};
							atlasDrawInfo.m_shadowMatrixIdx = static_cast<uint32_t>(m_shadowMatrices.size());
							atlasDrawInfo.m_drawListIdx = static_cast<uint32_t>(renderLists.size());
							atlasDrawInfo.m_offsetX = tileOffsetX;
							atlasDrawInfo.m_offsetY = tileOffsetY;
							atlasDrawInfo.m_size = tileSize;

							m_lightData.m_shadowAtlasDrawInfos.push_back(atlasDrawInfo);



							m_shadowMatrices.push_back(shadowMatrix);

							// extract view frustum plane equations from matrix
							{
								uint32_t contentTypeFlags = FrustumCullData::STATIC_OPAQUE_CONTENT_TYPE_BIT
									| FrustumCullData::STATIC_ALPHA_TESTED_CONTENT_TYPE_BIT
									| FrustumCullData::DYNAMIC_OPAQUE_CONTENT_TYPE_BIT
									| FrustumCullData::DYNAMIC_ALPHA_TESTED_CONTENT_TYPE_BIT;
								FrustumCullData cullData(shadowMatrix, 5, static_cast<uint32_t>(renderLists.size()), contentTypeFlags, glm::vec4(viewMat[0][2], viewMat[1][2], viewMat[2][2], viewMat[3][2]), spotLightComponent.m_radius);

								frustumCullData.push_back(cullData);
								renderLists.push_back({});
							}

							m_lightData.m_punctualLightsShadowed.push_back(punctualLightShadowed);
							m_lightData.m_punctualLightShadowedTransforms.push_back(glm::translate(transformationComponent.m_position) * glm::mat4_cast(transformationComponent.m_orientation) * glm::scale(glm::vec3(spotLightComponent.m_radius)));
							m_lightData.m_punctualLightShadowedOrder.push_back(static_cast<uint32_t>(m_lightData.m_punctualLightShadowedOrder.size()));
						}
						else
						{
							m_lightData.m_punctualLights.push_back(punctualLight);
							m_lightData.m_punctualLightTransforms.push_back(glm::translate(transformationComponent.m_position) * glm::mat4_cast(transformationComponent.m_orientation) * glm::scale(glm::vec3(spotLightComponent.m_radius)));
							m_lightData.m_punctualLightOrder.push_back(static_cast<uint32_t>(m_lightData.m_punctualLightOrder.size()));
						}

					});
			}

			// sort lights and assign to depth bins
			for (int shadowed = 0; shadowed < 2; ++shadowed)
			{
				auto &lightOrder = (shadowed == 0) ? m_lightData.m_punctualLightOrder : m_lightData.m_punctualLightShadowedOrder;
				auto &depthBins = (shadowed == 0) ? m_lightData.m_punctualLightDepthBins : m_lightData.m_punctualLightShadowedDepthBins;

				// sort by distance to camera
				if (shadowed == 0)
				{
					std::sort(lightOrder.begin(), lightOrder.end(), [&](const uint32_t &lhs, const uint32_t &rhs)
						{
							return -glm::dot(glm::vec4(m_lightData.m_punctualLights[lhs].m_position, 1.0f), viewMatDepthRow) < -glm::dot(glm::vec4(m_lightData.m_punctualLights[rhs].m_position, 1.0f), viewMatDepthRow);
						});
				}
				else
				{
					std::sort(lightOrder.begin(), lightOrder.end(), [&](const uint32_t &lhs, const uint32_t &rhs)
						{
							return -glm::dot(glm::vec4(m_lightData.m_punctualLightsShadowed[lhs].m_light.m_position, 1.0f), viewMatDepthRow) < -glm::dot(glm::vec4(m_lightData.m_punctualLightsShadowed[rhs].m_light.m_position, 1.0f), viewMatDepthRow);
						});
				}


				// clear bins
				for (size_t i = 0; i < depthBins.size(); ++i)
				{
					const uint32_t emptyBin = ((~0u & 0xFFFFu) << 16u);
					depthBins[i] = emptyBin;
				}

				// assign lights to bins
				for (size_t i = 0; i < lightOrder.size(); ++i)
				{
					const auto &light = shadowed == 0 ? m_lightData.m_punctualLights[lightOrder[i]] : m_lightData.m_punctualLightsShadowed[lightOrder[i]].m_light;
					const float radius = glm::sqrt(1.0f / light.m_invSqrAttRadius);
					float nearestPoint = 0.0f;
					float furthestPoint = 0.0f;

					float lightDepthVS = glm::dot(glm::vec4(light.m_position, 1.0f), viewMatDepthRow);

					//// spot light
					//if (light.m_angleScale != -1.0f)
					//{
					//	float cosAngle = -light.m_angleOffset / light.m_angleScale;
					//	float sinAngle = glm::sqrt(1.0f - cosAngle * cosAngle);
					//
					//	glm::vec3 lightDirVS = m_commonRenderData.m_viewMatrix * glm::vec4(light.m_direction, 0.0f);
					//
					//	//const glm::vec3 v1 = glm::cross(glm::vec3(0.0f, 0.0f, -1.0f), lightDirVS);
					//	//const glm::vec3 v2 = glm::cross(v1, lightDirVS);
					//	// optimized the obove lines into the following expression:
					//	const float v2 = lightDirVS.x * lightDirVS.x + lightDirVS.y * lightDirVS.y;
					//	// cone vertex
					//	const float p0 = -lightDepthVS;
					//	// first point on cone cap rim
					//	const float p1 = -lightDepthVS + radius * (cosAngle * lightDirVS.z + sinAngle * v2);
					//	// second point on cone cap rim
					//	const float p2 = -lightDepthVS + radius * (cosAngle * lightDirVS.z + sinAngle * -v2);
					//
					//	nearestPoint = -glm::max(p0, p1, p2);
					//	furthestPoint = -glm::min(p0, p1, p2);
					//}
					//// point light
					//else
					{
						nearestPoint = -lightDepthVS - radius;
						furthestPoint = -lightDepthVS + radius;
					}

					size_t minBin = glm::min(static_cast<size_t>(glm::max(nearestPoint / RendererConsts::Z_BIN_DEPTH, 0.0f)), size_t(RendererConsts::Z_BINS - 1));
					size_t maxBin = glm::min(static_cast<size_t>(glm::max(furthestPoint / RendererConsts::Z_BIN_DEPTH, 0.0f)), size_t(RendererConsts::Z_BINS - 1));

					for (size_t j = minBin; j <= maxBin; ++j)
					{
						uint32_t &val = depthBins[j];
						uint32_t minIndex = (val & 0xFFFF0000) >> 16;
						uint32_t maxIndex = val & 0xFFFF;
						minIndex = std::min(minIndex, static_cast<uint32_t>(i));
						maxIndex = std::max(maxIndex, static_cast<uint32_t>(i));
						val = ((minIndex & 0xFFFF) << 16) | (maxIndex & 0xFFFF);
					}
				}
			}

			// global participating media
			{
				m_entityRegistry.view<GlobalParticipatingMediumComponent, RenderableComponent>().each(
					[&](GlobalParticipatingMediumComponent &mediumComponent, RenderableComponent &)
					{
						GlobalParticipatingMedium medium{};
						medium.m_emissive = mediumComponent.m_emissiveColor * mediumComponent.m_emissiveIntensity;
						medium.m_extinction = mediumComponent.m_extinction;
						medium.m_scattering = mediumComponent.m_albedo * mediumComponent.m_extinction;
						medium.m_phase = mediumComponent.m_phaseAnisotropy;
						medium.m_heightFogEnabled = mediumComponent.m_heightFogEnabled;
						medium.m_heightFogStart = mediumComponent.m_heightFogStart;
						medium.m_heightFogFalloff = mediumComponent.m_heightFogFalloff;
						medium.m_maxHeight = mediumComponent.m_maxHeight;
						medium.m_textureScale = mediumComponent.m_textureScale;
						medium.m_textureBias = mediumComponent.m_textureBias;
						medium.m_densityTexture = mediumComponent.m_densityTexture.m_handle;


						m_lightData.m_globalParticipatingMedia.push_back(medium);
					});
			}

			// local participating media
			{
				std::vector<float> mediumRadii;

				m_entityRegistry.view<TransformationComponent, LocalParticipatingMediumComponent, RenderableComponent>().each(
					[&](TransformationComponent &transformationComponent, LocalParticipatingMediumComponent &mediumComponent, RenderableComponent &)
					{
						glm::mat4 worldToLocalTransposed =
							glm::transpose(glm::scale(1.0f / transformationComponent.m_scale)
								* glm::mat4_cast(glm::inverse(transformationComponent.m_orientation))
								* glm::translate(-transformationComponent.m_position));

						LocalParticipatingMedium medium{};
						medium.m_worldToLocal0 = worldToLocalTransposed[0];
						medium.m_worldToLocal1 = worldToLocalTransposed[1];
						medium.m_worldToLocal2 = worldToLocalTransposed[2];
						medium.m_position = transformationComponent.m_position;
						medium.m_emissive = mediumComponent.m_emissiveColor * mediumComponent.m_emissiveIntensity;
						medium.m_extinction = mediumComponent.m_extinction;
						medium.m_scattering = mediumComponent.m_albedo * mediumComponent.m_extinction;
						medium.m_phase = mediumComponent.m_phaseAnisotropy;
						medium.m_heightFogStart = mediumComponent.m_heightFogEnabled ? mediumComponent.m_heightFogStart : 1.0f;
						medium.m_heightFogFalloff = mediumComponent.m_heightFogFalloff;
						medium.m_textureScale = mediumComponent.m_textureScale;
						medium.m_textureBias = mediumComponent.m_textureBias;
						medium.m_densityTexture = mediumComponent.m_densityTexture.m_handle;
						medium.m_spherical = mediumComponent.m_spherical;

						m_lightData.m_localParticipatingMedia.push_back(medium);

						m_lightData.m_localMediaTransforms.push_back(glm::translate(transformationComponent.m_position) * glm::mat4_cast(transformationComponent.m_orientation) * glm::scale(transformationComponent.m_scale));
						m_lightData.m_localMediaOrder.push_back(static_cast<uint32_t>(m_lightData.m_localMediaOrder.size()));
						mediumRadii.push_back(glm::length(transformationComponent.m_scale));
					});

				// sort by distance to camera
				std::sort(m_lightData.m_localMediaOrder.begin(), m_lightData.m_localMediaOrder.end(), [&](const uint32_t &lhs, const uint32_t &rhs)
					{
						return -glm::dot(glm::vec4(m_lightData.m_localParticipatingMedia[lhs].m_position, 1.0f), viewMatDepthRow) < -glm::dot(glm::vec4(m_lightData.m_localParticipatingMedia[rhs].m_position, 1.0f), viewMatDepthRow);
					});

				// clear bins
				for (size_t i = 0; i < m_lightData.m_localMediaDepthBins.size(); ++i)
				{
					const uint32_t emptyBin = ((~0u & 0xFFFFu) << 16u);
					m_lightData.m_localMediaDepthBins[i] = emptyBin;
				}

				// assign lights to bins
				for (size_t i = 0; i < m_lightData.m_localParticipatingMedia.size(); ++i)
				{
					const auto &media = m_lightData.m_localParticipatingMedia[m_lightData.m_localMediaOrder[i]];
					const float radius = mediumRadii[m_lightData.m_localMediaOrder[i]];
					float lightDepthVS = glm::dot(glm::vec4(media.m_position, 1.0f), viewMatDepthRow);
					float nearestPoint = -lightDepthVS - radius;
					float furthestPoint = -lightDepthVS + radius;

					size_t minBin = glm::min(static_cast<size_t>(glm::max(nearestPoint / RendererConsts::Z_BIN_DEPTH, 0.0f)), size_t(RendererConsts::Z_BINS - 1));
					size_t maxBin = glm::min(static_cast<size_t>(glm::max(furthestPoint / RendererConsts::Z_BIN_DEPTH, 0.0f)), size_t(RendererConsts::Z_BINS - 1));

					for (size_t j = minBin; j <= maxBin; ++j)
					{
						uint32_t &val = m_lightData.m_localMediaDepthBins[j];
						uint32_t minIndex = (val & 0xFFFF0000) >> 16;
						uint32_t maxIndex = val & 0xFFFF;
						minIndex = std::min(minIndex, static_cast<uint32_t>(i));
						maxIndex = std::max(maxIndex, static_cast<uint32_t>(i));
						val = ((minIndex & 0xFFFF) << 16) | (maxIndex & 0xFFFF);
					}
				}
			}
		}

		// update all transforms
		{
			auto view = m_entityRegistry.view<TransformationComponent>();
			view.each([&](TransformationComponent &transformationComponent)
				{
					const glm::mat4 rotationMatrix = glm::mat4_cast(transformationComponent.m_orientation);
					const glm::mat4 scaleMatrix = glm::scale(transformationComponent.m_scale);
					transformationComponent.m_previousTransformation = transformationComponent.m_transformation;
					transformationComponent.m_transformation = glm::translate(transformationComponent.m_position) * rotationMatrix * scaleMatrix;
				});
		}

		// generate draw lists
		{
			auto view = m_entityRegistry.view<MeshComponent, TransformationComponent, RenderableComponent>();

			view.each([&](MeshComponent &meshComponent, TransformationComponent &transformationComponent, RenderableComponent &)
				{
					glm::quat transposeInverseRotation = glm::normalize(glm::quat_cast(glm::transpose(glm::inverse(glm::mat3(transformationComponent.m_transformation)))));

					for (const auto &p : meshComponent.m_subMeshMaterialPairs)
					{
						const auto &aabb = m_aabbs[p.first.m_handle];
						const glm::mat4 localScale = glm::scale(aabb.m_max - aabb.m_min);
						const glm::mat4 localBias = glm::translate(aabb.m_min);

						const uint32_t transformIndex = static_cast<uint32_t>(m_transformData.size() / 4);

						glm::mat4 transform = glm::transpose(transformationComponent.m_transformation * localBias * localScale);

						m_transformData.push_back(transform[0]);
						m_transformData.push_back(transform[1]);
						m_transformData.push_back(transform[2]);
						m_transformData.push_back(glm::vec4(transposeInverseRotation.x, transposeInverseRotation.y, transposeInverseRotation.z, transposeInverseRotation.w));

						SubMeshInstanceData instanceData;
						instanceData.m_subMeshIndex = p.first.m_handle;
						instanceData.m_transformIndex = transformIndex;
						instanceData.m_materialIndex = p.second.m_handle;

						const bool staticMobility = transformationComponent.m_mobility == TransformationComponent::Mobility::STATIC;
						uint32_t contentTypeMask = 0;

						switch (m_materialBatchAssignment[p.second.m_handle])
						{
						case 0: // Opaque
							contentTypeMask = staticMobility ? FrustumCullData::STATIC_OPAQUE_CONTENT_TYPE_BIT : FrustumCullData::DYNAMIC_OPAQUE_CONTENT_TYPE_BIT;
							break;
						case 1: // Alpha tested
							contentTypeMask = staticMobility ? FrustumCullData::STATIC_ALPHA_TESTED_CONTENT_TYPE_BIT : FrustumCullData::DYNAMIC_ALPHA_TESTED_CONTENT_TYPE_BIT;
							break;
						case 2: // transparent
							contentTypeMask = staticMobility ? FrustumCullData::STATIC_TRANSPARENT_CONTENT_TYPE_BIT : FrustumCullData::DYNAMIC_TRANSPARENT_CONTENT_TYPE_BIT;
							break;
						default:
							assert(false);
							break;
						}

						const glm::vec4 boundingSphere = m_boundingSpheres[p.first.m_handle];
						const glm::vec3 boundingSpherePos = transformationComponent.m_position + transformationComponent.m_orientation * (transformationComponent.m_scale.x * glm::vec3(boundingSphere));
						const float boundingSphereRadius = boundingSphere.w * transformationComponent.m_scale.x;

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

								switch (m_materialBatchAssignment[p.second.m_handle])
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

								float viewSpaceDepth = -glm::dot(cullData.m_viewMatrixDepthRow, glm::vec4(boundingSpherePos, 1.0f));
								const uint32_t depth = 0;// (1.0f - glm::clamp(viewSpaceDepth / cullData.m_depthRange, 0.0f, 1.0f)) *createMask(22);

								// key:
								// [drawListIdx 8][type 2][depth 22][instanceIdx 32]
								uint64_t drawCallKey = 0;
								drawCallKey |= (cullData.m_renderListIndex & createMask(8)) << 56;
								drawCallKey |= (m_materialBatchAssignment[p.second.m_handle] & createMask(2)) << 54;
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

		std::vector<BillboardDrawData> billboardDrawData;

		// billboards
		{
			m_entityRegistry.view<EditorBillboardComponent, TransformationComponent>().each(
				[&](EditorBillboardComponent &billboardComponent, TransformationComponent &transformationComponent)
				{
					BillboardDrawData drawData{};
					drawData.m_position = transformationComponent.m_position;
					drawData.m_scale = billboardComponent.m_scale;
					drawData.m_opacity = billboardComponent.m_opacity;
					drawData.m_texture = billboardComponent.m_texture;

					billboardDrawData.push_back(drawData);
				});

			m_entityRegistry.view<BillboardComponent, TransformationComponent, RenderableComponent>().each(
				[&](BillboardComponent &billboardComponent, TransformationComponent &transformationComponent, RenderableComponent &)
				{
					BillboardDrawData drawData{};
					drawData.m_position = transformationComponent.m_position;
					drawData.m_scale = billboardComponent.m_scale;
					drawData.m_opacity = billboardComponent.m_opacity;
					drawData.m_texture = billboardComponent.m_texture;

					billboardDrawData.push_back(drawData);
				});

			std::sort(billboardDrawData.begin(), billboardDrawData.end(), [&](const BillboardDrawData &lhs, const BillboardDrawData &rhs)
				{
					return -glm::dot(glm::vec4(lhs.m_position, 1.0f), viewMatDepthRow) < -glm::dot(glm::vec4(rhs.m_position, 1.0f), viewMatDepthRow);
				});
		}

		m_commonRenderData.m_directionalLightCount = static_cast<uint32_t>(m_lightData.m_directionalLights.size());
		m_commonRenderData.m_directionalLightShadowedCount = static_cast<uint32_t>(m_lightData.m_directionalLightsShadowed.size());
		m_commonRenderData.m_directionalLightShadowedProbeCount = static_cast<uint32_t>(m_lightData.m_directionalLightsShadowedProbe.size());
		m_commonRenderData.m_punctualLightCount = static_cast<uint32_t>(m_lightData.m_punctualLights.size());
		m_commonRenderData.m_punctualLightShadowedCount = static_cast<uint32_t>(m_lightData.m_punctualLightsShadowed.size());
		m_commonRenderData.m_globalParticipatingMediaCount = static_cast<uint32_t>(m_lightData.m_globalParticipatingMedia.size());
		m_commonRenderData.m_localParticipatingMediaCount = static_cast<uint32_t>(m_lightData.m_localParticipatingMedia.size());
		m_commonRenderData.m_reflectionProbeCount = static_cast<uint32_t>(m_lightData.m_localReflectionProbes.size());

		m_particleEmitterManager->update(timeDelta, m_commonRenderData.m_viewMatrix);

		DebugDrawData debugDrawData{};
		debugDrawData.m_vertexCounts[DebugDrawData::LINE] = (uint32_t)m_debugLineVertices.size();
		debugDrawData.m_vertexCounts[DebugDrawData::VISIBLE_LINE] = (uint32_t)m_debugLineVisibleVertices.size();
		debugDrawData.m_vertexCounts[DebugDrawData::HIDDEN_LINE] = (uint32_t)m_debugLineHiddenVertices.size();
		debugDrawData.m_vertexCounts[DebugDrawData::TRIANGLE] = (uint32_t)m_debugTriangleVertices.size();
		debugDrawData.m_vertexCounts[DebugDrawData::VISIBLE_TRIANGLE] = (uint32_t)m_debugTriangleVisibleVertices.size();
		debugDrawData.m_vertexCounts[DebugDrawData::HIDDEN_TRIANGLE] = (uint32_t)m_debugTriangleHiddenVertices.size();
		debugDrawData.m_vertices[DebugDrawData::LINE] = m_debugLineVertices.data();
		debugDrawData.m_vertices[DebugDrawData::VISIBLE_LINE] = m_debugLineVisibleVertices.data();
		debugDrawData.m_vertices[DebugDrawData::HIDDEN_LINE] = m_debugLineHiddenVertices.data();
		debugDrawData.m_vertices[DebugDrawData::TRIANGLE] = m_debugTriangleVertices.data();
		debugDrawData.m_vertices[DebugDrawData::VISIBLE_TRIANGLE] = m_debugTriangleVisibleVertices.data();
		debugDrawData.m_vertices[DebugDrawData::HIDDEN_TRIANGLE] = m_debugTriangleHiddenVertices.data();

		RenderData renderData;
		renderData.m_transformDataCount = static_cast<uint32_t>(m_transformData.size());
		renderData.m_transformData = m_transformData.data();
		renderData.m_shadowMatrixCount = static_cast<uint32_t>(m_shadowMatrices.size());
		renderData.m_shadowMatrices = m_shadowMatrices.data();
		renderData.m_shadowCascadeParams = m_shadowCascadeParams.data();
		renderData.m_probeViewProjectionMatrices = probeMatrices;
		renderData.m_probeRenderCount = probeRenderCount;
		renderData.m_probeRenderIndices = probeRenderIndices;
		renderData.m_probeRelightCount = probeRelightCount;
		renderData.m_probeRelightIndices = probeRelightIndices;
		renderData.m_subMeshInstanceDataCount = static_cast<uint32_t>(m_subMeshInstanceData.size());
		renderData.m_subMeshInstanceData = m_subMeshInstanceData.data();
		renderData.m_drawCallKeys = drawCallKeys.data();
		renderData.m_mainViewRenderListIndex = 0;
		renderData.m_probeDrawListOffset = probeDrawListOffset;
		renderData.m_shadowCascadeViewRenderListOffset = shadowCascadeRenderListOffset;
		renderData.m_shadowCascadeViewRenderListCount = shadowCascadeRenderListCount;
		renderData.m_probeShadowViewRenderListOffset = probeShadowsRenderListOffset;
		renderData.m_probeShadowViewRenderListCount = probeShadowRenderListCount;
		renderData.m_renderLists = renderLists.data();
		renderData.m_texCoordScaleBias = m_texCoordScaleBias.get();
		renderData.m_debugDrawData = &debugDrawData;
		renderData.m_billboardCount = (uint32_t)billboardDrawData.size();
		renderData.m_billboardDrawData = billboardDrawData.data();

		m_particleEmitterManager->getParticleDrawData(renderData.m_particleDataDrawListCount, renderData.m_particleDrawDataLists, renderData.m_particleDrawDataListSizes);

		m_renderer->render(m_commonRenderData, renderData, m_lightData);

		m_debugLineVertices.clear();
		m_debugLineVisibleVertices.clear();
		m_debugLineHiddenVertices.clear();
		m_debugTriangleVertices.clear();
		m_debugTriangleVisibleVertices.clear();
		m_debugTriangleHiddenVertices.clear();

		++m_commonRenderData.m_frame;
		//float t = 0.0f;
		//if (m_bvh.trace(m_commonRenderData.m_cameraPosition + m_commonRenderData.m_cameraDirection * m_commonRenderData.m_nearPlane, m_commonRenderData.m_cameraDirection, t))
		//{
		//	printf("%f\n", t);
		//}
	}
}

VEngine::Texture2DHandle VEngine::RenderSystem::createTexture(const char *filepath)
{
	return m_renderer->loadTexture(filepath);
}

VEngine::Texture3DHandle VEngine::RenderSystem::createTexture3D(const char *filepath)
{
	return m_renderer->loadTexture3D(filepath);
}

void VEngine::RenderSystem::destroyTexture(Texture2DHandle handle)
{
	m_renderer->freeTexture(handle);
}

void VEngine::RenderSystem::destroyTexture(Texture3DHandle handle)
{
	m_renderer->freeTexture(handle);
}

void VEngine::RenderSystem::updateTextureData()
{
	m_renderer->updateTextureData();
}

void VEngine::RenderSystem::updateTexture3DData()
{
	m_renderer->updateTexture3DData();
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
		m_aabbs[handles[i].m_handle] = { minC, maxC };
		const glm::vec3 center = minC + (maxC - minC) * 0.5f;
		const float radius = glm::distance(center, maxC);
		m_boundingSpheres[handles[i].m_handle] = glm::vec4(center, radius);

		m_texCoordScaleBias[handles[i].m_handle] = glm::vec4(subMeshes[i].m_maxTexCoord - subMeshes[i].m_minTexCoord, subMeshes[i].m_minTexCoord);
	}

	// create BVH
	//{
	//	std::vector<glm::vec3> vertices;
	//	for (size_t i = 0; i < count; ++i)
	//	{
	//		glm::vec3 *positions = (glm::vec3 *)subMeshes[i].m_positions;
	//		for (size_t j = 0; j < subMeshes[i].m_indexCount; ++j)
	//		{
	//			vertices.push_back(positions[subMeshes[i].m_indices[j]]);
	//		}
	//	}
	//	m_bvh.build(vertices.size() / 3, vertices.data(), 3);
	//	printf("BVH Depth: %d\n", m_bvh.getDepth());
	//	assert(m_bvh.validate());
	//	m_renderer->setBVH(m_bvh.getNodes().size(), m_bvh.getNodes().data(), m_bvh.getTriangles().size(), m_bvh.getTriangles().data());
	//}
}

void VEngine::RenderSystem::destroySubMeshes(uint32_t count, SubMeshHandle *handles)
{
	m_renderer->destroySubMeshes(count, handles);
}

entt::entity VEngine::RenderSystem::setCameraEntity(entt::entity cameraEntity)
{
	auto oldCamera = m_cameraEntity;
	m_cameraEntity = cameraEntity;
	return oldCamera;
}

entt::entity VEngine::RenderSystem::getCameraEntity() const
{
	return m_cameraEntity;
}

const uint32_t *VEngine::RenderSystem::getLuminanceHistogram() const
{
	return m_renderer->getLuminanceHistogram();
}

void VEngine::RenderSystem::getTimingInfo(size_t *count, const PassTimingInfo **data) const
{
	m_renderer->getTimingInfo(count, data);
}

void VEngine::RenderSystem::getOcclusionCullingStats(uint32_t &draws, uint32_t &totalDraws) const
{
	m_renderer->getOcclusionCullingStats(draws, totalDraws);
}

void VEngine::RenderSystem::resize(uint32_t width, uint32_t height, bool fullscreen, bool vsync)
{
	m_width = width;
	m_height = height;
	m_renderer->resize(width, height, fullscreen, vsync);
}

void VEngine::RenderSystem::resize(uint32_t editorViewportWidth, uint32_t editorViewportHeight, uint32_t swapChainWidth, uint32_t swapChainHeight, bool fullscreen, bool vsync)
{
	m_width = editorViewportWidth;
	m_height = editorViewportHeight;
	m_swapChainWidth = swapChainWidth;
	m_swapChainHeight = swapChainHeight;
	m_renderer->resize(m_width, m_height, m_swapChainWidth, m_swapChainHeight, fullscreen, vsync);
}

void VEngine::RenderSystem::setEditorMode(bool editorMode)
{
	m_renderer->setEditorMode(editorMode);
}

void VEngine::RenderSystem::initEditorImGuiCtx(ImGuiContext *editorImGuiCtx)
{
	m_renderer->initEditorImGuiCtx(editorImGuiCtx);
}

VEngine::Texture2DHandle VEngine::RenderSystem::getEditorSceneTextureHandle()
{
	return m_renderer->getEditorSceneTextureHandle();
}

void VEngine::RenderSystem::drawDebugLine(const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec4 &color0, const glm::vec4 &color1)
{
	m_debugLineVertices.push_back({ glm::vec4(position0, 1.0f), color0 });
	m_debugLineVertices.push_back({ glm::vec4(position1, 1.0f), color1 });
}

void VEngine::RenderSystem::drawDebugLineVisible(const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec4 &color0, const glm::vec4 &color1)
{
	m_debugLineVisibleVertices.push_back({ glm::vec4(position0, 1.0f), color0 });
	m_debugLineVisibleVertices.push_back({ glm::vec4(position1, 1.0f), color1 });
}

void VEngine::RenderSystem::drawDebugLineHidden(const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec4 &color0, const glm::vec4 &color1)
{
	m_debugLineHiddenVertices.push_back({ glm::vec4(position0, 1.0f), color0 });
	m_debugLineHiddenVertices.push_back({ glm::vec4(position1, 1.0f), color1 });
}

void VEngine::RenderSystem::drawDebugTriangle(const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec3 &position2, const glm::vec4 &color0, const glm::vec4 &color1, const glm::vec4 &color2)
{
	m_debugTriangleVertices.push_back({ glm::vec4(position0, 1.0f), color0 });
	m_debugTriangleVertices.push_back({ glm::vec4(position1, 1.0f), color1 });
	m_debugTriangleVertices.push_back({ glm::vec4(position2, 1.0f), color2 });
}

void VEngine::RenderSystem::drawDebugTriangleVisible(const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec3 &position2, const glm::vec4 &color0, const glm::vec4 &color1, const glm::vec4 &color2)
{
	m_debugTriangleVisibleVertices.push_back({ glm::vec4(position0, 1.0f), color0 });
	m_debugTriangleVisibleVertices.push_back({ glm::vec4(position1, 1.0f), color1 });
	m_debugTriangleVisibleVertices.push_back({ glm::vec4(position2, 1.0f), color2 });
}

void VEngine::RenderSystem::drawDebugTriangleHidden(const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec3 &position2, const glm::vec4 &color0, const glm::vec4 &color1, const glm::vec4 &color2)
{
	m_debugTriangleHiddenVertices.push_back({ glm::vec4(position0, 1.0f), color0 });
	m_debugTriangleHiddenVertices.push_back({ glm::vec4(position1, 1.0f), color1 });
	m_debugTriangleHiddenVertices.push_back({ glm::vec4(position2, 1.0f), color2 });
}

void VEngine::RenderSystem::updateMaterialBatchAssigments(size_t count, const Material *materials, MaterialHandle *handles)
{
	for (size_t i = 0; i < count; ++i)
	{
		m_materialBatchAssignment[handles[i].m_handle] = static_cast<uint8_t>(materials[i].m_alpha);
	}
}

void VEngine::RenderSystem::calculateCascadeViewProjectionMatrices(const glm::vec3 &lightDir,
	float maxShadowDistance,
	float splitLambda,
	float shadowTextureSize,
	size_t cascadeCount,
	glm::mat4 *viewProjectionMatrices,
	glm::vec4 *cascadeParams,
	glm::vec4 *viewMatrixDepthRows)
{
	float splits[DirectionalLightComponent::MAX_CASCADES];

	assert(cascadeCount <= DirectionalLightComponent::MAX_CASCADES);

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

	const float aspectRatio = m_commonRenderData.m_width * (1.0f / m_commonRenderData.m_height);
	float previousSplit = nearPlane;
	for (size_t i = 0; i < cascadeCount; ++i)
	{
		// https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html
		const float n = previousSplit;
		const float f = splits[i];
		const float k = glm::sqrt(1.0f + aspectRatio * aspectRatio) * glm::tan(m_commonRenderData.m_fovy * 0.5f);

		glm::vec3 center;
		float radius;
		const float k2 = k * k;
		const float fnSum = f + n;
		const float fnDiff = f - n;
		const float fnSum2 = fnSum * fnSum;
		const float fnDiff2 = fnDiff * fnDiff;
		if (k2 >= fnDiff / fnSum)
		{
			center = glm::vec3(0.0f, 0.0f, -f);
			radius = f * k;
		}
		else
		{
			center = glm::vec3(0.0f, 0.0f, -0.5f * fnSum * (1.0f + k2));
			radius = 0.5f * sqrt(fnDiff2 + 2 * (f * f + n * n) * k2 + fnSum2 * k2 * k2);
		}
		glm::vec4 tmp = m_commonRenderData.m_invViewMatrix * glm::vec4(center, 1.0f);
		center = glm::vec3(tmp / tmp.w);

		glm::vec3 upDir(0.0f, 1.0f, 0.0f);

		// choose different up vector if light direction would be linearly dependent otherwise
		if (abs(lightDir.x) < 0.001 && abs(lightDir.z) < 0.001)
		{
			upDir = glm::vec3(1.0f, 1.0f, 0.0f);
		}

		glm::mat4 lightView = glm::lookAt(center + lightDir * 150.0f, center, upDir);

		constexpr float depthRange = 300.0f;
		constexpr float precisionRange = 65536.0f;

		// snap to shadow map texel to avoid shimmering
		lightView[3].x -= fmodf(lightView[3].x, (radius / 256.0f) * 2.0f);
		lightView[3].y -= fmodf(lightView[3].y, (radius / 256.0f) * 2.0f);
		lightView[3].z -= fmodf(lightView[3].z, depthRange / precisionRange);

		viewMatrixDepthRows[i] = glm::vec4(lightView[0][2], lightView[1][2], lightView[2][2], lightView[3][2]);

		viewProjectionMatrices[i] = glm::ortho(-radius, radius, -radius, radius, 0.0f, depthRange) * lightView;

		// depthNormalBiases[i] holds the depth/normal offset biases in texel units
		const float unitsPerTexel = radius * 2.0f / shadowTextureSize;
		cascadeParams[i].x = unitsPerTexel * -cascadeParams[i].x / depthRange;
		cascadeParams[i].y = unitsPerTexel * cascadeParams[i].y;
		cascadeParams[i].z = 1.0f / unitsPerTexel;

		previousSplit = splits[i];
	}
}