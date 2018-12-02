#pragma once
#include "Component.h"
#include "Graphics/Mesh.h"

namespace VEngine
{
	struct MeshComponent : public Component<MeshComponent>
	{
		Mesh m_mesh;

		explicit MeshComponent(const Mesh &mesh)
			:m_mesh(mesh)
		{
		}
	};
}