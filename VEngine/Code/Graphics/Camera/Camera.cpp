#include "Camera.h"
#include <glm\gtc\matrix_transform.hpp>

VEngine::Camera::Camera(const glm::vec3 &position, const glm::quat &rotation, float aspectRatio, float fovy, float near, float far)
	:m_position(position),
	m_rotation(rotation),
	m_aspectRatio(aspectRatio),
	m_fovy(fovy),
	m_near(near),
	m_far(far)
{
	updateViewMatrix();
	updateProjectionMatrix();
}

void VEngine::Camera::setRotation(const glm::quat &rotation)
{
	m_rotation = rotation;
	updateViewMatrix();
}

void VEngine::Camera::setPosition(const glm::vec3 &position)
{
	m_position = position;
	updateViewMatrix();
}

void VEngine::Camera::setAspectRatio(float aspectRatio)
{
	m_aspectRatio = aspectRatio;
	updateProjectionMatrix();
}

void VEngine::Camera::setFovy(float fovy)
{
	m_fovy = fovy;
	updateProjectionMatrix();
}

void VEngine::Camera::rotate(const glm::vec3 &pitchYawRollOffset)
{
	glm::quat tmp = glm::quat(glm::vec3(pitchYawRollOffset.x, 0.0, 0.0));
	glm::quat tmp1 = glm::quat(glm::angleAxis(pitchYawRollOffset.y, glm::vec3(0.0, 1.0, 0.0)));
	m_rotation = glm::normalize(tmp * m_rotation * tmp1);

	updateViewMatrix();
}

void VEngine::Camera::translate(const glm::vec3 &translationOffset)
{
	glm::vec3 forward(m_viewMatrix[0][2], m_viewMatrix[1][2], m_viewMatrix[2][2]);
	glm::vec3 strafe(m_viewMatrix[0][0], m_viewMatrix[1][0], m_viewMatrix[2][0]);

	m_position += translationOffset.z * forward + translationOffset.x * strafe;
	updateViewMatrix();
}

void VEngine::Camera::lookAt(const glm::vec3 &targetPosition)
{
	const glm::vec3 up(0.0, 1.0f, 0.0);

	m_viewMatrix = glm::lookAt(m_position, targetPosition, up);
	m_rotation = glm::quat_cast(m_viewMatrix);
}

glm::mat4 VEngine::Camera::getViewMatrix() const
{
	return m_viewMatrix;
}

glm::mat4 VEngine::Camera::getProjectionMatrix() const
{
	return m_projectionMatrix;
}

glm::vec3 VEngine::Camera::getPosition() const
{
	return m_position;
}

glm::quat VEngine::Camera::getRotation() const
{
	return m_rotation;
}

glm::vec3 VEngine::Camera::getForwardDirection() const
{
	return -glm::transpose(m_viewMatrix)[2];
}

glm::vec3 VEngine::Camera::getUpDirection() const
{
	return glm::transpose(m_viewMatrix)[1];
}

float VEngine::Camera::getAspectRatio() const
{
	return m_aspectRatio;
}

float VEngine::Camera::getFovy() const
{
	return m_fovy;
}

void VEngine::Camera::updateViewMatrix()
{
	glm::mat4 translationMatrix;
	m_viewMatrix = glm::mat4_cast(m_rotation) * glm::translate(translationMatrix, -m_position);
}

void VEngine::Camera::updateProjectionMatrix()
{
	m_projectionMatrix = glm::perspective(m_fovy, m_aspectRatio, m_near, m_far);
	m_projectionMatrix[1][1] *= -1;
}
