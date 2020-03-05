#include "GALTestRenderer.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"

using namespace VEngine::gal;

VEngine::GALTestRenderer::GALTestRenderer(uint32_t width, uint32_t height, void *windowHandle)
	:m_width(width),
	m_height(height),
	m_frameIndex(uint64_t() - 1),
	m_semaphoreValue(),
	m_frameSemaphoreValues(),
	m_graphicsDevice(GraphicsDevice::create(windowHandle, true, GraphicsBackend::VULKAN)),
	m_queue(),
	m_swapChain(),
	m_semaphore(),
	m_pipeline(),
	m_vertexBuffer(),
	m_indexBuffer(),
	m_cmdListPools(),
	m_cmdLists(),
	m_imageViews()
{
	m_queue = m_graphicsDevice->getGraphicsQueue();
	m_graphicsDevice->createSwapChain(m_queue, width, height, &m_swapChain);
	m_graphicsDevice->createSemaphore(0, &m_semaphore);


	//PipelineColorBlendAttachmentState colorBlendAttachments[]
	//{
	//	GraphicsPipelineBuilder::s_defaultBlendAttachment,
	//};
	//
	//DynamicState dynamicState[] = { DynamicState::VIEWPORT, DynamicState::SCISSOR };
	//
	//GraphicsPipelineCreateInfo pipelineCreateInfo;
	//GraphicsPipelineBuilder pipelineBuilder(pipelineCreateInfo);
	//pipelineBuilder.setVertexShader("Resources/Shaders/galTest_vert.spv");
	//pipelineBuilder.setFragmentShader("Resources/Shaders/galTest_frag.spv");
	//pipelineBuilder.setPolygonModeCullMode(PolygonMode::FILL, (CullModeFlags)CullModeFlagBits::BACK_BIT, FrontFace::COUNTER_CLOCKWISE);
	//pipelineBuilder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
	//pipelineBuilder.setColorBlendAttachment(GraphicsPipelineBuilder::s_defaultBlendAttachment);
	//pipelineBuilder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
	//pipelineBuilder.setColorAttachmentFormat(m_swapChain->getImageFormat());
	//pipelineBuilder.setDepthStencilAttachmentFormat(Format::D32_SFLOAT);
	//
	//m_graphicsDevice->createGraphicsPipelines(1, &pipelineCreateInfo, &m_pipeline);

	m_graphicsDevice->createCommandListPool(m_queue, &m_cmdListPools[0]);
	m_graphicsDevice->createCommandListPool(m_queue, &m_cmdListPools[1]);
	m_cmdListPools[0]->allocate(1, &m_cmdLists[0]);
	m_cmdListPools[1]->allocate(1, &m_cmdLists[1]);
}

VEngine::GALTestRenderer::~GALTestRenderer()
{
	m_graphicsDevice->waitIdle();
	m_graphicsDevice->destroyImageView(m_imageViews[0]);
	m_graphicsDevice->destroyImageView(m_imageViews[1]);
	m_graphicsDevice->destroyCommandListPool(m_cmdListPools[0]);
	m_graphicsDevice->destroyCommandListPool(m_cmdListPools[1]);
	m_graphicsDevice->destroySemaphore(m_semaphore);
	m_graphicsDevice->destroySwapChain();
	GraphicsDevice::destroy(m_graphicsDevice);
}

void VEngine::GALTestRenderer::render()
{
	++m_frameIndex;
	uint64_t resIdx = m_frameIndex % 2;

	// wait for frame - 2 to complete
	m_semaphore->wait(m_frameSemaphoreValues[resIdx]);
	
	m_graphicsDevice->beginFrame();

	// acquire swapchain image
	uint32_t swapChainIndex;
	m_swapChain->getCurrentImageIndex(swapChainIndex, m_semaphore, ++m_semaphoreValue);
	auto *swapChainImage = m_swapChain->getImage(swapChainIndex);

	// destroy old image view
	m_graphicsDevice->destroyImageView(m_imageViews[resIdx]);

	// create new image view
	ImageViewCreateInfo imageViewCreateInfo{ swapChainImage };
	m_graphicsDevice->createImageView(imageViewCreateInfo, &m_imageViews[resIdx]);

	// reset command list pool of this frame
	m_cmdListPools[resIdx]->reset();

	auto *cmdList = m_cmdLists[resIdx];
	cmdList->begin();
	{
		// transition from UNDEFINED to COLOR_ATTACHMENT
		Barrier barrier0 = Initializers::imageBarrier(swapChainImage,
			(uint32_t)PipelineStageFlagBits::TOP_OF_PIPE_BIT,
			(uint32_t)PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT,
			ResourceState::UNDEFINED,
			ResourceState::WRITE_ATTACHMENT);
		cmdList->barrier(1, &barrier0);
		
		ColorAttachmentDescription colorAttachmentDesc = { m_imageViews[resIdx], AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, {0.2f, 0.5f, 1.0f} };
		cmdList->beginRenderPass(1, &colorAttachmentDesc, nullptr, { {}, {m_width, m_height} });
		{
			// render stuff
		}
		cmdList->endRenderPass();

		// transition from COLOR_ATTACHMENT to PRESENT
		Barrier barrier1 = Initializers::imageBarrier(swapChainImage,
			(uint32_t)PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT,
			(uint32_t)PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT,
			ResourceState::WRITE_ATTACHMENT,
			ResourceState::PRESENT_IMAGE);
		cmdList->barrier(1, &barrier1);
	}
	cmdList->end();

	uint64_t waitValue = m_semaphoreValue;
	uint64_t signalValue = ++m_semaphoreValue;
	PipelineStageFlags waitDstStageMask = (uint32_t)PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT;
	SubmitInfo submitInfo;
	submitInfo.m_waitSemaphoreCount = 1;
	submitInfo.m_waitSemaphores = &m_semaphore;
	submitInfo.m_waitValues = &waitValue;
	submitInfo.m_waitDstStageMask = &waitDstStageMask;
	submitInfo.m_commandListCount = 1;
	submitInfo.m_commandLists = &cmdList;
	submitInfo.m_signalSemaphoreCount = 1;
	submitInfo.m_signalSemaphores = &m_semaphore;
	submitInfo.m_signalValues = &signalValue;

	m_queue->submit(1, &submitInfo);

	m_swapChain->present(m_semaphore, m_semaphoreValue);

	m_frameSemaphoreValues[resIdx] = m_semaphoreValue;

	m_graphicsDevice->endFrame();
}
