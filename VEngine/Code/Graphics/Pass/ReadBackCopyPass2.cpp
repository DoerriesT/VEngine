#include "ReadBackCopyPass2.h"
#include "Graphics/RendererConsts.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

void VEngine::ReadBackCopyPass2::addToGraph(rg::RenderGraph2 &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_srcBuffer), {gal::ResourceState::READ_BUFFER_TRANSFER, PipelineStageFlagBits::TRANSFER_BIT}},
	};

	graph.addPass("Read Back Copy", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry2 &registry)
		{
			cmdList->copyBuffer(registry.getBuffer(data.m_srcBuffer), data.m_dstBuffer, 1, &data.m_bufferCopy);
			
			Barrier barrier = Initializers::bufferBarrier(data.m_dstBuffer, PipelineStageFlagBits::TRANSFER_BIT, PipelineStageFlagBits::HOST_BIT, ResourceState::WRITE_BUFFER_TRANSFER, ResourceState::READ_BUFFER_HOST);
			cmdList->barrier(1, &barrier);
		}, true);
}
