#pragma once
#include "volk.h"
#include <vector>

namespace VEngine
{
	class CommandBufferPool
	{
	public:
		explicit CommandBufferPool(uint32_t allocationChunkSize = 64);
		CommandBufferPool(const CommandBufferPool &) = delete;
		CommandBufferPool(const CommandBufferPool &&) = delete;
		CommandBufferPool &operator= (const CommandBufferPool &) = delete;
		CommandBufferPool &operator= (const CommandBufferPool &&) = delete;
		~CommandBufferPool();
		VkCommandBuffer acquire(uint32_t queueFamilyIndex);
		void reset();

	private:
		const uint32_t m_allocationChunkSize;
		VkCommandPool m_commandPools[3] = {};
		std::vector<VkCommandBuffer> m_commandBuffers[3] = {};
		uint32_t m_nextFreeCmdBuf[3] = {};
	};
}