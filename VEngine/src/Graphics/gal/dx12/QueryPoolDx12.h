#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>

namespace VEngine
{
	namespace gal
	{
		class QueryPoolDx12 : public QueryPool
		{
		public:
			explicit QueryPoolDx12(ID3D12Device *device, QueryType queryType, uint32_t queryCount, QueryPipelineStatisticFlags pipelineStatistics);
			QueryPoolDx12(QueryPoolDx12 &) = delete;
			QueryPoolDx12(QueryPoolDx12 &&) = delete;
			QueryPoolDx12 &operator= (const QueryPoolDx12 &) = delete;
			QueryPoolDx12 &operator= (const QueryPoolDx12 &&) = delete;
			~QueryPoolDx12();
			void *getNativeHandle() const override;
			D3D12_QUERY_HEAP_TYPE getHeapType() const;

		private:
			ID3D12QueryHeap *m_queryHeap;
			D3D12_QUERY_HEAP_TYPE m_heapType;
		};
	}
}