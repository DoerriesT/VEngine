#pragma once
#include "gal/FwdDecl.h"
#include <cstdint>
#include <vector>

namespace VEngine
{
	class CommandListFramePool
	{
	public:
		explicit CommandListFramePool(gal::GraphicsDevice *graphicsDevice, uint32_t allocationChunkSize = 64);
		CommandListFramePool(const CommandListFramePool &) = delete;
		CommandListFramePool(const CommandListFramePool &&) = delete;
		CommandListFramePool &operator= (const CommandListFramePool &) = delete;
		CommandListFramePool &operator= (const CommandListFramePool &&) = delete;
		~CommandListFramePool();
		gal::CommandList *acquire(gal::Queue *queue);
		void reset();

	private:
		gal::GraphicsDevice *m_graphicsDevice;
		const uint32_t m_allocationChunkSize;
		gal::CommandListPool *m_commandListPools[3] = {};
		std::vector<gal::CommandList *> m_commandLists[3] = {};
		uint32_t m_nextFreeCmdList[3] = {};
	};
}