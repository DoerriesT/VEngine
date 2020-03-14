#pragma once
#include <glm\vec3.hpp>
#include <glm\mat4x4.hpp>
#include <glm\gtc\quaternion.hpp>

namespace VEngine
{
	struct TransformationComponent;
	struct CameraComponent;

	class Camera
	{
	public:
		explicit Camera(TransformationComponent &transformationComponent, CameraComponent &cameraComponent);
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
		TransformationComponent & m_transformationComponent;
		CameraComponent & m_cameraComponent;

		void updateViewMatrix();
		void updateProjectionMatrix();
	};
}