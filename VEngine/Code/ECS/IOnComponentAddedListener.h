#pragma once

namespace VEngine
{
	struct Entity;
	class BaseComponent;

	class IOnComponentAddedListener
	{
	public:
		virtual void onComponentAdded(const Entity *_entity, BaseComponent *_addedComponent) = 0;
	};
}
