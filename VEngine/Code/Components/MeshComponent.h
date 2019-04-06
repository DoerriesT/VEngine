#pragma once
#include "Handles.h"
#include "Graphics/RenderData.h"
#include <vector>

namespace VEngine
{
	struct MeshComponent
	{
		std::vector<std::pair<SubMeshData, MaterialHandle>> m_subMeshMaterialPairs;
	};
}