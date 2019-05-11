#include "VKMaterialManager.h"
#include "Graphics/RendererConsts.h"
#include "Graphics/Mesh.h"
#include "Utility/Utility.h"
#include "VKContext.h"
#include "VKUtility.h"
#include "Graphics/RenderData.h"
#include <glm/packing.hpp>

VEngine::VKMaterialManager::VKMaterialManager(VKBuffer &stagingBuffer, VKBuffer &materialBuffer)
	:m_stagingBuffer(stagingBuffer),
	m_materialBuffer(materialBuffer),
	m_freeHandles(new MaterialHandle[RendererConsts::MAX_MATERIALS]),
	m_freeHandleCount(RendererConsts::MAX_MATERIALS)
{
	for (MaterialHandle i = 0; i < RendererConsts::MAX_MATERIALS; ++i)
	{
		m_freeHandles[i] = RendererConsts::MAX_MATERIALS - i - 1;
	}
}

VEngine::VKMaterialManager::~VKMaterialManager()
{
	delete[] m_freeHandles;
	m_freeHandles = nullptr;
}

void VEngine::VKMaterialManager::createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		if (!m_freeHandleCount)
		{
			Utility::fatalExit("Out of Material Handles!", EXIT_FAILURE);
		}

		--m_freeHandleCount;
		handles[i] = m_freeHandles[m_freeHandleCount];
	}

	updateMaterials(count, materials, handles);
}

void VEngine::VKMaterialManager::updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	// fill staging buffer
	{
		auto packTextures = [](uint32_t tex0, uint32_t tex1) -> uint32_t
		{
			return ((tex0 & 0xFFFF) << 16) | (tex1 & 0xFFFF);
		};

		auto rgbmEncode = [](glm::vec3 color) -> glm::vec4
		{
			glm::vec4 rgbm;
			color *= 1.0 / 6.0;
			float a = glm::clamp(glm::max(glm::max(color.r, color.g), glm::max(color.b, 1e-6f)), 0.0f, 1.0f);
			a = ceil(rgbm.a * 255.0f) / 255.0f;
			glm::vec3 rgb = color / rgbm.a;
			return glm::vec4(rgb, a);
		};

		// decode
		//float3 RGBMDecode(float4 rgbm) {
		//	return 6.0 * rgbm.rgb * rgbm.a;
		//}

		MaterialData *data;
		g_context.m_allocator.mapMemory(m_stagingBuffer.getAllocation(), (void **)&data);
		{
			for (uint32_t i = 0; i < count; ++i)
			{
				auto &dstData = data[i];
				auto &srcData = materials[i];

				dstData.m_albedoOpacity = glm::packUnorm4x8(glm::vec4(srcData.m_albedoFactor, 0.5f));
				dstData.m_emissive = glm::packUnorm4x8(rgbmEncode(srcData.m_emissiveFactor));
				dstData.m_metalnessRoughness = glm::packUnorm4x8(glm::vec4(srcData.m_metallicFactor, srcData.m_roughnessFactor, 0.0f, 0.0f));
				dstData.m_albedoNormalTexture = packTextures(srcData.m_albedoTexture, srcData.m_normalTexture);
				dstData.m_metalnessRoughnessTexture = packTextures(srcData.m_metallicTexture, srcData.m_roughnessTexture);
				dstData.m_occlusionEmissiveTexture = packTextures(srcData.m_occlusionTexture, srcData.m_emissiveTexture);
				dstData.m_displacementTexture = packTextures(srcData.m_displacementTexture, 0);
			}
		}
		g_context.m_allocator.unmapMemory(m_stagingBuffer.getAllocation());
	}

	// copy to device local buffer
	{
		VkBufferCopy *bufferCopies = new VkBufferCopy[count];

		for (uint32_t i = 0; i < count; ++i)
		{
			auto &copy = bufferCopies[i];
			copy.srcOffset = i * sizeof(MaterialData);
			copy.dstOffset = handles[i] * sizeof(MaterialData);
			copy.size = sizeof(MaterialData);
		}

		VkCommandBuffer copyCmd = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		{
			vkCmdCopyBuffer(copyCmd, m_stagingBuffer.getBuffer(), m_materialBuffer.getBuffer(), count, bufferCopies);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, copyCmd);

		delete[] bufferCopies;
	}
}

void VEngine::VKMaterialManager::destroyMaterials(uint32_t count, MaterialHandle *handles)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		assert(m_freeHandleCount < RendererConsts::MAX_MATERIALS);
		m_freeHandles[m_freeHandleCount] = handles[i];
		++m_freeHandleCount;
	}
}
