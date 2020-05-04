#include "QueryPoolDx12.h"
#include "UtilityDx12.h"

VEngine::gal::QueryPoolDx12::QueryPoolDx12(ID3D12Device *device, QueryType queryType, uint32_t queryCount, QueryPipelineStatisticFlags pipelineStatistics)
	:m_queryHeap(),
	m_heapType(UtilityDx12::translate(queryType))
{
	D3D12_QUERY_HEAP_DESC queryHeapDesc{ m_heapType, queryCount, 0 };
	UtilityDx12::checkResult(device->CreateQueryHeap(&queryHeapDesc, __uuidof(ID3D12QueryHeap), (void **)&m_queryHeap), "Failed to create query heap!");
}

VEngine::gal::QueryPoolDx12::~QueryPoolDx12()
{
	m_queryHeap->Release();
}

void *VEngine::gal::QueryPoolDx12::getNativeHandle() const
{
	return m_queryHeap;
}

D3D12_QUERY_HEAP_TYPE VEngine::gal::QueryPoolDx12::getHeapType() const
{
	return m_heapType;
}
