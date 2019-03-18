#include "RenderSystem.h"
#include "ECS/EntityManager.h"
#include "ECS/Component/MeshComponent.h"
#include "ECS/Component/TransformationComponent.h"
#include "ECS/Component/RenderableComponent.h"
#include "ECS/Component/PointLightComponent.h"
#include "ECS/Component/CameraComponent.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "GlobalVar.h"
#include <glm/ext.hpp>
#include <glm/vec3.hpp>
#include <algorithm>

VEngine::RenderSystem::RenderSystem(EntityManager &entityManager)
	:m_entityManager(entityManager),
	m_cameraEntity(),
	m_renderParams(),
	m_drawLists(),
	m_lightData()
{
}

void VEngine::RenderSystem::init()
{
	m_renderer.reset(new VKRenderer());
	m_renderer->init(g_windowWidth, g_windowHeight);
}

void VEngine::RenderSystem::input(double time, double timeDelta)
{
}

void VEngine::RenderSystem::update(double time, double timeDelta)
{
	m_drawLists.m_opaqueItems.clear();
	m_drawLists.m_maskedItems.clear();
	m_drawLists.m_blendedItems.clear();

	m_lightData.m_shadowData.clear();
	m_lightData.m_directionalLightData.clear();
	m_lightData.m_pointLightData.clear();
	m_lightData.m_spotLightData.clear();
	m_lightData.m_shadowJobs.clear();

	if (m_cameraEntity)
	{
		// update render params
		{
			CameraComponent *cameraComponent = m_entityManager.getComponent<CameraComponent>(m_cameraEntity);
			TransformationComponent *transformationComponent = m_entityManager.getComponent<TransformationComponent>(m_cameraEntity);

			glm::mat4 &viewMatrix = cameraComponent->m_viewMatrix;
			glm::mat4 &projectionMatrix = cameraComponent->m_projectionMatrix;

			m_renderParams.m_time = static_cast<float>(time);
			m_renderParams.m_fovy = cameraComponent->m_fovy;
			m_renderParams.m_nearPlane = cameraComponent->m_near;
			m_renderParams.m_farPlane = cameraComponent->m_far;
			m_renderParams.m_prevViewMatrix = m_renderParams.m_viewMatrix;
			m_renderParams.m_prevProjectionMatrix = m_renderParams.m_projectionMatrix;
			m_renderParams.m_prevViewProjectionMatrix = m_renderParams.m_viewProjectionMatrix;
			m_renderParams.m_prevInvViewMatrix = m_renderParams.m_invViewMatrix;
			m_renderParams.m_prevInvProjectionMatrix = m_renderParams.m_invProjectionMatrix;
			m_renderParams.m_prevInvViewProjectionMatrix = m_renderParams.m_invViewProjectionMatrix;
			m_renderParams.m_viewMatrix = viewMatrix;
			m_renderParams.m_projectionMatrix = projectionMatrix;
			m_renderParams.m_viewProjectionMatrix = projectionMatrix * viewMatrix;
			m_renderParams.m_invViewMatrix = glm::inverse(viewMatrix);
			m_renderParams.m_invProjectionMatrix = glm::inverse(projectionMatrix);
			m_renderParams.m_invViewProjectionMatrix = glm::inverse(m_renderParams.m_viewProjectionMatrix);
			m_renderParams.m_cameraPosition = glm::vec4(transformationComponent->m_position, 1.0f);
			m_renderParams.m_cameraDirection = -glm::vec4(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2], 1.0f);
			m_renderParams.m_width = g_windowWidth;
			m_renderParams.m_height = g_windowHeight;
			m_renderParams.m_timeDelta = static_cast<float>(timeDelta);
		}


		// update all transformations and generate draw lists
		{
			m_entityManager.each<MeshComponent, TransformationComponent, RenderableComponent>(
				[this](const Entity *entity, MeshComponent &meshComponent, TransformationComponent &transformationComponent, RenderableComponent&)
			{
				transformationComponent.m_prevTransformation = transformationComponent.m_transformation;
				transformationComponent.m_transformation = glm::translate(transformationComponent.m_position)
					* glm::mat4_cast(transformationComponent.m_rotation)
					* glm::scale(glm::mat4(), glm::vec3(transformationComponent.m_scale));

				for (const auto &subMesh : meshComponent.m_mesh.m_subMeshes)
				{
					DrawItem drawItem = {};
					drawItem.m_vertexOffset = subMesh.m_vertexOffset;
					drawItem.m_baseIndex = subMesh.m_indexOffset / sizeof(uint32_t);
					drawItem.m_indexCount = subMesh.m_indexCount;
					drawItem.m_perDrawData.m_albedoFactorMetallic = glm::vec4(subMesh.m_material.m_albedoFactor, subMesh.m_material.m_metallicFactor);
					drawItem.m_perDrawData.m_emissiveFactorRoughness = glm::vec4(subMesh.m_material.m_emissiveFactor, subMesh.m_material.m_roughnessFactor);
					drawItem.m_perDrawData.m_modelMatrix = transformationComponent.m_transformation;
					drawItem.m_perDrawData.m_albedoTexture = subMesh.m_material.m_albedoTexture;
					drawItem.m_perDrawData.m_normalTexture = subMesh.m_material.m_normalTexture;
					drawItem.m_perDrawData.m_metallicTexture = subMesh.m_material.m_metallicTexture;
					drawItem.m_perDrawData.m_roughnessTexture = subMesh.m_material.m_roughnessTexture;
					drawItem.m_perDrawData.m_occlusionTexture = subMesh.m_material.m_occlusionTexture;
					drawItem.m_perDrawData.m_emissiveTexture = subMesh.m_material.m_emissiveTexture;
					drawItem.m_perDrawData.m_displacementTexture = subMesh.m_material.m_displacementTexture;

					switch (subMesh.m_material.m_alpha)
					{
					case Material::Alpha::OPAQUE:
						m_drawLists.m_opaqueItems.push_back(drawItem);
						break;
					case Material::Alpha::MASKED:
						m_drawLists.m_maskedItems.push_back(drawItem);
						break;
					case Material::Alpha::BLENDED:
						m_drawLists.m_blendedItems.push_back(drawItem);
						break;
					default:
						assert(false);
					}
				}
			});
		}

		// generate light data
		{
			glm::mat4 proj = glm::transpose(m_renderParams.m_projectionMatrix);
			glm::vec4 frustumPlaneEquations[] =
			{
				glm::normalize(proj[3] + proj[0]),	// left
				glm::normalize(proj[3] - proj[0]),	// right
				glm::normalize(proj[3] + proj[1]),	// bottom
				glm::normalize(proj[3] - proj[1]),	// top
				glm::normalize(proj[2]),			// near
				glm::normalize(proj[3] - proj[2])	// far
			};

			m_entityManager.each<TransformationComponent, PointLightComponent, RenderableComponent>(
				[this, &frustumPlaneEquations](const Entity *entity, TransformationComponent &transformationComponent, PointLightComponent &pointLightComponent, RenderableComponent&)
			{
				PointLightData pl = {};
				glm::vec3 pos = glm::vec3(m_renderParams.m_viewMatrix * glm::vec4(transformationComponent.m_position, 1.0f));
				pl.m_positionRadius = glm::vec4(pos, pointLightComponent.m_radius);
				float intensity = pointLightComponent.m_luminousPower * (1.0f / (4.0f * glm::pi<float>()));
				pl.m_colorInvSqrAttRadius = glm::vec4(pointLightComponent.m_color * intensity, 1.0f / (pointLightComponent.m_radius * pointLightComponent.m_radius));

				// frustum cull
				for (const auto &plane : frustumPlaneEquations)
				{
					if (glm::dot(glm::vec4(pos, 1.0f), plane) <= -pl.m_positionRadius.w)
					{
						return;
					}
				}

				m_lightData.m_pointLightData.push_back(pl);
			});

			std::sort(m_lightData.m_pointLightData.begin(), m_lightData.m_pointLightData.end(),
				[](const PointLightData &lhs, const PointLightData &rhs)
			{
				return (-lhs.m_positionRadius.z - lhs.m_positionRadius.w) < (-rhs.m_positionRadius.z - rhs.m_positionRadius.w);
			});

			const float binDepth = 1.0f;
			const unsigned int emptyBin = ((~0 & 0xFFFF) << 16);

			// clear bins
			for (size_t i = 0; i < m_lightData.m_zBins.size(); ++i)
			{
				m_lightData.m_zBins[i] = emptyBin;
			}

			// assign lights
			for (size_t i = 0; i < m_lightData.m_pointLightData.size(); ++i)
			{
				glm::vec4 &posRadius = m_lightData.m_pointLightData[i].m_positionRadius;
				float nearestPoint = -posRadius.z - posRadius.w;
				float furthestPoint = -posRadius.z + posRadius.w;

				size_t minBin = glm::clamp(static_cast<size_t>(glm::max(nearestPoint / binDepth, 0.0f)), size_t(0), size_t(8191));
				size_t maxBin = glm::clamp(static_cast<size_t>(glm::max(furthestPoint / binDepth, 0.0f)), size_t(0), size_t(8191));

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

			m_lightData.m_directionalLightData.push_back(
				{
					glm::vec4(glm::vec3(100.0f), 1.0f),
					glm::vec4(glm::normalize(glm::vec3(0.1f, 3.0f, -1.0f)), 1.0f),
					0,
					3
				}
			);

			glm::mat4 matrices[3];
			calculateCascadeViewProjectionMatrices(m_renderParams, glm::normalize(glm::vec3(0.1f, 3.0f, -1.0f)), 0.1f, 30.0f, 0.7f, 2048, 3, matrices);

			m_lightData.m_shadowData.push_back({ matrices[0], { 0.25f, 0.25f, 0.0f, 0.0f } });
			m_lightData.m_shadowData.push_back({ matrices[1], { 0.25f, 0.25f,  0.25f, 0.0f } });
			m_lightData.m_shadowData.push_back({ matrices[2], { 0.25f, 0.25f, 0.0f,  0.25f } });
			//m_lightData.m_shadowData.push_back({ matrices[3], { 0.25f, 0.25f, 1.0f, 1.0f } });

			m_lightData.m_shadowJobs.push_back({ matrices[0], 0, 0, 2048 });
			m_lightData.m_shadowJobs.push_back({ matrices[1], 2048, 0, 2048 });
			m_lightData.m_shadowJobs.push_back({ matrices[2], 0, 2048, 2048 });
			//m_lightData.m_shadowJobs.push_back({ matrices[3], 2048, 2048, 2048 });
		}

		m_renderParams.m_directionalLightCount = static_cast<uint32_t>(m_lightData.m_directionalLightData.size());
		m_renderParams.m_pointLightCount = static_cast<uint32_t>(m_lightData.m_pointLightData.size());
	}
}

void VEngine::RenderSystem::render()
{
	if (m_cameraEntity)
	{
		m_renderer->render(m_renderParams, m_drawLists, m_lightData);
		++m_renderParams.m_frame;
	}
}

void VEngine::RenderSystem::reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize)
{
	m_renderer->reserveMeshBuffers(vertexSize, indexSize);
}

void VEngine::RenderSystem::uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize)
{
	m_renderer->uploadMeshData(vertices, vertexSize, indices, indexSize);
}

uint32_t VEngine::RenderSystem::createTexture(const char *filepath)
{
	return m_renderer->loadTexture(filepath);
}

void VEngine::RenderSystem::updateTextureData()
{
	m_renderer->updateTextureData();
}

void VEngine::RenderSystem::setCameraEntity(const Entity *cameraEntity)
{
	m_cameraEntity = cameraEntity;
}

const VEngine::Entity *VEngine::RenderSystem::getCameraEntity() const
{
	return m_cameraEntity;
}

void VEngine::RenderSystem::calculateCascadeViewProjectionMatrices(
	const RenderParams &renderParams,
	const glm::vec3 &lightDir,
	float nearPlane,
	float farPlane,
	float splitLambda,
	float shadowTextureSize,
	size_t cascadeCount,
	glm::mat4 *viewProjectionMatrices)
{
	const size_t maxShadowCascades = 4;
	float splits[maxShadowCascades];

	assert(cascadeCount <= maxShadowCascades);

	float range = farPlane - nearPlane;
	float ratio = farPlane / nearPlane;

	for (size_t i = 0; i < cascadeCount; ++i)
	{
		float p = (i + 1) / static_cast<float>(cascadeCount);
		float log = nearPlane * std::pow(ratio, p);
		float uniform = nearPlane + range * p;
		float d = splitLambda * (log - uniform) + uniform;
		splits[i] = (d - nearPlane) / range;
	}
	glm::mat4 vulkanCorrection =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, -1.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.5f, 0.0f },
	{ 0.0f, 0.0f, 0.5f, 1.0f }
	};


	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < cascadeCount; i++)
	{
		glm::mat4 projectionMatrix = glm::perspective(renderParams.m_fovy, 1600.0f / 900.0f, nearPlane + lastSplitDist * (farPlane - nearPlane), nearPlane + splits[i] * (farPlane - nearPlane));
		glm::mat4 invProjection = glm::inverse(projectionMatrix);

		glm::vec3 frustumCorners[8];
		frustumCorners[0] = glm::vec3(-1.0f, -1.0f, -1.0f); // xyz
		frustumCorners[1] = glm::vec3(1.0f, -1.0f, -1.0f); // Xyz
		frustumCorners[2] = glm::vec3(-1.0f, 1.0f, -1.0f); // xYz
		frustumCorners[3] = glm::vec3(1.0f, 1.0f, -1.0f); // XYz
		frustumCorners[4] = glm::vec3(-1.0f, -1.0f, 1.0f); // xyZ
		frustumCorners[5] = glm::vec3(1.0f, -1.0f, 1.0f); // XyZ
		frustumCorners[6] = glm::vec3(-1.0f, 1.0f, 1.0f); // xYZ
		frustumCorners[7] = glm::vec3(1.0f, 1.0f, 1.0f); // XYZ

		glm::vec3 minCorner = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 maxCorner = glm::vec3(std::numeric_limits<float>::lowest());

		for (size_t i = 0; i < 8; ++i)
		{
			glm::vec4 corner4 = invProjection * glm::vec4(frustumCorners[i], 1.0f);
			glm::vec3 corner = corner4 /= corner4.w;
			minCorner = glm::min(minCorner, corner);
			maxCorner = glm::max(maxCorner, corner);
		}

		float radius = glm::distance(minCorner, maxCorner) * 0.5f;
		glm::vec3 sphereCenter = (minCorner + maxCorner) * 0.5f;
		glm::vec3 target = renderParams.m_invViewMatrix * glm::vec4(sphereCenter, 1.0f);

		glm::vec3 upDir(0.0f, 1.0f, 0.0f);

		// choose different up vector if light direction would be linearly dependent otherwise
		if (abs(lightDir.x) < 0.001 && abs(lightDir.z) < 0.001)
		{
			upDir = glm::vec3(1.0f, 1.0f, 0.0f);
		}

		glm::mat4 lightView = glm::lookAt(target + lightDir * 150.0f, target - lightDir * 150.0f, upDir);

		lightView[3].x -= fmodf(lightView[3].x, (radius / static_cast<float>(shadowTextureSize)) * 2.0f);
		lightView[3].y -= fmodf(lightView[3].y, (radius / static_cast<float>(shadowTextureSize)) * 2.0f);

		viewProjectionMatrices[i] = vulkanCorrection * glm::ortho(-radius, radius, -radius, radius, 0.0f, 300.0f) * lightView;

		lastSplitDist = splits[i];
	}
}
