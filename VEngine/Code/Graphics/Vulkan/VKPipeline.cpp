#include "VKPipeline.h"
#include "Utility/Utility.h"

size_t VEngine::VKGraphicsPipelineDescriptionHash::operator()(const VKGraphicsPipelineDescription &value) const
{
	size_t result = 0;

	for (size_t i = 0; i < sizeof(VKGraphicsPipelineDescription); ++i)
	{
		Utility::hashCombine(result, reinterpret_cast<const char *>(&value)[i]);
	}

	return result;
}

size_t VEngine::VKComputePipelineDescriptionHash::operator()(const VKComputePipelineDescription &value) const
{
	size_t result = 0;
	
	for (size_t i = 0; i < sizeof(VKComputePipelineDescription); ++i)
	{
		Utility::hashCombine(result, reinterpret_cast<const char *>(&value)[i]);
	}

	return result;
}