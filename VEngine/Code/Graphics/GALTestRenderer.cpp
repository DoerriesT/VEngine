#include "GALTestRenderer.h"
#include "gal/GraphicsDevice.h"
#include "gal/SwapChain.h"
#include "gal/Initializers.h"
#include "gal/CommandListPool.h"
#include "gal/CommandList.h"
#include "gal/Semaphore.h"
#include "gal/Queue.h"

VEngine::GALTestRenderer::GALTestRenderer(uint32_t width, uint32_t height, void *windowHandle)
	:m_width(width),
	m_height(height),
	m_frameIndex(uint64_t() - 1),
	m_semaphoreValue(),
	m_frameSemaphoreValues(),
	m_graphicsDevice(gal::GraphicsDevice::create(windowHandle, true, gal::GraphicsBackend::VULKAN)),
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


	//gal::PipelineColorBlendAttachmentState colorBlendAttachments[]
	//{
	//	gal::GraphicsPipelineBuilder::s_defaultBlendAttachment,
	//};
	//
	//gal::DynamicState dynamicState[] = { gal::DynamicState::VIEWPORT, gal::DynamicState::SCISSOR };
	//
	//gal::GraphicsPipelineCreateInfo pipelineCreateInfo;
	//gal::GraphicsPipelineBuilder pipelineBuilder(pipelineCreateInfo);
	//pipelineBuilder.setVertexShader("Resources/Shaders/galTest_vert.spv");
	//pipelineBuilder.setFragmentShader("Resources/Shaders/galTest_frag.spv");
	//pipelineBuilder.setPolygonModeCullMode(gal::PolygonMode::FILL, (gal::CullModeFlags)gal::CullModeFlagBits::BACK_BIT, gal::FrontFace::COUNTER_CLOCKWISE);
	//pipelineBuilder.setDepthTest(true, true, gal::CompareOp::GREATER_OR_EQUAL);
	//pipelineBuilder.setColorBlendAttachment(gal::GraphicsPipelineBuilder::s_defaultBlendAttachment);
	//pipelineBuilder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
	//pipelineBuilder.setColorAttachmentFormat(m_swapChain->getImageFormat());
	//pipelineBuilder.setDepthStencilAttachmentFormat(gal::Format::D32_SFLOAT);
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
	gal::GraphicsDevice::destroy(m_graphicsDevice);
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
	gal::ImageViewCreateInfo imageViewCreateInfo{ swapChainImage };
	m_graphicsDevice->createImageView(imageViewCreateInfo, &m_imageViews[resIdx]);

	// reset command list pool of this frame
	m_cmdListPools[resIdx]->reset();

	auto *cmdList = m_cmdLists[resIdx];
	cmdList->begin();
	{
		// transition from UNDEFINED to COLOR_ATTACHMENT
		gal::Barrier barrier0 = gal::Initializers::imageBarrier(swapChainImage,
			(uint32_t)gal::PipelineStageFlagBits::TOP_OF_PIPE_BIT,
			(uint32_t)gal::PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT,
			gal::ResourceState::UNDEFINED,
			gal::ResourceState::WRITE_ATTACHMENT);
		cmdList->barrier(1, &barrier0);
		
		gal::ColorAttachmentDescription colorAttachmentDesc = { m_imageViews[resIdx], gal::AttachmentLoadOp::CLEAR, gal::AttachmentStoreOp::STORE, {0.2f, 0.5f, 1.0f} };
		cmdList->beginRenderPass(1, &colorAttachmentDesc, nullptr, { {}, {m_width, m_height} });
		{
			// render stuff
		}
		cmdList->endRenderPass();

		// transition from COLOR_ATTACHMENT to PRESENT
		gal::Barrier barrier1 = gal::Initializers::imageBarrier(swapChainImage,
			(uint32_t)gal::PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT,
			(uint32_t)gal::PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT,
			gal::ResourceState::WRITE_ATTACHMENT,
			gal::ResourceState::PRESENT_IMAGE);
		cmdList->barrier(1, &barrier1);
	}
	cmdList->end();

	uint64_t waitValue = m_semaphoreValue;
	uint64_t signalValue = ++m_semaphoreValue;
	gal::PipelineStageFlags waitDstStageMask = (uint32_t)gal::PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT;
	gal::SubmitInfo submitInfo;
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
