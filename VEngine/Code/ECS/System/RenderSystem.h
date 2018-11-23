#pragma once
#include "System.h"
#include <memory>
#include "Scene.h"

namespace VEngine
{
	class EntityManager;
	class VKRenderer;

	class RenderSystem
		: public System<RenderSystem>
	{
	public:
		explicit RenderSystem(EntityManager &entityManager);
		~RenderSystem();
		void init() override;
		void input(double currentTime, double timeDelta) override;
		void update(double currentTime, double timeDelta) override;
		void render() override;
		void onComponentAdded(const Entity *entity, BaseComponent *addedComponent) override;
		void onComponentRemoved(const Entity *entity, BaseComponent *removedComponent) override;
		void onDestruction(const Entity *entity) override;

	private:
		EntityManager &m_entityManager;
		std::unique_ptr<VKRenderer> m_renderer;
		Scene m_scene;
	};
}