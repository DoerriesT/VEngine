#include "SwapChainDx12.h"
#include "UtilityDx12.h"
#include "GraphicsDeviceDx12.h"

VEngine::gal::SwapChainDx12::SwapChainDx12(GraphicsDeviceDx12 *graphicsDevice, ID3D12Device *device, void *windowHandle, Queue *presentQueue, uint32_t width, uint32_t height)
    :m_graphicsDeviceDx12(graphicsDevice),
    m_device(device),
    m_windowHandle(windowHandle),
    m_presentQueue(presentQueue),
    m_swapChain(),
    m_imageCount(),
    m_images(),
    m_imageMemoryPool(),
    m_imageFormat(),
    m_extent(),
    m_currentImageIndex()
{
    create(width, height);
}

VEngine::gal::SwapChainDx12::~SwapChainDx12()
{
    destroy();
}

void *VEngine::gal::SwapChainDx12::getNativeHandle() const
{
	return m_swapChain;
}

void VEngine::gal::SwapChainDx12::resize(uint32_t width, uint32_t height)
{
    m_graphicsDeviceDx12->waitIdle();
    destroy();
    create(width, height);
}

uint32_t VEngine::gal::SwapChainDx12::getCurrentImageIndex()
{
	return m_swapChain->GetCurrentBackBufferIndex();
}

void VEngine::gal::SwapChainDx12::present(Semaphore *waitSemaphore, uint64_t semaphoreWaitValue, Semaphore *signalSemaphore, uint64_t semaphoreSignalValue)
{
	//waitSemaphore->wait(semaphoreWaitValue);
	UtilityDx12::checkResult(m_swapChain->Present(0, 0), "Failed to present!");
	signalSemaphore->signal(semaphoreSignalValue);
}

VEngine::gal::Extent2D VEngine::gal::SwapChainDx12::getExtent() const
{
	return m_extent;
}

VEngine::gal::Extent2D VEngine::gal::SwapChainDx12::getRecreationExtent() const
{
	return m_extent;
}

VEngine::gal::Format VEngine::gal::SwapChainDx12::getImageFormat() const
{
	return m_imageFormat;
}

VEngine::gal::Image *VEngine::gal::SwapChainDx12::getImage(size_t index) const
{
    assert(index < m_imageCount);
    return m_images[index];
}

VEngine::gal::Queue *VEngine::gal::SwapChainDx12::getPresentQueue() const
{
	return m_presentQueue;
}

void VEngine::gal::SwapChainDx12::create(uint32_t width, uint32_t height)
{
    IDXGIFactory4 *factory;
    UtilityDx12::checkResult(CreateDXGIFactory2(0, __uuidof(IDXGIFactory4), (void **)&factory), "Failed to create DXGIFactory!");

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = s_maxImageCount;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    IDXGISwapChain1 *swapChain;
    UtilityDx12::checkResult(factory->CreateSwapChainForHwnd((ID3D12CommandQueue *)m_presentQueue->getNativeHandle(), (HWND)m_windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain), "Failed to create swapchain!");
    UtilityDx12::checkResult(swapChain->QueryInterface(__uuidof(IDXGISwapChain4), (void **)&m_swapChain), "Failed to create swapchain!");

    UtilityDx12::checkResult(factory->MakeWindowAssociation((HWND)m_windowHandle, DXGI_MWA_NO_ALT_ENTER), "Failed to create swapchain!");

    m_imageCount = s_maxImageCount;

    ImageCreateInfo imageCreateInfo{};
    imageCreateInfo.m_width = width;
    imageCreateInfo.m_height = height;
    imageCreateInfo.m_depth = 1;
    imageCreateInfo.m_layers = 1;
    imageCreateInfo.m_levels = 1;
    imageCreateInfo.m_samples = SampleCount::_1;
    imageCreateInfo.m_imageType = ImageType::_2D;
    imageCreateInfo.m_format = static_cast<Format>(swapChainDesc.Format);
    imageCreateInfo.m_createFlags = 0;
    imageCreateInfo.m_usageFlags = ImageUsageFlagBits::TRANSFER_DST_BIT | ImageUsageFlagBits::STORAGE_BIT | ImageUsageFlagBits::COLOR_ATTACHMENT_BIT;

    for (uint32_t i = 0; i < m_imageCount; ++i)
    {
        ID3D12Resource *imageDx;
        UtilityDx12::checkResult(m_swapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void **)&imageDx), "Failed to retrieve swapchain image!");
        auto *memory = m_imageMemoryPool.alloc();
        assert(memory);

        m_images[i] = new(memory) ImageDx12(imageDx, nullptr, imageCreateInfo);
    }

    m_extent.m_width = width;
    m_extent.m_height = height;

    m_imageFormat = Format::B8G8R8A8_UNORM;
}

void VEngine::gal::SwapChainDx12::destroy()
{
    for (uint32_t i = 0; i < m_imageCount; ++i)
    {
        // call destructor and free backing memory
        m_images[i]->~ImageDx12();
        m_imageMemoryPool.free(reinterpret_cast<ByteArray<sizeof(ImageDx12)> *>(m_images[i]));
    }
}