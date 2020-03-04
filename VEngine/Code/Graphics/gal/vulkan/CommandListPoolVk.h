#pragma once
#include "Graphics/gal/CommandListPool.h"
#include "Graphics/Vulkan/volk.h"
#include "Utility/ObjectPool.h"
#include "CommandListVk.h"

namespace VEngine
{
	namespace gal
	{
		class GraphicsDeviceVk;
		class QueueVk;

		class CommandListPoolVk : public CommandListPool
		{
		public:
			explicit CommandListPoolVk(GraphicsDeviceVk &device, const QueueVk &queue);
			CommandListPoolVk(CommandListPoolVk &) = delete;
			CommandListPoolVk(CommandListPoolVk &&) = delete;
			CommandListPoolVk &operator= (const CommandListPoolVk &) = delete;
			CommandListPoolVk &operator= (const CommandListPoolVk &&) = delete;
			~CommandListPoolVk();
			void allocate(uint32_t count, CommandList **commandLists) override;
			void free(uint32_t count, CommandList *commandLists) override;
			void reset() override;
		private:
			GraphicsDeviceVk *m_device;
			VkCommandPool m_commandPool;
			DynamicObjectPool<ByteArray<sizeof(CommandListVk)>> m_commandListMemoryPool;
		};
	}
}