#include "VKPipeline.h"
#include "Utility/Utility.h"
#include <cassert>

void VEngine::RenderPassDescription::finalize()
{
	m_hashValue = 0;

	for (size_t i = 0; i < sizeof(RenderPassDescription); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

void VEngine::RenderPassCompatibilityDescription::finalize()
{
	m_hashValue = 0;

	for (size_t i = 0; i < sizeof(RenderPassCompatibilityDescription); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

void VEngine::VKGraphicsPipelineDescription::finalize()
{
	m_hashValue = 0;

	for (size_t i = 0; i < sizeof(VKGraphicsPipelineDescription); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

void VEngine::VKComputePipelineDescription::finalize()
{
	m_hashValue = 0;

	for (size_t i = 0; i < sizeof(VKComputePipelineDescription); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

size_t VEngine::RenderPassDescriptionHash::operator()(const RenderPassDescription & value) const
{
	return value.m_hashValue;
}

size_t VEngine::RenderPassCompatibilityDescriptionHash::operator()(const RenderPassCompatibilityDescription & value) const
{
	return value.m_hashValue;
}

size_t VEngine::VKGraphicsPipelineDescriptionHash::operator()(const VKGraphicsPipelineDescription &value) const
{
	return value.m_hashValue;
}

size_t VEngine::VKComputePipelineDescriptionHash::operator()(const VKComputePipelineDescription &value) const
{
	return value.m_hashValue;
}

size_t VEngine::VKCombinedGraphicsPipelineRenderPassDescriptionHash::operator()(const VKCombinedGraphicsPipelineRenderPassDescription &value) const
{
	size_t result = value.m_graphicsPipelineDescription.m_hashValue;
	Utility::hashCombine(result, value.m_renderPassCompatibilityDescription.m_hashValue);
	return result;
}

VEngine::VKShaderStageDescription::SpecializationInfo::SpecializationInfo()
{
	m_info.mapEntryCount = 0;
	m_info.pMapEntries = m_entries;
	m_info.pData = m_data;
	m_info.dataSize = sizeof(m_data);
}

void VEngine::VKShaderStageDescription::SpecializationInfo::addEntry(uint32_t constantID, int32_t value)
{
	addEntry(constantID, &value);
}

void VEngine::VKShaderStageDescription::SpecializationInfo::addEntry(uint32_t constantID, uint32_t value)
{
	addEntry(constantID, &value);
}

void VEngine::VKShaderStageDescription::SpecializationInfo::addEntry(uint32_t constantID, float value)
{
	addEntry(constantID, &value);
}

const VkSpecializationInfo *VEngine::VKShaderStageDescription::SpecializationInfo::getInfo() const
{
	return &m_info;
}

void VEngine::VKShaderStageDescription::SpecializationInfo::addEntry(uint32_t constantID, void *value)
{
	assert(m_info.mapEntryCount < MAX_ENTRY_COUNT);
	memcpy(&m_data[m_info.mapEntryCount * 4], value, 4);
	m_entries[m_info.mapEntryCount] = { constantID, m_info.mapEntryCount * 4, 4 };
	++m_info.mapEntryCount;
}
