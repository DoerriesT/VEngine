#pragma once
#include "Vulkan/VKBufferData.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace VEngine
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texCoord;
	};

	class Model
	{
	public:
		explicit Model(const char *filepath);
		VKBufferData getIndexBufferData() const;
		VKBufferData getVertexBufferData() const;
		uint32_t getIndexCount() const;

	private:
		VKBufferData m_indexBuffer;
		VKBufferData m_vertexBuffer;
		uint32_t indexCount;
	};
}