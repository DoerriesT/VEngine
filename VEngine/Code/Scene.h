#pragma once
#include <map>
#include <string>
#include "Graphics/Mesh.h"
#include <entt/entity/registry.hpp>

namespace VEngine
{
	class RenderSystem;

	struct Scene
	{
		std::map<std::string, entt::entity> m_entities;
		std::map<std::string, Mesh> m_meshes;
		std::map<std::string, uint32_t> m_textures;

		void load(RenderSystem &renderSystem, std::string filepath);
		void unload(RenderSystem &renderSystem);
	};
}