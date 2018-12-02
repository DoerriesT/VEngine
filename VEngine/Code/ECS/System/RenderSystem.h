#pragma once
#include "System.h"
#include <memory>
#include "Graphics/RenderParams.h"

namespace VEngine
{
	class EntityManager;
	struct Entity;
	class VKRenderer;

	class RenderSystem : public System<RenderSystem>
	{
	public:
		explicit RenderSystem(EntityManager &entityManager);
		void init() override;
		void input(double time, double timeDelta) override;
		void update(double time, double timeDelta) override;
		void render() override;
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);
		void setCameraEntity(const Entity *cameraEntity);
		const Entity *getCameraEntity() const;

	private:
		EntityManager &m_entityManager;
		std::unique_ptr<VKRenderer> m_renderer;
		const Entity *m_cameraEntity;
		RenderParams m_renderParams;
	};
}