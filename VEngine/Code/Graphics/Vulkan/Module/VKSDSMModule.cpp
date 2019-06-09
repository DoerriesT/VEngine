#include "VKSDSMModule.h"
#include "Graphics/Vulkan/Pass/VKSDSMClearPass.h"
#include "Graphics/Vulkan/Pass/VKSDSMDepthReducePass.h"
#include "Graphics/Vulkan/Pass/VKSDSMSplitsPass.h"
#include "Graphics/Vulkan/Pass/VKSDSMBoundsReducePass.h"
#include "Graphics/Vulkan/VKResourceDefinitions.h"

void VEngine::VKSDSMModule::addToGraph(FrameGraph::Graph &graph, const InputData &inData, OutputData &outData)
{
	FrameGraph::BufferHandle sdsmDepthBoundsBufferHandle = VKResourceDefinitions::createSDSMDepthBoundsBufferHandle(graph);
	FrameGraph::BufferHandle sdsmSplitsBufferHandle = VKResourceDefinitions::createSDSMSplitsBufferHandle(graph, 4);
	outData.m_partitionBoundsBufferHandle = VKResourceDefinitions::createSDSMPartitionBoundsBufferHandle(graph, 4);

	// sdsm clear
	VKSDSMClearPass::Data sdsmClearPassData;
	sdsmClearPassData.m_renderResources = inData.m_renderResources;
	sdsmClearPassData.m_pipelineCache = inData.m_pipelineCache;
	sdsmClearPassData.m_descriptorSetCache = inData.m_descriptorSetCache;
	sdsmClearPassData.m_depthBoundsBufferHandle = sdsmDepthBoundsBufferHandle;
	sdsmClearPassData.m_partitionBoundsBufferHandle = outData.m_partitionBoundsBufferHandle;

	VKSDSMClearPass::addToGraph(graph, sdsmClearPassData);


	// sdsm depth reduce
	VKSDSMDepthReducePass::Data sdsmDepthReducePassData;
	sdsmDepthReducePassData.m_renderResources = inData.m_renderResources;
	sdsmDepthReducePassData.m_pipelineCache = inData.m_pipelineCache;
	sdsmDepthReducePassData.m_descriptorSetCache = inData.m_descriptorSetCache;
	sdsmDepthReducePassData.m_width = inData.m_width;
	sdsmDepthReducePassData.m_height = inData.m_height;
	sdsmDepthReducePassData.m_depthBoundsBufferHandle = sdsmDepthBoundsBufferHandle;
	sdsmDepthReducePassData.m_depthImageHandle = inData.m_depthImageHandle;

	VKSDSMDepthReducePass::addToGraph(graph, sdsmDepthReducePassData);


	// sdsm splits
	VKSDSMSplitsPass::Data sdsmSplitsPassData;
	sdsmSplitsPassData.m_renderResources = inData.m_renderResources;
	sdsmSplitsPassData.m_pipelineCache = inData.m_pipelineCache;
	sdsmSplitsPassData.m_descriptorSetCache = inData.m_descriptorSetCache;
	sdsmSplitsPassData.m_nearPlane = inData.m_nearPlane;
	sdsmSplitsPassData.m_farPlane = inData.m_farPlane;
	sdsmSplitsPassData.m_depthBoundsBufferHandle = sdsmDepthBoundsBufferHandle;
	sdsmSplitsPassData.m_splitsBufferHandle = sdsmSplitsBufferHandle;

	VKSDSMSplitsPass::addToGraph(graph, sdsmSplitsPassData);


	// sdsm bounds reduce
	VKSDSMBoundsReducePass::Data sdsmBoundsReducePassData;
	sdsmBoundsReducePassData.m_renderResources = inData.m_renderResources;
	sdsmBoundsReducePassData.m_pipelineCache = inData.m_pipelineCache;
	sdsmBoundsReducePassData.m_descriptorSetCache = inData.m_descriptorSetCache;
	sdsmBoundsReducePassData.m_width = inData.m_width;
	sdsmBoundsReducePassData.m_height = inData.m_height;
	sdsmBoundsReducePassData.m_invProjection = inData.m_invProjection;
	sdsmBoundsReducePassData.m_partitionBoundsBufferHandle = outData.m_partitionBoundsBufferHandle;
	sdsmBoundsReducePassData.m_splitsBufferHandle = sdsmSplitsBufferHandle;
	sdsmBoundsReducePassData.m_depthImageHandle = inData.m_depthImageHandle;

	VKSDSMBoundsReducePass::addToGraph(graph, sdsmBoundsReducePassData);
}
