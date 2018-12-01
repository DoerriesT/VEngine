#include "RenderSystem.h"
#include "ECS/EntityManager.h"
#include "ECS/Component/ModelComponent.h"
#include "ECS/Component/TransformationComponent.h"
#include "ECS/Component/RenderableComponent.h"
#include "ECS/Component/CameraComponent.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "GlobalVar.h"
#include <glm/ext.hpp>

namespace VEngine
{
	extern GlobalVar<unsigned int> g_windowWidth;
	extern GlobalVar<unsigned int> g_windowHeight;
}

VEngine::RenderSystem::RenderSystem(EntityManager &entityManager)
	:m_entityManager(entityManager),
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
	// update all transformations
	m_entityManager.each<ModelComponent, TransformationComponent, RenderableComponent>(
		[](const Entity *entity, ModelComponent &modelComponent, TransformationComponent &transformationComponent, RenderableComponent&)
	{
		transformationComponent.m_prevTransformation = transformationComponent.m_transformation;
		transformationComponent.m_transformation = glm::translate(transformationComponent.m_position)
			* glm::mat4_cast(transformationComponent.m_rotation)
			* glm::scale(glm::vec3(transformationComponent.m_scale));
	});

	if (m_cameraEntity)
	{
		CameraComponent *cameraComponent = m_entityManager.getComponent<CameraComponent>(m_cameraEntity);
		TransformationComponent *transformationComponent = m_entityManager.getComponent<TransformationComponent>(m_cameraEntity);

		glm::mat4 &viewMatrix = cameraComponent->m_viewMatrix;
		glm::mat4 &projectionMatrix = cameraComponent->m_projectionMatrix;

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
		m_renderParams.m_cameraPosition = transformationComponent->m_position;
		m_renderParams.m_cameraDirection = -glm::vec3(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2]);

		m_renderer->update(m_renderParams);
	}
}

void VEngine::RenderSystem::render()
{
	if (m_cameraEntity)
	{
		m_renderer->render();
	}
}

void VEngine::RenderSystem::reserveMeshBuffer(uint64_t size)
{
	m_renderer->reserveMeshBuffer(size);
}

void VEngine::RenderSystem::uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize)
{
	m_renderer->uploadMeshData(vertices, vertexSize, indices, indexSize);
}

void VEngine::RenderSystem::setCameraEntity(const Entity *cameraEntity)
{
	m_cameraEntity = cameraEntity;
}

const VEngine::Entity *VEngine::RenderSystem::getCameraEntity() const
{
	return m_cameraEntity;
}
