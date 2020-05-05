#pragma once
#include <vector>
#include <glm\vec3.hpp>
#include <glm\vec2.hpp>
#include <string>

struct Material
{
	enum class Alpha
	{
		OPAQUE, MASKED, BLENDED
	};

	std::string m_name;
	Alpha m_alpha;
	glm::vec3 m_albedoFactor;
	float m_metalnessFactor;
	float m_roughnessFactor;
	glm::vec3 m_emissiveFactor;
	float m_opacity;
	std::string m_albedoTexture;
	std::string m_normalTexture;
	std::string m_metalnessTexture;
	std::string m_roughnessTexture;
	std::string m_occlusionTexture;
	std::string m_emissiveTexture;
	std::string m_displacementTexture;
};

struct Mesh
{
	std::string m_name;
	size_t m_materialIndex;
	glm::vec3 m_aabbMin;
	glm::vec3 m_aabbMax;
	std::vector<uint16_t> m_indices;
	std::vector<glm::vec3> m_positions;
	std::vector<glm::vec3> m_normals;
	std::vector<glm::vec2> m_texCoords;
};

struct Model
{
	glm::vec3 m_aabbMin = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 m_aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
	std::vector<Mesh> m_meshes;
	std::vector<Material> m_materials;
};

class IModelLoader
{
public:
	virtual ~IModelLoader() = default;
	virtual Model loadModel(const std::string &filepath, bool mergeByMaterial, bool invertTexcoordY) const = 0;
};