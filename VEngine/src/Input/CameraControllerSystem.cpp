#include "CameraControllerSystem.h"
#include "Input/UserInput.h"
#include "Components/TransformationComponent.h"
#include "Components/CameraComponent.h"
#include "Graphics/Camera/Camera.h"

VEngine::CameraControllerSystem::CameraControllerSystem(entt::registry &entityRegistry, UserInput & userInput, const std::function<void(bool)>& grabMouse)
	:m_entityRegistry(entityRegistry),
	m_userInput(userInput),
	m_grabMouse(grabMouse),
	m_grabbedMouse(),
	m_mouseHistory(),
	m_mouseSmoothFactor(0.05f)
{
}

void VEngine::CameraControllerSystem::update(float timeDelta, entt::entity cameraEntity)
{
	auto view = m_entityRegistry.view<TransformationComponent, CameraComponent>();

	view.each([&](entt::entity entity, TransformationComponent &transformationComponent, CameraComponent &cameraComponent)
	{
		// creating a Camera object automatically updates the view/projection matrices of the CameraComponent
		Camera camera(transformationComponent, cameraComponent);

		if (entity == cameraEntity)
		{
			switch (cameraComponent.m_controllerType)
			{
			case CameraComponent::ControllerType::FPS:
			{
				bool pressed = false;
				float mod = 1.0f;
				glm::vec3 cameraTranslation;

				glm::vec2 mouseDelta = {};

				if (m_userInput.isMouseButtonPressed(InputMouse::BUTTON_RIGHT))
				{
					if (!m_grabbedMouse)
					{
						m_grabbedMouse = true;
						m_grabMouse(m_grabbedMouse);
					}
					mouseDelta = m_userInput.getMousePosDelta();
				}
				else
				{
					if (m_grabbedMouse)
					{
						m_grabbedMouse = false;
						m_grabMouse(m_grabbedMouse);
					}
				}

				m_mouseHistory = glm::mix(m_mouseHistory, mouseDelta, timeDelta / (timeDelta + m_mouseSmoothFactor));
				if (glm::dot(m_mouseHistory, m_mouseHistory) > 0.0f)
				{
					camera.rotate(glm::vec3(m_mouseHistory.y * 0.005f, m_mouseHistory.x * 0.005f, 0.0));
				}


				if (m_userInput.isKeyPressed(InputKey::UP))
				{
					camera.rotate(glm::vec3(-timeDelta, 0.0f, 0.0));
				}
				if (m_userInput.isKeyPressed(InputKey::DOWN))
				{
					camera.rotate(glm::vec3(timeDelta, 0.0f, 0.0));
				}
				if (m_userInput.isKeyPressed(InputKey::LEFT))
				{
					camera.rotate(glm::vec3(0.0f, -timeDelta, 0.0));
				}
				if (m_userInput.isKeyPressed(InputKey::RIGHT))
				{
					camera.rotate(glm::vec3(0.0f, timeDelta, 0.0));
				}

				if (m_userInput.isKeyPressed(InputKey::LEFT_SHIFT))
				{
					mod = 5.0f;
				}
				if (m_userInput.isKeyPressed(InputKey::W))
				{
					cameraTranslation.z = -mod * (float)timeDelta;
					pressed = true;
				}
				if (m_userInput.isKeyPressed(InputKey::S))
				{
					cameraTranslation.z = mod * (float)timeDelta;
					pressed = true;
				}
				if (m_userInput.isKeyPressed(InputKey::A))
				{
					cameraTranslation.x = -mod * (float)timeDelta;
					pressed = true;
				}
				if (m_userInput.isKeyPressed(InputKey::D))
				{
					cameraTranslation.x = mod * (float)timeDelta;
					pressed = true;
				}
				if (pressed)
				{
					camera.translate(cameraTranslation);
				}
			}
			break;
			default:
				break;
			}
		}
	});
}
