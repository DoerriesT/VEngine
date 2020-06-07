#include "MaterialManager.h"
#include "RendererConsts.h"
#include "Utility/Utility.h"
#include <glm/packing.hpp>
#include "Graphics/RenderData.h"
#include "Graphics/Mesh.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"

using namespace VEngine::gal;

VEngine::MaterialManager::MaterialManager(GraphicsDevice *graphicsDevice, Buffer *stagingBuffer, Buffer *materialBuffer)
	:m_graphicsDevice(graphicsDevice),
	m_cmdListPool(),
	m_cmdList(),
	m_stagingBuffer(stagingBuffer),
	m_materialBuffer(materialBuffer),
	m_freeHandles(new uint32_t[RendererConsts::MAX_MATERIALS]),
	m_freeHandleCount(RendererConsts::MAX_MATERIALS)
{
	m_graphicsDevice->createCommandListPool(m_graphicsDevice->getGraphicsQueue(), &m_cmdListPool);
	m_cmdListPool->allocate(1, &m_cmdList);

	for (uint32_t i = 0; i < RendererConsts::MAX_MATERIALS; ++i)
	{
		m_freeHandles[i] = RendererConsts::MAX_MATERIALS - i - 1;
	}
}

VEngine::MaterialManager::~MaterialManager()
{
	delete[] m_freeHandles;
	m_freeHandles = nullptr;

	m_cmdListPool->free(1, &m_cmdList);
	m_graphicsDevice->destroyCommandListPool(m_cmdListPool);
}

void VEngine::MaterialManager::createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		if (!m_freeHandleCount)
		{
			Utility::fatalExit("Out of Material Handles!", EXIT_FAILURE);
		}

		--m_freeHandleCount;
		handles[i] = { m_freeHandles[m_freeHandleCount] };
	}

	updateMaterials(count, materials, handles);
}

void VEngine::MaterialManager::updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
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
		m_stagingBuffer->map((void **)&data);
		{
			for (uint32_t i = 0; i < count; ++i)
			{
				auto &dstData = data[i];
				auto &srcData = materials[i];

				dstData.m_albedoOpacity = glm::packUnorm4x8(glm::vec4(srcData.m_albedoFactor, 0.5f));
				dstData.m_emissive = glm::packUnorm4x8(rgbmEncode(srcData.m_emissiveFactor));
				dstData.m_metalnessRoughness = glm::packUnorm4x8(glm::vec4(srcData.m_metallicFactor, srcData.m_roughnessFactor, 0.0f, 0.0f));
				dstData.m_albedoNormalTexture = packTextures(srcData.m_albedoTexture.m_handle, srcData.m_normalTexture.m_handle);
				dstData.m_metalnessRoughnessTexture = packTextures(srcData.m_metallicTexture.m_handle, srcData.m_roughnessTexture.m_handle);
				dstData.m_occlusionEmissiveTexture = packTextures(srcData.m_occlusionTexture.m_handle, srcData.m_emissiveTexture.m_handle);
				dstData.m_displacementTexture = packTextures(srcData.m_displacementTexture.m_handle, 0);
			}
		}
		m_stagingBuffer->unmap();
	}

	// copy to device local buffer
	{
		BufferCopy *bufferCopies = new BufferCopy[count];

		for (uint32_t i = 0; i < count; ++i)
		{
			auto &copy = bufferCopies[i];
			copy.m_srcOffset = i * sizeof(MaterialData);
			copy.m_dstOffset = handles[i].m_handle * sizeof(MaterialData);
			copy.m_size = sizeof(MaterialData);
		}

		m_cmdListPool->reset();
		m_cmdList->begin();
		{
			m_cmdList->copyBuffer(m_stagingBuffer, m_materialBuffer, count, bufferCopies);
		}
		m_cmdList->end();
		Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), m_cmdList);

		delete[] bufferCopies;
	}
}

void VEngine::MaterialManager::destroyMaterials(uint32_t count, MaterialHandle *handles)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		assert(m_freeHandleCount < RendererConsts::MAX_MATERIALS);
		m_freeHandles[m_freeHandleCount] = handles[i].m_handle;
		++m_freeHandleCount;
	}
}
