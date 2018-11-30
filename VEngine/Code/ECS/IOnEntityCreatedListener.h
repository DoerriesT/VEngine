#pragma once

namespace VEngine
{
	struct Entity;

	class IOnEntityCreatedListener
	{
	public:
		virtual void onEntityCreated(const Entity *entity) = 0;
	};
}