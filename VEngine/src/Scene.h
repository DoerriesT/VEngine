#pragma once
#include <map>
#include <string>
#include "Graphics/Mesh.h"
#include <entt/entity/registry.hpp>
#include "Graphics/RenderData.h"
#include "Handles.h"

namespace VEngine
{
	class RenderSystem;

	struct Scene
	{
		std::vector<std::pair<std::string, entt::entity>> m_entities;
		std::map<std::string, std::vector<std::pair<SubMeshHandle, MaterialHandle>>> m_meshInstances;
		std::map<std::string, Texture2DHandle> m_textures;
		std::map<std::string, std::vector<MaterialHandle>> m_materials;
		std::map<std::string, std::vector<SubMeshHandle>> m_meshes;

		void load(RenderSystem &renderSystem, std::string filepath);
		void unload(RenderSystem &renderSystem);
	};
}