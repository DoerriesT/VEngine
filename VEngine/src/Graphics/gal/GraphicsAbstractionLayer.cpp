#include "GraphicsAbstractionLayer.h"
#include "vulkan/GraphicsDeviceVk.h"
//#include "dx12/GraphicsDeviceDx12.h"

VEngine::gal::GraphicsDevice *VEngine::gal::GraphicsDevice::create(void *windowHandle, bool debugLayer, GraphicsBackendType backend)
{
	switch (backend)
	{
	case GraphicsBackendType::VULKAN:
		return new GraphicsDeviceVk(windowHandle, debugLayer);
	//case GraphicsBackendType::D3D12:
	//	return new GraphicsDeviceDx12(windowHandle, debugLayer);
	default:
		assert(false);
		break;
	}
	return nullptr;
}

void VEngine::gal::GraphicsDevice::destroy(const GraphicsDevice *device)
{
	delete device;
}