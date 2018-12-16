#include "VKShaderModule.h"
#include <vector>
#include "Graphics/Vulkan/VKContext.h"
#include "Utility/Utility.h"

VEngine::VKShaderModule::VKShaderModule(const char *filepath)
{
	std::vector<char> code = Utility::readBinaryFile(filepath);
	VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	if (vkCreateShaderModule(g_context.m_device, &createInfo, nullptr, &m_shaderModule) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create shader module!", -1);
	}
}

VEngine::VKShaderModule::~VKShaderModule()
{
	vkDestroyShaderModule(g_context.m_device, m_shaderModule, nullptr);
}

VkShaderModule VEngine::VKShaderModule::get()
{
	return m_shaderModule;
}
