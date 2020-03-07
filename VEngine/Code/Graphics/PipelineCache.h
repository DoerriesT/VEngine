#pragma once
#include "gal/GraphicsAbstractionLayer.h"
#include <unordered_map>

namespace VEngine
{
	struct HashedGraphicsPipelineCreateInfo
	{
		gal::GraphicsPipelineCreateInfo m_createInfo;
		size_t m_hashValue;
	};

	struct HashedComputePipelineCreateInfo
	{
		gal::ComputePipelineCreateInfo m_createInfo;
		size_t m_hashValue;
	};

	bool operator==(const HashedGraphicsPipelineCreateInfo &lhs, const HashedGraphicsPipelineCreateInfo &rhs);
	bool operator==(const HashedComputePipelineCreateInfo &lhs, const HashedComputePipelineCreateInfo &rhs);

	struct GraphicsPipelineHash { size_t operator()(const HashedGraphicsPipelineCreateInfo &value) const; };
	struct ComputePipelineHash { size_t operator()(const HashedComputePipelineCreateInfo &value) const; };

	class PipelineCache
	{
	public:
		explicit PipelineCache(gal::GraphicsDevice *graphicsDevice);
		PipelineCache(PipelineCache &) = delete;
		PipelineCache(PipelineCache &&) = delete;
		PipelineCache &operator= (const PipelineCache &) = delete;
		PipelineCache &operator= (const PipelineCache &&) = delete;
		~PipelineCache();
		gal::GraphicsPipeline *getPipeline(const gal::GraphicsPipelineCreateInfo &createInfo);
		gal::ComputePipeline *getPipeline(const gal::ComputePipelineCreateInfo &createInfo);

	private:
		gal::GraphicsDevice *m_graphicsDevice;
		std::unordered_map<HashedGraphicsPipelineCreateInfo, gal::GraphicsPipeline *, GraphicsPipelineHash> m_graphicsPipelines;
		std::unordered_map<HashedComputePipelineCreateInfo, gal::ComputePipeline *, ComputePipelineHash> m_computePipelines;
	};
}