#pragma once
#include <vulkan\vulkan.h>

namespace VEngine
{
	class VKShaderModule
	{
	public:
		explicit VKShaderModule(const char *filepath);
		~VKShaderModule();
		VkShaderModule get();

	private:
		VkShaderModule m_shaderModule;
	};
}