#pragma once
#include "Component.h"
#include "Graphics/Model.h"
#include <memory>

namespace VEngine
{
	struct ModelComponent : public Component<ModelComponent>
	{
		std::shared_ptr<Model> m_model;

		explicit ModelComponent(std::shared_ptr<Model> model)
			:m_model(model)
		{
		}
	};
}