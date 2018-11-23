#pragma once

namespace VEngine
{
	struct Entity;

	class IOnEntityCreatedListener
	{
	public:
		virtual void onEntityCreated(const Entity *_entity) = 0;
	};
}