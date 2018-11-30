#pragma once

namespace VEngine
{
	struct Entity;
	struct IComponent;

	class IOnComponentAddedListener
	{
	public:
		virtual void onComponentAdded(const Entity *entity, IComponent *addedComponent) = 0;
	};
}
