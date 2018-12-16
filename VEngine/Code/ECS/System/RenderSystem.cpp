#include "RenderSystem.h"
#include "ECS/EntityManager.h"
#include "ECS/Component/MeshComponent.h"
#include "ECS/Component/TransformationComponent.h"
#include "ECS/Component/RenderableComponent.h"
#include "ECS/Component/CameraComponent.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "GlobalVar.h"
#include <glm/ext.hpp>

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

	if (m_cameraEntity)
	{
		CameraComponent *cameraComponent = m_entityManager.getComponent<CameraComponent>(m_cameraEntity);
		TransformationComponent *transformationComponent = m_entityManager.getComponent<TransformationComponent>(m_cameraEntity);

		glm::mat4 &viewMatrix = cameraComponent->m_viewMatrix;
		glm::mat4 &projectionMatrix = cameraComponent->m_projectionMatrix;

		++m_renderParams.m_frame;
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

		// generate light data
		{
			m_lightData.m_shadowData.clear();
			m_lightData.m_directionalLightData.clear();
			m_lightData.m_pointLightData.clear();
			m_lightData.m_spotLightData.clear();
			m_lightData.m_shadowJobs.clear();

			m_lightData.m_directionalLightData.push_back(
				{
					glm::vec4(glm::vec3(5.0f), 1.0f),
					glm::vec4(glm::normalize(glm::vec3(0.1f, 3.0f, -1.0f)), 1.0f),
					0,
					3
				}
			);

			glm::mat4 matrices[3];
			calculateCascadeViewProjectionMatrices(m_renderParams, glm::normalize(glm::vec3(0.1f, 3.0f, -1.0f)), 0.1f, 30.0f, 0.7f, 2048, 3, matrices);

			m_lightData.m_shadowData.push_back({ matrices[0], { 0.25f, 0.25f, 0.0f, 0.0f } });
			m_lightData.m_shadowData.push_back({ matrices[1], { 0.25f, 0.25f, 1.0f, 0.0f } });
			m_lightData.m_shadowData.push_back({ matrices[2], { 0.25f, 0.25f, 0.0f, 1.0f } });
			//m_lightData.m_shadowData.push_back({ matrices[3], { 0.25f, 0.25f, 1.0f, 1.0f } });

			m_lightData.m_shadowJobs.push_back({ matrices[0], 0, 0, 2048 });
			m_lightData.m_shadowJobs.push_back({ matrices[1], 2048, 0, 2048 });
			m_lightData.m_shadowJobs.push_back({ matrices[2], 0, 2048, 2048 });
			//m_lightData.m_shadowJobs.push_back({ matrices[3], 2048, 2048, 2048 });
		}

		m_renderer->update(m_renderParams, m_drawLists, m_lightData);
	}
}

void VEngine::RenderSystem::render()
{
	if (m_cameraEntity)
	{
		m_renderer->render();
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

	glm::mat4 projectionMatrix = glm::perspective(renderParams.m_fovy, 1600.0f / 900.0f, nearPlane, farPlane);
	glm::mat4 invViewProjection = glm::inverse(projectionMatrix * renderParams.m_viewMatrix);

	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < cascadeCount; i++) 
	{
		float splitDist = splits[i];

		glm::vec3 frustumCorners[8] = 
		{
			glm::vec3(-1.0f,  1.0f, -1.0f),
			glm::vec3(1.0f,  1.0f, -1.0f),
			glm::vec3(1.0f, -1.0f, -1.0f),
			glm::vec3(-1.0f, -1.0f, -1.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		// Project frustum corners into world space
		for (uint32_t i = 0; i < 8; i++) 
		{
			glm::vec4 invCorner = invViewProjection * glm::vec4(frustumCorners[i], 1.0f);
			frustumCorners[i] = invCorner / invCorner.w;
		}

		for (uint32_t i = 0; i < 4; i++) 
		{
			glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
			frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
			frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t i = 0; i < 8; i++) 
		{
			frustumCenter += frustumCorners[i];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t i = 0; i < 8; i++) 
		{
			float distance = glm::length(frustumCorners[i] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter + lightDir * 150.0f, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, 300.0f);
		glm::mat4 vulkanCorrection =
		{
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.5f, 0.0f },
			{ 0.0f, 0.0f, 0.5f, 1.0f }
		};
		viewProjectionMatrices[i] = vulkanCorrection * lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = splits[i];
	}
}
