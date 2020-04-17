#include "PipelineCache.h"
#include "Utility/Utility.h"

bool VEngine::operator==(const HashedGraphicsPipelineCreateInfo &lhs, const HashedGraphicsPipelineCreateInfo &rhs)
{
	return memcmp(&lhs.m_createInfo, &rhs.m_createInfo, sizeof(lhs.m_createInfo)) == 0;
}

bool VEngine::operator==(const HashedComputePipelineCreateInfo &lhs, const HashedComputePipelineCreateInfo &rhs)
{
	return memcmp(&lhs.m_createInfo, &rhs.m_createInfo, sizeof(lhs.m_createInfo)) == 0;
}

size_t VEngine::GraphicsPipelineHash::operator()(const HashedGraphicsPipelineCreateInfo &value) const
{
	return value.m_hashValue;
}

size_t VEngine::ComputePipelineHash::operator()(const HashedComputePipelineCreateInfo &value) const
{
	return value.m_hashValue;
}

VEngine::PipelineCache::PipelineCache(gal::GraphicsDevice *graphicsDevice)
	:m_graphicsDevice(graphicsDevice),
	m_graphicsPipelines(),
	m_computePipelines()
{
}

VEngine::PipelineCache::~PipelineCache()
{
	for (auto &p : m_graphicsPipelines)
	{
		m_graphicsDevice->destroyGraphicsPipeline(p.second);
	}

	for (auto &p : m_computePipelines)
	{
		m_graphicsDevice->destroyComputePipeline(p.second);
	}
}

VEngine::gal::GraphicsPipeline *VEngine::PipelineCache::getPipeline(const gal::GraphicsPipelineCreateInfo &createInfo)
{
	size_t hashValue = 0;
	for (size_t i = 0; i < sizeof(createInfo); ++i)
	{
		Utility::hashCombine(hashValue, reinterpret_cast<const char *>(&createInfo)[i]);
	}

	auto &pipeline = m_graphicsPipelines[{createInfo, hashValue}];

	if (!pipeline)
	{
		printf("Pipeline Cache: Creating Pipeline!\n");
		m_graphicsDevice->createGraphicsPipelines(1, &createInfo, &pipeline);
	}

	return pipeline;
}

VEngine::gal::ComputePipeline *VEngine::PipelineCache::getPipeline(const gal::ComputePipelineCreateInfo &createInfo)
{
	size_t hashValue = 0;
	for (size_t i = 0; i < sizeof(createInfo); ++i)
	{
		Utility::hashCombine(hashValue, reinterpret_cast<const char *>(&createInfo)[i]);
	}

	auto &pipeline = m_computePipelines[{createInfo, hashValue}];

	if (!pipeline)
	{
		printf("Pipeline Cache: Creating Pipeline!\n");
		m_graphicsDevice->createComputePipelines(1, &createInfo, &pipeline);
	}

	return pipeline;
}
