#include "GraphicsDeviceDx12.h"
#include "SwapChainDx12.h"
#include "UtilityDx12.h"
#include "Utility/Utility.h"

#define GPU_DESCRIPTOR_HEAP_SIZE (1000000)
#define GPU_SAMPLER_DESCRIPTOR_HEAP_SIZE (1024)
#define CPU_DESCRIPTOR_HEAP_SIZE (1000000)
#define CPU_SAMPLER_DESCRIPTOR_HEAP_SIZE (1024)
#define CPU_RTV_DESCRIPTOR_HEAP_SIZE (1024)
#define CPU_DSV_DESCRIPTOR_HEAP_SIZE (1024)

VEngine::gal::GraphicsDeviceDx12::GraphicsDeviceDx12(void *windowHandle, bool debugLayer)
	:m_device(),
	m_graphicsQueue(),
	m_computeQueue(),
	m_transferQueue(),
	m_windowHandle(windowHandle),
	//m_gpuMemoryAllocator(),
	m_swapChain(),
	m_cmdListRecordContext(),
	m_graphicsPipelineMemoryPool(64),
	m_computePipelineMemoryPool(64),
	m_commandListPoolMemoryPool(32),
	m_gpuDescriptorAllocator(GPU_DESCRIPTOR_HEAP_SIZE, 1),
	m_gpuSamplerDescriptorAllocator(GPU_SAMPLER_DESCRIPTOR_HEAP_SIZE, 1),
	m_cpuDescriptorAllocator(CPU_DESCRIPTOR_HEAP_SIZE, 1),
	m_cpuSamplerDescriptorAllocator(CPU_SAMPLER_DESCRIPTOR_HEAP_SIZE, 1),
	m_cpuRTVDescriptorAllocator(CPU_RTV_DESCRIPTOR_HEAP_SIZE, 1),
	m_cpuDSVDescriptorAllocator(CPU_DSV_DESCRIPTOR_HEAP_SIZE, 1),
	m_imageMemoryPool(256),
	m_bufferMemoryPool(256),
	m_imageViewMemoryPool(512),
	m_bufferViewMemoryPool(32),
	m_samplerMemoryPool(16),
	m_semaphoreMemoryPool(16),
	m_queryPoolMemoryPool(16),
	m_descriptorSetPoolMemoryPool(16),
	m_descriptorSetLayoutMemoryPool(8),
	m_debugLayers(debugLayer)
{
	// Enable the D3D12 debug layer.
	{
		ID3D12Debug *debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void **)&debugController)))
		{
			debugController->EnableDebugLayer();
		}
		debugController->Release();
	}

	// get adapter
	IDXGIAdapter4 *dxgiAdapter4 = nullptr;
	{
		IDXGIFactory4 *dxgiFactory;
		UINT createFactoryFlags = 0;

		if (debugLayer)
		{
			createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
		}

		UtilityDx12::checkResult(CreateDXGIFactory2(createFactoryFlags, __uuidof(IDXGIFactory4), (void **)&dxgiFactory), "Failed to create DXGIFactory!");

		IDXGIAdapter1 *dxgiAdapter1;

		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// check to see if the adapter can create a D3D12 device without actually
			// creating it. the adapter with the largest dedicated video memory is favored
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				UtilityDx12::checkResult(dxgiAdapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void **)&dxgiAdapter4), "Failed to create DXGIAdapter4!");
			}
		}
	}

	// create device
	{
		ID3D12Device2 *d3d12Device2;
		UtilityDx12::checkResult(D3D12CreateDevice(dxgiAdapter4, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), (void **)&d3d12Device2), "Failed to create device!");

		if (debugLayer)
		{
			ID3D12InfoQueue *pInfoQueue;
			if (SUCCEEDED(d3d12Device2->QueryInterface(__uuidof(ID3D12InfoQueue), (void **)&pInfoQueue)))
			{
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

				// suppress whole categories of messages
				//D3D12_MESSAGE_CATEGORY Categories[] = {};

				// suppress messages based on their severity level
				D3D12_MESSAGE_SEVERITY Severities[] =
				{
					D3D12_MESSAGE_SEVERITY_INFO
				};

				// suppress individual messages by their ID
				D3D12_MESSAGE_ID DenyIds[] =
				{
					D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
					D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
					D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
				};

				D3D12_INFO_QUEUE_FILTER NewFilter = {};
				//NewFilter.DenyList.NumCategories = _countof(Categories);
				//NewFilter.DenyList.pCategoryList = Categories;
				NewFilter.DenyList.NumSeverities = _countof(Severities);
				NewFilter.DenyList.pSeverityList = Severities;
				NewFilter.DenyList.NumIDs = _countof(DenyIds);
				NewFilter.DenyList.pIDList = DenyIds;

				//UtilityDx12::checkResult(pInfoQueue->PushStorageFilter(&NewFilter), "Failed to set info queue filter!");
				pInfoQueue->Release();
			}
		}

		m_device = d3d12Device2;
	}
	dxgiAdapter4->Release();

	// create queues
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc{};
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 0;

		// graphics queue
		{
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			UtilityDx12::checkResult(m_device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void **)&m_graphicsQueue.m_queue), "Failed to create graphics queue!");
			m_graphicsQueue.m_queueFamily = 0;
			m_graphicsQueue.m_timestampValidBits = 64;
			m_graphicsQueue.m_presentable = true;
		}

		// compute queue
		{
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
			UtilityDx12::checkResult(m_device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void **)&m_computeQueue.m_queue), "Failed to create compute queue!");
			m_computeQueue.m_queueFamily = 1;
			m_computeQueue.m_timestampValidBits = 64;
			m_computeQueue.m_presentable = true;
		}

		// transfer queue
		{
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
			UtilityDx12::checkResult(m_device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void **)&m_transferQueue.m_queue), "Failed to create transfer queue!");
			m_transferQueue.m_queueFamily = 2;
			m_transferQueue.m_timestampValidBits = 64;
			m_transferQueue.m_presentable = false;
		}
	}

	// create descriptor heaps
	{
		// cpu heaps
		{
			// cpu heap
			D3D12_DESCRIPTOR_HEAP_DESC cpuHeapDesc{};
			cpuHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			cpuHeapDesc.NumDescriptors = CPU_DESCRIPTOR_HEAP_SIZE;
			cpuHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			cpuHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&cpuHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cpuDescriptorHeap), "Failed to create descriptor heap");

			// cpu sampler heap
			D3D12_DESCRIPTOR_HEAP_DESC cpuSamplerHeapDesc{};
			cpuSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			cpuSamplerHeapDesc.NumDescriptors = CPU_SAMPLER_DESCRIPTOR_HEAP_SIZE;
			cpuSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			cpuSamplerHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&cpuSamplerHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cpuSamplerDescriptorHeap), "Failed to create descriptor heap");

			// cpu rtv heap
			D3D12_DESCRIPTOR_HEAP_DESC cpuRtvHeapDesc{};
			cpuRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			cpuRtvHeapDesc.NumDescriptors = CPU_RTV_DESCRIPTOR_HEAP_SIZE;
			cpuRtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			cpuRtvHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&cpuRtvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cpuRTVDescriptorHeap), "Failed to create descriptor heap");

			// cpu dsv heap
			D3D12_DESCRIPTOR_HEAP_DESC cpuDsvHeapDesc{};
			cpuDsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			cpuDsvHeapDesc.NumDescriptors = CPU_DSV_DESCRIPTOR_HEAP_SIZE;
			cpuDsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			cpuDsvHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&cpuDsvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cpuDSVDescriptorHeap), "Failed to create descriptor heap");
		}

		// gpu heaps
		{
			// gpu heap
			D3D12_DESCRIPTOR_HEAP_DESC gpuHeapDesc{};
			gpuHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			gpuHeapDesc.NumDescriptors = GPU_DESCRIPTOR_HEAP_SIZE;
			gpuHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			gpuHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&gpuHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cmdListRecordContext.m_gpuDescriptorHeap), "Failed to create descriptor heap");

			// gpu sampler heap
			D3D12_DESCRIPTOR_HEAP_DESC gpuSamplerHeapDesc{};
			gpuSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			gpuSamplerHeapDesc.NumDescriptors = GPU_SAMPLER_DESCRIPTOR_HEAP_SIZE;
			gpuSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			gpuSamplerHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&gpuSamplerHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cmdListRecordContext.m_gpuSamplerDescriptorHeap), "Failed to create descriptor heap");
		}

		m_descriptorIncrementSizes[0] = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		m_descriptorIncrementSizes[1] = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		m_descriptorIncrementSizes[2] = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_descriptorIncrementSizes[3] = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}

	// create command signatures
	{
		// indirect draw
		D3D12_INDIRECT_ARGUMENT_DESC drawIndirectArgumentDesc{};
		drawIndirectArgumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

		D3D12_COMMAND_SIGNATURE_DESC drawIndirectCmdSigDesc{};
		drawIndirectCmdSigDesc.ByteStride = sizeof(DrawIndirectCommand);
		drawIndirectCmdSigDesc.NumArgumentDescs = 1;
		drawIndirectCmdSigDesc.pArgumentDescs = &drawIndirectArgumentDesc;
		drawIndirectCmdSigDesc.NodeMask = 0;

		UtilityDx12::checkResult(m_device->CreateCommandSignature(&drawIndirectCmdSigDesc, nullptr, __uuidof(ID3D12CommandSignature), (void **)&m_cmdListRecordContext.m_drawIndirectSignature), "Failed to create command signature!");


		// indirect draw indexed
		D3D12_INDIRECT_ARGUMENT_DESC drawIndexedIndirectArgumentDesc{};
		drawIndexedIndirectArgumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC drawIndexedIndirectCmdSigDesc{};
		drawIndexedIndirectCmdSigDesc.ByteStride = sizeof(DrawIndexedIndirectCommand);
		drawIndexedIndirectCmdSigDesc.NumArgumentDescs = 1;
		drawIndexedIndirectCmdSigDesc.pArgumentDescs = &drawIndexedIndirectArgumentDesc;
		drawIndexedIndirectCmdSigDesc.NodeMask = 0;

		UtilityDx12::checkResult(m_device->CreateCommandSignature(&drawIndexedIndirectCmdSigDesc, nullptr, __uuidof(ID3D12CommandSignature), (void **)&m_cmdListRecordContext.m_drawIndexedIndirectSignature), "Failed to create command signature!");


		// indirect dispatch
		D3D12_INDIRECT_ARGUMENT_DESC dispatchIndirectArgumentDesc{};
		dispatchIndirectArgumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

		D3D12_COMMAND_SIGNATURE_DESC dispatchIndirectCmdSigDesc{};
		dispatchIndirectCmdSigDesc.ByteStride = sizeof(DispatchIndirectCommand);
		dispatchIndirectCmdSigDesc.NumArgumentDescs = 1;
		dispatchIndirectCmdSigDesc.pArgumentDescs = &dispatchIndirectArgumentDesc;
		dispatchIndirectCmdSigDesc.NodeMask = 0;

		UtilityDx12::checkResult(m_device->CreateCommandSignature(&dispatchIndirectCmdSigDesc, nullptr, __uuidof(ID3D12CommandSignature), (void **)&m_cmdListRecordContext.m_dispatchIndirectSignature), "Failed to create command signature!");
	}
}

VEngine::gal::GraphicsDeviceDx12::~GraphicsDeviceDx12()
{
	waitIdle();

	//m_gpuMemoryAllocator->destroy();
	//delete m_gpuMemoryAllocator;
	if (m_swapChain)
	{
		delete m_swapChain;
	}

	m_cmdListRecordContext.m_drawIndirectSignature->Release();
	m_cmdListRecordContext.m_drawIndexedIndirectSignature->Release();
	m_cmdListRecordContext.m_dispatchIndirectSignature->Release();

	m_cmdListRecordContext.m_gpuDescriptorHeap->Release();
	m_cmdListRecordContext.m_gpuSamplerDescriptorHeap->Release();
	m_cpuDescriptorHeap->Release();
	m_cpuSamplerDescriptorHeap->Release();
	m_cpuRTVDescriptorHeap->Release();
	m_cpuDSVDescriptorHeap->Release();

	m_graphicsQueue.m_queue->Release();
	m_computeQueue.m_queue->Release();
	m_transferQueue.m_queue->Release();

	m_device->Release();
}

void VEngine::gal::GraphicsDeviceDx12::createGraphicsPipelines(uint32_t count, const GraphicsPipelineCreateInfo *createInfo, GraphicsPipeline **pipelines)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto *memory = m_graphicsPipelineMemoryPool.alloc();
		assert(memory);
		pipelines[i] = new(memory) GraphicsPipelineDx12(m_device, createInfo[i]);
	}
}

void VEngine::gal::GraphicsDeviceDx12::createComputePipelines(uint32_t count, const ComputePipelineCreateInfo *createInfo, ComputePipeline **pipelines)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto *memory = m_computePipelineMemoryPool.alloc();
		assert(memory);
		pipelines[i] = new(memory) ComputePipelineDx12(m_device, createInfo[i]);
	}
}

void VEngine::gal::GraphicsDeviceDx12::destroyGraphicsPipeline(GraphicsPipeline *pipeline)
{
	if (pipeline)
	{
		auto *pipelineDx = dynamic_cast<GraphicsPipelineDx12 *>(pipeline);
		assert(pipelineDx);

		// call destructor and free backing memory
		pipelineDx->~GraphicsPipelineDx12();
		m_graphicsPipelineMemoryPool.free(reinterpret_cast<ByteArray<sizeof(GraphicsPipelineDx12)> *>(pipelineDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::destroyComputePipeline(ComputePipeline *pipeline)
{
	if (pipeline)
	{
		auto *pipelineDx = dynamic_cast<ComputePipelineDx12 *>(pipeline);
		assert(pipelineDx);

		// call destructor and free backing memory
		pipelineDx->~ComputePipelineDx12();
		m_computePipelineMemoryPool.free(reinterpret_cast<ByteArray<sizeof(ComputePipelineDx12)> *>(pipelineDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::createCommandListPool(const Queue *queue, CommandListPool **commandListPool)
{
	auto *memory = m_commandListPoolMemoryPool.alloc();
	assert(memory);
	auto *queueVk = dynamic_cast<const QueueDx12 *>(queue);
	assert(queueVk);

	const uint32_t queueFamilyIdx = queueVk->getFamilyIndex();
	assert(queueFamilyIdx < 3);

	D3D12_COMMAND_LIST_TYPE cmdListTypes[]{ D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_TYPE_COPY };
	*commandListPool = new(memory) CommandListPoolDx12(m_device, cmdListTypes[queueFamilyIdx], &m_cmdListRecordContext);
}

void VEngine::gal::GraphicsDeviceDx12::destroyCommandListPool(CommandListPool *commandListPool)
{
	if (commandListPool)
	{
		auto *poolDx = dynamic_cast<CommandListPoolDx12 *>(commandListPool);
		assert(poolDx);

		// call destructor and free backing memory
		poolDx->~CommandListPoolDx12();
		m_commandListPoolMemoryPool.free(reinterpret_cast<ByteArray<sizeof(CommandListPoolDx12)> *>(poolDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::createQueryPool(QueryType queryType, uint32_t queryCount, QueryPool **queryPool)
{
	auto *memory = m_queryPoolMemoryPool.alloc();
	assert(memory);
	*queryPool = new(memory) QueryPoolDx12(m_device, queryType, queryCount, 0);
}

void VEngine::gal::GraphicsDeviceDx12::destroyQueryPool(QueryPool *queryPool)
{
	if (queryPool)
	{
		auto *poolDx = dynamic_cast<QueryPoolDx12 *>(queryPool);
		assert(poolDx);

		// call destructor and free backing memory
		poolDx->~QueryPoolDx12();
		m_queryPoolMemoryPool.free(reinterpret_cast<ByteArray<sizeof(QueryPoolDx12)> *>(poolDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::createImage(const ImageCreateInfo &imageCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Image **image)
{
	auto *memory = m_imageMemoryPool.alloc();
	assert(memory);

	ID3D12Resource *nativeHandle = nullptr; // TODO
	void *allocHandle = nullptr; // TODO

	*image = new(memory) ImageDx12(nativeHandle, allocHandle, imageCreateInfo);
}

void VEngine::gal::GraphicsDeviceDx12::createBuffer(const BufferCreateInfo &bufferCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Buffer **buffer)
{
	auto *memory = m_bufferMemoryPool.alloc();
	assert(memory);

	ID3D12Resource *nativeHandle = nullptr; // TODO
	void *allocHandle = nullptr; // TODO

	*buffer = new(memory) BufferDx12(nativeHandle, allocHandle, bufferCreateInfo);
}

void VEngine::gal::GraphicsDeviceDx12::destroyImage(Image *image)
{
	if (image)
	{
		auto *imageDx = dynamic_cast<ImageDx12 *>(image);
		assert(imageDx);
		//m_gpuMemoryAllocator->destroyImage((VkImage)imageVk->getNativeHandle(), reinterpret_cast<AllocationHandleVk>(imageVk->getAllocationHandle()));

		// call destructor and free backing memory
		imageDx->~ImageDx12();
		m_imageMemoryPool.free(reinterpret_cast<ByteArray<sizeof(ImageDx12)> *>(imageDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::destroyBuffer(Buffer *buffer)
{
	if (buffer)
	{
		auto *bufferDx = dynamic_cast<BufferDx12 *>(buffer);
		assert(bufferDx);
		//m_gpuMemoryAllocator->destroyBuffer((VkBuffer)bufferVk->getNativeHandle(), reinterpret_cast<AllocationHandleVk>(bufferVk->getAllocationHandle()));

		// call destructor and free backing memory
		bufferDx->~BufferDx12();
		m_bufferMemoryPool.free(reinterpret_cast<ByteArray<sizeof(BufferDx12)> *>(bufferDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::createImageView(const ImageViewCreateInfo &imageViewCreateInfo, ImageView **imageView)
{
	auto *memory = m_imageViewMemoryPool.alloc();
	assert(memory);

	*imageView = new(memory) ImageViewDx12(m_device, imageViewCreateInfo);
}

void VEngine::gal::GraphicsDeviceDx12::createImageView(Image *image, ImageView **imageView)
{
	const auto &imageDesc = image->getDescription();

	ImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.m_image = image;
	imageViewCreateInfo.m_viewType = static_cast<ImageViewType>(imageDesc.m_imageType);
	imageViewCreateInfo.m_format = imageDesc.m_format;
	imageViewCreateInfo.m_components = {};
	imageViewCreateInfo.m_baseMipLevel = 0;
	imageViewCreateInfo.m_levelCount = imageDesc.m_levels;
	imageViewCreateInfo.m_baseArrayLayer = 0;
	imageViewCreateInfo.m_layerCount = imageDesc.m_layers;

	createImageView(imageViewCreateInfo, imageView);
}

void VEngine::gal::GraphicsDeviceDx12::createBufferView(const BufferViewCreateInfo &bufferViewCreateInfo, BufferView **bufferView)
{
	auto *memory = m_bufferViewMemoryPool.alloc();
	assert(memory);

	*bufferView = new(memory) BufferViewDx12(m_device, bufferViewCreateInfo);
}

void VEngine::gal::GraphicsDeviceDx12::destroyImageView(ImageView *imageView)
{
	if (imageView)
	{
		auto *viewDx = dynamic_cast<ImageViewDx12 *>(imageView);
		assert(viewDx);

		// call destructor and free backing memory
		viewDx->~ImageViewDx12();
		m_imageViewMemoryPool.free(reinterpret_cast<ByteArray<sizeof(ImageViewDx12)> *>(viewDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::destroyBufferView(BufferView *bufferView)
{
	if (bufferView)
	{
		auto *viewDx = dynamic_cast<BufferViewDx12 *>(bufferView);
		assert(viewDx);

		// call destructor and free backing memory
		viewDx->~BufferViewDx12();
		m_bufferViewMemoryPool.free(reinterpret_cast<ByteArray<sizeof(BufferViewDx12)> *>(viewDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::createSampler(const SamplerCreateInfo &samplerCreateInfo, Sampler **sampler)
{
	auto *memory = m_samplerMemoryPool.alloc();
	assert(memory);

	*sampler = new(memory) SamplerDx12(m_device, samplerCreateInfo);
}

void VEngine::gal::GraphicsDeviceDx12::destroySampler(Sampler *sampler)
{
	if (sampler)
	{
		auto *samplerDx = dynamic_cast<SamplerDx12 *>(sampler);
		assert(samplerDx);

		// call destructor and free backing memory
		samplerDx->~SamplerDx12();
		m_samplerMemoryPool.free(reinterpret_cast<ByteArray<sizeof(SamplerDx12)> *>(samplerDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::createSemaphore(uint64_t initialValue, Semaphore **semaphore)
{
	auto *memory = m_semaphoreMemoryPool.alloc();
	assert(memory);

	*semaphore = new(memory) SemaphoreDx12(m_device, initialValue);
}

void VEngine::gal::GraphicsDeviceDx12::destroySemaphore(Semaphore *semaphore)
{
	if (semaphore)
	{
		auto *semaphoreDx = dynamic_cast<SemaphoreDx12 *>(semaphore);
		assert(semaphoreDx);

		// call destructor and free backing memory
		semaphoreDx->~SemaphoreDx12();
		m_semaphoreMemoryPool.free(reinterpret_cast<ByteArray<sizeof(SemaphoreDx12)> *>(semaphoreDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::createDescriptorSetPool(uint32_t maxSets, const DescriptorSetLayout *descriptorSetLayout, DescriptorSetPool **descriptorSetPool)
{
	auto *memory = m_descriptorSetPoolMemoryPool.alloc();
	assert(memory);

	const DescriptorSetLayoutDx12 *layoutDx = dynamic_cast<const DescriptorSetLayoutDx12 *>(descriptorSetLayout);
	assert(layoutDx);

	bool samplerPool = layoutDx->needsSamplerHeap();

	TLSFAllocator &heapAllocator = samplerPool ? m_gpuSamplerDescriptorAllocator : m_gpuDescriptorAllocator;
	ID3D12DescriptorHeap *heap = samplerPool ? m_cmdListRecordContext.m_gpuSamplerDescriptorHeap : m_cmdListRecordContext.m_gpuDescriptorHeap;
	UINT incSize = samplerPool ? m_descriptorIncrementSizes[1] : m_descriptorIncrementSizes[0];

	*descriptorSetPool = new(memory) DescriptorSetPoolDx12(m_device, heapAllocator, heap->GetCPUDescriptorHandleForHeapStart(), heap->GetGPUDescriptorHandleForHeapStart(), incSize, layoutDx, maxSets);
}

void VEngine::gal::GraphicsDeviceDx12::destroyDescriptorSetPool(DescriptorSetPool *descriptorSetPool)
{
	if (descriptorSetPool)
	{
		auto *poolDx = dynamic_cast<DescriptorSetPoolDx12 *>(descriptorSetPool);
		assert(poolDx);

		// call destructor and free backing memory
		poolDx->~DescriptorSetPoolDx12();
		m_descriptorSetPoolMemoryPool.free(reinterpret_cast<ByteArray<sizeof(DescriptorSetPoolDx12)> *>(poolDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::createDescriptorSetLayout(uint32_t bindingCount, const DescriptorSetLayoutBinding *bindings, DescriptorSetLayout **descriptorSetLayout)
{
	auto *memory = m_descriptorSetLayoutMemoryPool.alloc();
	assert(memory);

	uint32_t descriptorCount = 0;
	bool hasSamplers = false;
	bool hasNonSamplers = false;

	for (size_t i = 0; i < bindingCount; ++i)
	{
		descriptorCount += bindings[i].m_descriptorCount;
		auto type = bindings[i].m_descriptorType;
		hasSamplers = hasSamplers || type == DescriptorType::SAMPLER;
		hasNonSamplers = hasNonSamplers || type != DescriptorType::SAMPLER;
	}

	if (hasSamplers && hasNonSamplers)
	{
		Utility::fatalExit("Tried to create descriptor set layout with both sampler and non-sampler descriptors!", EXIT_FAILURE);
	}

	*descriptorSetLayout = new(memory) DescriptorSetLayoutDx12(descriptorCount, hasSamplers);
}

void VEngine::gal::GraphicsDeviceDx12::destroyDescriptorSetLayout(DescriptorSetLayout *descriptorSetLayout)
{
	if (descriptorSetLayout)
	{
		auto *layoutDx = dynamic_cast<DescriptorSetLayoutDx12 *>(descriptorSetLayout);
		assert(layoutDx);

		// call destructor and free backing memory
		layoutDx->~DescriptorSetLayoutDx12();
		m_descriptorSetLayoutMemoryPool.free(reinterpret_cast<ByteArray<sizeof(DescriptorSetLayoutDx12)> *>(layoutDx));
	}
}

void VEngine::gal::GraphicsDeviceDx12::createSwapChain(const Queue *presentQueue, uint32_t width, uint32_t height, SwapChain **swapChain)
{
	assert(!m_swapChain);
	assert(width && height);
	Queue *queue = nullptr;
	queue = presentQueue == &m_graphicsQueue ? &m_graphicsQueue : queue;
	queue = presentQueue == &m_computeQueue ? &m_computeQueue : queue;
	assert(queue);
	*swapChain = m_swapChain = new SwapChainDx12(this, m_device, m_windowHandle, queue, width, height);
}

void VEngine::gal::GraphicsDeviceDx12::destroySwapChain()
{
	assert(m_swapChain);
	delete m_swapChain;
	m_swapChain = nullptr;
}

void VEngine::gal::GraphicsDeviceDx12::waitIdle()
{
	m_graphicsQueue.waitIdle();
	m_computeQueue.waitIdle();
	m_transferQueue.waitIdle();
}

void VEngine::gal::GraphicsDeviceDx12::setDebugObjectName(ObjectType objectType, void *object, const char *name)
{
	constexpr size_t wStrLen = 1024;
	wchar_t wName[wStrLen];

	size_t ret;
	mbstowcs_s(&ret, wName, name, wStrLen - 1);

	switch (objectType)
	{
	case ObjectType::QUEUE:
		((ID3D12CommandQueue *)((QueueDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case VEngine::gal::ObjectType::SEMAPHORE:
		((ID3D12Fence *)((SemaphoreDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case VEngine::gal::ObjectType::COMMAND_LIST:
		((ID3D12GraphicsCommandList5 *)((CommandListDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case VEngine::gal::ObjectType::BUFFER:
		((ID3D12Resource *)((BufferDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case VEngine::gal::ObjectType::IMAGE:
		((ID3D12Resource *)((ImageDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case VEngine::gal::ObjectType::QUERY_POOL:
		((ID3D12QueryHeap *)((QueryPoolDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case VEngine::gal::ObjectType::BUFFER_VIEW:
		break;
	case VEngine::gal::ObjectType::IMAGE_VIEW:
		break;
	case VEngine::gal::ObjectType::GRAPHICS_PIPELINE:
		((ID3D12PipelineState *)((GraphicsPipelineDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case VEngine::gal::ObjectType::COMPUTE_PIPELINE:
		((ID3D12PipelineState *)((ComputePipelineDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case VEngine::gal::ObjectType::DESCRIPTOR_SET_LAYOUT:
		break;
	case VEngine::gal::ObjectType::SAMPLER:
		break;
	case VEngine::gal::ObjectType::DESCRIPTOR_SET_POOL:
		break;
	case VEngine::gal::ObjectType::DESCRIPTOR_SET:
		break;
	case VEngine::gal::ObjectType::COMMAND_LIST_POOL:
		break;
	case VEngine::gal::ObjectType::SWAPCHAIN:
		break;
	default:
		break;
	}
}

VEngine::gal::Queue *VEngine::gal::GraphicsDeviceDx12::getGraphicsQueue()
{
	return &m_graphicsQueue;
}

VEngine::gal::Queue *VEngine::gal::GraphicsDeviceDx12::getComputeQueue()
{
	return &m_computeQueue;
}

VEngine::gal::Queue *VEngine::gal::GraphicsDeviceDx12::getTransferQueue()
{
	return &m_transferQueue;
}

void VEngine::gal::GraphicsDeviceDx12::getQueryPoolResults(QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void *data, uint64_t stride, QueryResultFlags flags)
{
	// TODO
	// no implementation
}

float VEngine::gal::GraphicsDeviceDx12::getTimestampPeriod() const
{
	// TODO: move this to Queue
	return 1.0f;
}

uint64_t VEngine::gal::GraphicsDeviceDx12::getMinUniformBufferOffsetAlignment() const
{
	return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
}

uint64_t VEngine::gal::GraphicsDeviceDx12::getMinStorageBufferOffsetAlignment() const
{
	return D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
}

float VEngine::gal::GraphicsDeviceDx12::getMaxSamplerAnisotropy() const
{
	// based on https://docs.microsoft.com/en-us/windows/win32/direct3d12/hardware-feature-levels
	return 16.0f;
}
