#pragma once
#include <cstdint>

namespace VEngine
{
	namespace gal
	{
		class CommandList;


		class CommandListPool
		{
		public:
			virtual ~CommandListPool() = 0;
			virtual void allocate(uint32_t count, CommandList **commandLists) = 0;
			virtual void free(uint32_t count, CommandList *commandLists) = 0;
			virtual void reset() = 0;
		};
	}
}