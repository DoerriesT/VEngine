#pragma once
#include "Component.h"

namespace VEngine
{
	struct RenderableComponent : public Component<RenderableComponent>
	{
		static const std::uint64_t FAMILY_ID;
	};
}