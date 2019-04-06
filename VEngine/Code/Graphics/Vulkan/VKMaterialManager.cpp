#include "VKMaterialManager.h"
#include "Graphics/RendererConsts.h"
#include "Graphics/Mesh.h"
#include "Utility/Utility.h"
#include "VKContext.h"
#include "VKUtility.h"
#include "Graphics/RenderData.h"

VEngine::VKMaterialManager::VKMaterialManager(VKBuffer &stagingBuffer, VKBuffer &materialBuffer)
	:m_stagingBuffer(stagingBuffer),
	m_materialBuffer(materialBuffer),
	m_freeHandles(new MaterialHandle[RendererConsts::MAX_MATERIALS]),
	m_freeHandleCount(RendererConsts::MAX_MATERIALS)
{
	for (MaterialHandle i = 0; i < RendererConsts::MAX_MATERIALS; ++i)
	{
		m_freeHandles[i] = RendererConsts::MAX_MATERIALS - i;
	}
}

VEngine::VKMaterialManager::~VKMaterialManager()
{
	delete[] m_freeHandles;
	m_freeHandles = nullptr;
}

void VEngine::VKMaterialManager::createMaterials(size_t count, const Material *materials, MaterialHandle *handles)
{
	for (size_t i = 0; i < count; ++i)
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

void VEngine::VKMaterialManager::updateMaterials(size_t count, const Material *materials, MaterialHandle *handles)
{
	// fill staging buffer
	{
		auto packTextures = [](uint32_t tex0, uint32_t tex1) -> uint32_t
		{
			return ((tex0 & 0xFFFF) << 16) | (tex1 & 0xFFFF);
		};

		MaterialData *data;
		g_context.m_allocator.mapMemory(m_stagingBuffer.getAllocation(), (void **)&data);
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &dstData = data[i];
				auto &srcData = materials[i];

				dstData.m_albedoFactor[0] = srcData.m_albedoFactor[0];
				dstData.m_albedoFactor[1] = srcData.m_albedoFactor[1];
				dstData.m_albedoFactor[2] = srcData.m_albedoFactor[2];
				dstData.m_metalnessFactor = srcData.m_metallicFactor;
				dstData.m_emissiveFactor[0] = srcData.m_emissiveFactor[0];
				dstData.m_emissiveFactor[1] = srcData.m_emissiveFactor[1];
				dstData.m_emissiveFactor[2] = srcData.m_emissiveFactor[2];
				dstData.m_roughnessFactor = srcData.m_roughnessFactor;
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

		for (size_t i = 0; i < count; ++i)
		{
			auto &copy = bufferCopies[i];
			copy.srcOffset = i * sizeof(MaterialData);
			copy.dstOffset = (handles[i] - 1) * sizeof(MaterialData);
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

void VEngine::VKMaterialManager::destroyMaterials(size_t count, MaterialHandle *handles)
{
	for (size_t i = 0; i < count; ++i)
	{
		m_freeHandles[m_freeHandleCount] = handles[i];
		++m_freeHandleCount;
		assert(m_freeHandleCount < RendererConsts::MAX_MATERIALS);
	}
}
