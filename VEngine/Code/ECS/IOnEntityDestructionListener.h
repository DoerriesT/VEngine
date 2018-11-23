#pragma once

namespace VEngine
{
	struct Entity;

	class IOnEntityDestructionListener
	{
	public:
		virtual void onDestruction(const Entity *_entity) = 0;
	};
}