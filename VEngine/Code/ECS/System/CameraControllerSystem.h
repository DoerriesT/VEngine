#pragma once
#include "System.h"
#include <functional>
#include <glm/vec2.hpp>

namespace VEngine
{
	class EntityManager;
	class UserInput;
	struct Entity;

	class CameraControllerSystem : public System<CameraControllerSystem>
	{
	public:
		explicit CameraControllerSystem(EntityManager &entityManager, UserInput &userInput, const std::function<void(bool)> &grabMouse);
		void init() override;
		void input(double time, double timeDelta) override;
		void update(double time, double timeDelta) override;
		void render() override;

	private:
		EntityManager &m_entityManager;
		UserInput &m_userInput;
		std::function<void(bool)> m_grabMouse;
		bool m_grabbedMouse;
		glm::vec2 m_mouseHistory;
		float m_mouseSmoothFactor;
	};
}