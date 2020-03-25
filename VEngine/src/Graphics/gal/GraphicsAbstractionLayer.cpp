#include "GraphicsAbstractionLayer.h"
#include "vulkan/GraphicsDeviceVk.h"

VEngine::gal::GraphicsDevice *VEngine::gal::GraphicsDevice::create(void *windowHandle, bool debugLayer, GraphicsBackendType backend)
{
	switch (backend)
	{
	case GraphicsBackendType::VULKAN:
		return new GraphicsDeviceVk(windowHandle, debugLayer);
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