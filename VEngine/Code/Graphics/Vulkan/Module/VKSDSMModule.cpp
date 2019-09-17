#include "VKSDSMModule.h"
#include "Graphics/Vulkan/Pass/VKSDSMClearPass.h"
#include "Graphics/Vulkan/Pass/VKSDSMDepthReducePass.h"
#include "Graphics/Vulkan/Pass/VKSDSMSplitsPass.h"
#include "Graphics/Vulkan/Pass/VKSDSMBoundsReducePass.h"
#include "Graphics/Vulkan/VKResourceDefinitions.h"

void VEngine::VKSDSMModule::addToGraph(RenderGraph &graph, const InputData &inData, OutputData &outData)
{
	BufferViewHandle sdsmDepthBoundsBufferHandle = VKResourceDefinitions::createSDSMDepthBoundsBufferViewHandle(graph);
	outData.m_splitsBufferHandle = VKResourceDefinitions::createSDSMSplitsBufferViewHandle(graph, 4);
	outData.m_partitionBoundsBufferHandle = VKResourceDefinitions::createSDSMPartitionBoundsBufferViewHandle(graph, 4);

	// sdsm clear
	VKSDSMClearPass::Data sdsmClearPassData;
	sdsmClearPassData.m_passRecordContext = inData.m_passRecordContext;
	sdsmClearPassData.m_depthBoundsBufferHandle = sdsmDepthBoundsBufferHandle;
	sdsmClearPassData.m_partitionBoundsBufferHandle = outData.m_partitionBoundsBufferHandle;

	VKSDSMClearPass::addToGraph(graph, sdsmClearPassData);


	// sdsm depth reduce
	VKSDSMDepthReducePass::Data sdsmDepthReducePassData;
	sdsmDepthReducePassData.m_passRecordContext = inData.m_passRecordContext;
	sdsmDepthReducePassData.m_depthBoundsBufferHandle = sdsmDepthBoundsBufferHandle;
	sdsmDepthReducePassData.m_depthImageHandle = inData.m_depthImageHandle;

	VKSDSMDepthReducePass::addToGraph(graph, sdsmDepthReducePassData);


	// sdsm splits
	VKSDSMSplitsPass::Data sdsmSplitsPassData;
	sdsmSplitsPassData.m_passRecordContext = inData.m_passRecordContext;
	sdsmSplitsPassData.m_depthBoundsBufferHandle = sdsmDepthBoundsBufferHandle;
	sdsmSplitsPassData.m_splitsBufferHandle = outData.m_splitsBufferHandle;

	VKSDSMSplitsPass::addToGraph(graph, sdsmSplitsPassData);


	// sdsm bounds reduce
	VKSDSMBoundsReducePass::Data sdsmBoundsReducePassData;
	sdsmBoundsReducePassData.m_passRecordContext = inData.m_passRecordContext;
	sdsmBoundsReducePassData.m_partitionBoundsBufferHandle = outData.m_partitionBoundsBufferHandle;
	sdsmBoundsReducePassData.m_splitsBufferHandle = outData.m_splitsBufferHandle;
	sdsmBoundsReducePassData.m_depthImageHandle = inData.m_depthImageHandle;

	VKSDSMBoundsReducePass::addToGraph(graph, sdsmBoundsReducePassData);
}
