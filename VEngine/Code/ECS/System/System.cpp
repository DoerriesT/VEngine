#include "System.h"

std::uint64_t VEngine::BaseSystem::m_typeCount = 0;

bool VEngine::BaseSystem::validate(const std::bitset<COMPONENT_TYPE_COUNT> &componentBitSet)
{
	for (auto &configuration : m_validBitSets)
	{
		if ((configuration & componentBitSet) == configuration)
		{
			return true;
		}
	}
	return false;
}
