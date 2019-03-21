#pragma once
#include "System.h"
#include <memory>
#include "Graphics/RenderParams.h"
#include "Graphics/DrawItem.h"
#include "Graphics/LightData.h"

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
		uint32_t createTexture(const char *filepath);
		void updateTextureData();
		void setCameraEntity(const Entity *cameraEntity);
		const Entity *getCameraEntity() const;

	private:
		EntityManager &m_entityManager;
		std::unique_ptr<VKRenderer> m_renderer;
		const Entity *m_cameraEntity;
		RenderParams m_renderParams;
		DrawLists m_drawLists;
		LightData m_lightData;
		float m_haltonX[16];
		float m_haltonY[16];

		void calculateCascadeViewProjectionMatrices(const RenderParams &renderParams, const glm::vec3 &lightDir, float nearPlane, float farPlane, float splitLambda, float shadowTextureSize, size_t cascadeCount, glm::mat4 *viewProjectionMatrices);
	};
}