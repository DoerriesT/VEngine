#pragma once

namespace VEngine
{
	struct Entity;
	struct IComponent;

	class IOnComponentRemovedListener
	{
	public:
		virtual void onComponentRemoved(const Entity *entity, IComponent *removedComponent) = 0;
	};
}