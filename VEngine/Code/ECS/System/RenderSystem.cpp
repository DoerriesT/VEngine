#include "RenderSystem.h"
#include "ECS/EntityManager.h"
#include "ECS/Component/MeshComponent.h"
#include "ECS/Component/TransformationComponent.h"
#include "ECS/Component/RenderableComponent.h"
#include "ECS/Component/CameraComponent.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "GlobalVar.h"
#include <glm/ext.hpp>
#include "Graphics/DrawItem.h"

namespace VEngine
{
	extern GlobalVar<unsigned int> g_windowWidth;
	extern GlobalVar<unsigned int> g_windowHeight;
}

VEngine::RenderSystem::RenderSystem(EntityManager &entityManager)
	:m_entityManager(entityManager),
	m_cameraEntity(),
	m_renderParams()
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
	DrawLists drawLists = {};

	// update all transformations
	m_entityManager.each<MeshComponent, TransformationComponent, RenderableComponent>(
		[&drawLists](const Entity *entity, MeshComponent &meshComponent, TransformationComponent &transformationComponent, RenderableComponent&)
	{
		transformationComponent.m_prevTransformation = transformationComponent.m_transformation;
		transformationComponent.m_transformation = glm::translate(transformationComponent.m_position)
			* glm::mat4_cast(transformationComponent.m_rotation)
			* glm::scale(glm::vec3(transformationComponent.m_scale));

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
				drawLists.m_opaqueItems.push_back(drawItem);
				break;
			case Material::Alpha::MASKED:
				drawLists.m_maskedItems.push_back(drawItem);
				break;
			case Material::Alpha::BLENDED:
				drawLists.m_blendedItems.push_back(drawItem);
				break;
			default:
				assert(false);
			}
		}
	});

	for (auto &item : drawLists.m_opaqueItems)
	{
		drawLists.m_allItems.push_back(item);
	}
	for (auto &item : drawLists.m_maskedItems)
	{
		drawLists.m_allItems.push_back(item);
	}
	for (auto &item : drawLists.m_blendedItems)
	{
		drawLists.m_allItems.push_back(item);
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
		m_renderParams.m_invViewMatrix = glm::transpose(viewMatrix);
		m_renderParams.m_invProjectionMatrix = glm::inverse(projectionMatrix);
		m_renderParams.m_invViewProjectionMatrix = glm::inverse(m_renderParams.m_viewProjectionMatrix);
		m_renderParams.m_cameraPosition = glm::vec4(transformationComponent->m_position, 1.0f);
		m_renderParams.m_cameraDirection = -glm::vec4(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2], 1.0f);

		m_renderer->update(m_renderParams, drawLists);
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
