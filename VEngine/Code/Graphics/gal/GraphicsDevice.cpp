#include "GraphicsDevice.h"
#include "vulkan/GraphicsDeviceVk.h"

VEngine::gal::GraphicsDevice *VEngine::gal::GraphicsDevice::create(void *windowHandle, bool debugLayer, GraphicsBackend backend)
{
	switch (backend)
	{
	case GraphicsBackend::VULKAN:
		return new GraphicsDeviceVk(windowHandle, debugLayer);
	default:
		assert(false);
		break;
	}
	return nullptr;
}
