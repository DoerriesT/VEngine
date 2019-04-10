#pragma once
#include "Handles.h"
#include "VKBuffer.h"

namespace VEngine
{
	struct Material;

	class VKMaterialManager
	{
	public:
		explicit VKMaterialManager(VKBuffer &stagingBuffer, VKBuffer &materialBuffer);
		~VKMaterialManager();
		void createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void destroyMaterials(uint32_t count, MaterialHandle *handles);

	private:
		VKBuffer &m_stagingBuffer;
		VKBuffer &m_materialBuffer;
		MaterialHandle *m_freeHandles;
		uint32_t m_freeHandleCount;
	};
}