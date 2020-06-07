#pragma once
#include "Handles.h"
#include "gal/FwdDecl.h"

namespace VEngine
{
	struct Material;

	class MaterialManager
	{
	public:
		explicit MaterialManager(gal::GraphicsDevice *graphicsDevice, gal::Buffer *stagingBuffer, gal::Buffer *materialBuffer);
		~MaterialManager();
		void createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void destroyMaterials(uint32_t count, MaterialHandle *handles);

	private:
		gal::GraphicsDevice *m_graphicsDevice;
		gal::CommandListPool *m_cmdListPool;
		gal::CommandList *m_cmdList;
		gal::Buffer *m_stagingBuffer;
		gal::Buffer *m_materialBuffer;
		uint32_t *m_freeHandles;
		uint32_t m_freeHandleCount;
	};
}