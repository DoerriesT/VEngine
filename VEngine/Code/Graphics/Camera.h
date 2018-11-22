#pragma once
#include <glm\vec3.hpp>
#include <glm\mat4x4.hpp>
#include <glm\gtc\quaternion.hpp>

namespace VEngine
{
	class Camera
	{
	public:
		explicit Camera(const glm::vec3 &position, const glm::quat &rotation, float aspectRatio, float fovy, float near, float far);
		explicit Camera(const glm::vec3 &position, const glm::vec3 &pitchYawRoll, float aspectRatio, float fovy, float near, float far);
		void setRotation(const glm::quat &rotation);
		void setPosition(const glm::vec3 &position);
		void setAspectRatio(float aspectRatio);
		void setFovy(float fovy);
		void rotate(const glm::vec3 &pitchYawRollOffset);
		void translate(const glm::vec3 &translationOffset);
		void lookAt(const glm::vec3 &targetPosition);
		glm::mat4 getViewMatrix() const;
		glm::mat4 getProjectionMatrix() const;
		glm::vec3 getPosition() const;
		glm::quat getRotation() const;
		glm::vec3 getForwardDirection() const;
		glm::vec3 getUpDirection() const;
		float getAspectRatio() const;
		float getFovy() const;

	private:
		glm::vec3 m_position;
		glm::quat m_rotation;
		float m_aspectRatio;
		float m_fovy;
		float m_near;
		float m_far;

		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;

		void updateViewMatrix();
		void updateProjectionMatrix();
	};
}