#pragma once
#include "Handles.h"
#include <vector>

namespace VEngine
{
	struct MeshComponent
	{
		std::vector<std::pair<SubMeshHandle, MaterialHandle>> m_subMeshMaterialPairs;
	};
}