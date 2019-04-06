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
		void createMaterials(size_t count, const Material *materials, MaterialHandle *handles);
		void updateMaterials(size_t count, const Material *materials, MaterialHandle *handles);
		void destroyMaterials(size_t count, MaterialHandle *handles);

	private:
		VKBuffer &m_stagingBuffer;
		VKBuffer &m_materialBuffer;
		MaterialHandle *m_freeHandles;
		size_t m_freeHandleCount;
	};
}