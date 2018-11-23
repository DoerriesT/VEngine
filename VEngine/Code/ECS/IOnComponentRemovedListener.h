#pragma once

namespace VEngine
{
	struct Entity;
	class BaseComponent;

	class IOnComponentRemovedListener
	{
	public:
		virtual void onComponentRemoved(const Entity *_entity, BaseComponent *_removedComponent) = 0;
	};
}