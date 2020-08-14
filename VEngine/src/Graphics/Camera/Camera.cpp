#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Components/TransformationComponent.h"
#include "Components/CameraComponent.h"


VEngine::Camera::Camera(TransformationComponent & transformationComponent, CameraComponent & cameraComponent)
	:m_transformationComponent(transformationComponent),
	m_cameraComponent(cameraComponent)
{
	updateViewMatrix();
	updateProjectionMatrix();
}

void VEngine::Camera::setRotation(const glm::quat &rotation)
{
	m_transformationComponent.m_orientation = rotation;
	updateViewMatrix();
}

void VEngine::Camera::setPosition(const glm::vec3 &position)
{
	m_transformationComponent.m_position = position;
	updateViewMatrix();
}

void VEngine::Camera::setAspectRatio(float aspectRatio)
{
	m_cameraComponent.m_aspectRatio = aspectRatio;
	updateProjectionMatrix();
}

void VEngine::Camera::setFovy(float fovy)
{
	m_cameraComponent.m_fovy = fovy;
	updateProjectionMatrix();
}

void VEngine::Camera::rotate(const glm::vec3 &pitchYawRollOffset)
{
	glm::quat tmp = glm::quat(glm::vec3(-pitchYawRollOffset.x, 0.0, 0.0));
	glm::quat tmp1 = glm::quat(glm::angleAxis(-pitchYawRollOffset.y, glm::vec3(0.0, 1.0, 0.0)));
	m_transformationComponent.m_orientation = glm::normalize(tmp1 * m_transformationComponent.m_orientation * tmp);

	updateViewMatrix();
}

void VEngine::Camera::translate(const glm::vec3 &translationOffset)
{
	glm::vec3 forward(m_cameraComponent.m_viewMatrix[0][2], m_cameraComponent.m_viewMatrix[1][2], m_cameraComponent.m_viewMatrix[2][2]);
	glm::vec3 strafe(m_cameraComponent.m_viewMatrix[0][0], m_cameraComponent.m_viewMatrix[1][0], m_cameraComponent.m_viewMatrix[2][0]);

	m_transformationComponent.m_position += translationOffset.z * forward + translationOffset.x * strafe;
	updateViewMatrix();
}

void VEngine::Camera::lookAt(const glm::vec3 &targetPosition)
{
	const glm::vec3 up(0.0, 1.0f, 0.0);

	m_cameraComponent.m_viewMatrix = glm::lookAt(m_transformationComponent.m_position, targetPosition, up);
	m_transformationComponent.m_orientation = glm::quat_cast(m_cameraComponent.m_viewMatrix);
}

glm::mat4 VEngine::Camera::getViewMatrix() const
{
	return m_cameraComponent.m_viewMatrix;
}

glm::mat4 VEngine::Camera::getProjectionMatrix() const
{
	return m_cameraComponent.m_projectionMatrix;
}

glm::vec3 VEngine::Camera::getPosition() const
{
	return m_transformationComponent.m_position;
}

glm::quat VEngine::Camera::getRotation() const
{
	return m_transformationComponent.m_orientation;
}

glm::vec3 VEngine::Camera::getForwardDirection() const
{
	return -glm::vec3(m_cameraComponent.m_viewMatrix[0][2], m_cameraComponent.m_viewMatrix[1][2], m_cameraComponent.m_viewMatrix[2][2]);
}

glm::vec3 VEngine::Camera::getUpDirection() const
{
	return glm::vec3(m_cameraComponent.m_viewMatrix[0][1], m_cameraComponent.m_viewMatrix[1][1], m_cameraComponent.m_viewMatrix[2][1]);
}

float VEngine::Camera::getAspectRatio() const
{
	return m_cameraComponent.m_aspectRatio;
}

float VEngine::Camera::getFovy() const
{
	return m_cameraComponent.m_fovy;
}

void VEngine::Camera::updateViewMatrix()
{
	glm::mat4 translationMatrix;
	m_cameraComponent.m_viewMatrix = glm::mat4_cast(glm::inverse(m_transformationComponent.m_orientation)) * glm::translate(translationMatrix, -m_transformationComponent.m_position);
}

void VEngine::Camera::updateProjectionMatrix()
{
	m_cameraComponent.m_projectionMatrix = glm::perspective(m_cameraComponent.m_fovy, m_cameraComponent.m_aspectRatio, m_cameraComponent.m_far, m_cameraComponent.m_near);
}
