#pragma once
#include <functional>
#include <glm/vec2.hpp>
#include <entt/entity/registry.hpp>

namespace VEngine
{
	class EntityManager;
	class UserInput;

	class CameraControllerSystem
	{
	public:
		explicit CameraControllerSystem(entt::registry &entityRegistry, UserInput &userInput, const std::function<void(bool)> &grabMouse);
		void update(float timeDelta, entt::entity cameraEntity);

	private:
		entt::registry &m_entityRegistry;
		UserInput &m_userInput;
		std::function<void(bool)> m_grabMouse;
		bool m_grabbedMouse;
		glm::vec2 m_mouseHistory;
		float m_mouseSmoothFactor;
	};
}