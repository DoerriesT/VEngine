#pragma once
#include "Component.h"
#include "Graphics/Model.h"
#include <memory>

namespace VEngine
{
	struct ModelComponent : public Component<ModelComponent>
	{
		static const std::uint64_t FAMILY_ID;
		std::shared_ptr<Model> m_model;

		explicit ModelComponent(std::shared_ptr<Model> model)
			:m_model(model)
		{
		}
	};
}