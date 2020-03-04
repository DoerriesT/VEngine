#pragma once
#include "Graphics/gal/QueryPool.h"
#include "Graphics/Vulkan/volk.h"
#include "Graphics/gal/Common.h"

namespace VEngine
{
	namespace gal
	{
		class QueryPoolVk : public QueryPool
		{
		public:
			explicit QueryPoolVk(VkDevice device, QueryType queryType, uint32_t queryCount, QueryPipelineStatisticFlags pipelineStatistics);
			QueryPoolVk(QueryPoolVk &) = delete;
			QueryPoolVk(QueryPoolVk &&) = delete;
			QueryPoolVk &operator= (const QueryPoolVk &) = delete;
			QueryPoolVk &operator= (const QueryPoolVk &&) = delete;
			~QueryPoolVk();
			void *getNativeHandle() const override;

		private:
			VkDevice m_device;
			VkQueryPool m_queryPool;
		};
	}
}