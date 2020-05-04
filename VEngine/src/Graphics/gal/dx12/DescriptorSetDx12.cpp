#include "DescriptorSetDx12.h"
#include "ResourceDx12.h"
#include "Utility/TLSFAllocator.h"
#include "Utility/Utility.h"

VEngine::gal::DescriptorSetDx12::DescriptorSetDx12(
	ID3D12Device *device, 
	const D3D12_CPU_DESCRIPTOR_HANDLE &cpuBaseHandle, 
	const D3D12_GPU_DESCRIPTOR_HANDLE &gpuBaseHandle, 
	const DescriptorSetLayoutDx12 *layout, 
	UINT incSize, 
	bool samplerHeap)
	:m_device(device),
	m_cpuBaseHandle(cpuBaseHandle),
	m_gpuBaseHandle(gpuBaseHandle),
	m_layout(layout),
	m_incSize(incSize),
	m_samplerHeap(samplerHeap)
{
}

void *VEngine::gal::DescriptorSetDx12::getNativeHandle() const
{
	return (void *)&m_gpuBaseHandle;
}

void VEngine::gal::DescriptorSetDx12::update(uint32_t count, const DescriptorSetUpdate2 *updates)
{
	for (size_t i = 0; i < count; ++i)
	{
		const auto &update = updates[i];

		const uint32_t baseOffset = m_layout->getOffset(update.m_dstBinding);
		
		assert((update.m_descriptorType == DescriptorType2::SAMPLER) == m_samplerHeap);

		for (size_t j = 0; j < update.m_descriptorCount; ++j)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE dstRangeStart = m_cpuBaseHandle;
			dstRangeStart.ptr += m_incSize * (m_layout->getOffset(update.m_dstBinding) + update.m_dstArrayElement + j);

			D3D12_CPU_DESCRIPTOR_HANDLE srcRangeStart{};

			bool copy = false;

			switch (update.m_descriptorType)
			{
			case DescriptorType2::SAMPLER:
			{
				copy = true;
				srcRangeStart = *(D3D12_CPU_DESCRIPTOR_HANDLE *)update.m_samplers[j]->getNativeHandle();
				break;
			}
			case DescriptorType2::TEXTURE:
			{
				copy = true;
				const ImageViewDx12 *imageViewDx = dynamic_cast<const ImageViewDx12 *>(update.m_imageViews[j]);
				assert(imageViewDx);
				srcRangeStart = imageViewDx->getSRV();
				break;
			}
			case DescriptorType2::RW_TEXTURE:
			{
				copy = true;
				const ImageViewDx12 *imageViewDx = dynamic_cast<const ImageViewDx12 *>(update.m_imageViews[j]);
				assert(imageViewDx);
				srcRangeStart = imageViewDx->getUAV();
				break;
			}
			case DescriptorType2::TYPED_BUFFER:
			{
				copy = true;
				const BufferViewDx12 *bufferViewDx = dynamic_cast<const BufferViewDx12 *>(update.m_bufferViews[j]);
				assert(bufferViewDx);
				srcRangeStart = bufferViewDx->getSRV();
				break;
			}
			case DescriptorType2::RW_TYPED_BUFFER:
			{
				copy = true;
				const BufferViewDx12 *bufferViewDx = dynamic_cast<const BufferViewDx12 *>(update.m_bufferViews[j]);
				assert(bufferViewDx);
				srcRangeStart = bufferViewDx->getUAV();
				break;
			}
			case DescriptorType2::CONSTANT_BUFFER:
			{
				D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
				viewDesc.BufferLocation = ((ID3D12Resource *)update.m_bufferInfo[j].m_buffer->getNativeHandle())->GetGPUVirtualAddress() + update.m_bufferInfo[j].m_offset;
				viewDesc.SizeInBytes = update.m_bufferInfo[j].m_range;
				m_device->CreateConstantBufferView(&viewDesc, dstRangeStart);
				break;
			}
			case DescriptorType2::BYTE_BUFFER:
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
				viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				viewDesc.Buffer.FirstElement = update.m_bufferInfo[j].m_offset / 4;
				viewDesc.Buffer.NumElements = update.m_bufferInfo[j].m_range / 4;
				viewDesc.Buffer.StructureByteStride = 0;
				viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

				m_device->CreateShaderResourceView((ID3D12Resource *)update.m_bufferInfo[j].m_buffer->getNativeHandle(), &viewDesc, dstRangeStart);
				break;
			}
			case DescriptorType2::RW_BYTE_BUFFER:
			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
				viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				viewDesc.Buffer.FirstElement = update.m_bufferInfo[j].m_offset / 4;
				viewDesc.Buffer.NumElements = update.m_bufferInfo[j].m_range / 4;
				viewDesc.Buffer.StructureByteStride = 0;
				viewDesc.Buffer.CounterOffsetInBytes = 0;
				viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

				m_device->CreateUnorderedAccessView((ID3D12Resource *)update.m_bufferInfo[j].m_buffer->getNativeHandle(), nullptr, &viewDesc, dstRangeStart);
				break;
			}
			case DescriptorType2::STRUCTURED_BUFFER:
			{
				const uint32_t elementSize = 1; // TODO

				assert(update.m_bufferInfo[j].m_offset % elementSize == 0);
				assert(update.m_bufferInfo[j].m_range % elementSize == 0);

				D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
				viewDesc.Format = DXGI_FORMAT_UNKNOWN;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				viewDesc.Buffer.FirstElement = update.m_bufferInfo[j].m_offset / elementSize;
				viewDesc.Buffer.NumElements = update.m_bufferInfo[j].m_range / elementSize;
				viewDesc.Buffer.StructureByteStride = elementSize;
				viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

				m_device->CreateShaderResourceView((ID3D12Resource *)update.m_bufferInfo[j].m_buffer->getNativeHandle(), &viewDesc, dstRangeStart);
				break;
			}
			case DescriptorType2::RW_STRUCTURED_BUFFER:
			{
				const uint32_t elementSize = 1; // TODO

				assert(update.m_bufferInfo[j].m_offset % elementSize == 0);
				assert(update.m_bufferInfo[j].m_range % elementSize == 0);

				D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
				viewDesc.Format = DXGI_FORMAT_UNKNOWN;
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				viewDesc.Buffer.FirstElement = update.m_bufferInfo[j].m_offset / elementSize;
				viewDesc.Buffer.NumElements = update.m_bufferInfo[j].m_range / elementSize;
				viewDesc.Buffer.StructureByteStride = elementSize;
				viewDesc.Buffer.CounterOffsetInBytes = 0;
				viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

				m_device->CreateUnorderedAccessView((ID3D12Resource *)update.m_bufferInfo[j].m_buffer->getNativeHandle(), nullptr, &viewDesc, dstRangeStart);
				break;
			}
			default:
				assert(false);
				break;
			}

			assert(srcRangeStart.ptr);

			// TODO: batch these calls
			if (copy)
			{
				m_device->CopyDescriptorsSimple(1, dstRangeStart, srcRangeStart, m_samplerHeap ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
	}
}

VEngine::gal::DescriptorSetLayoutDx12::DescriptorSetLayoutDx12(uint32_t descriptorCount, bool samplerHeap)
	:m_totalDescriptorCount(descriptorCount),
	m_samplerHeap(samplerHeap)
{
}

VEngine::gal::DescriptorSetLayoutDx12::~DescriptorSetLayoutDx12()
{
}

void *VEngine::gal::DescriptorSetLayoutDx12::getNativeHandle() const
{
	return nullptr;
}

uint32_t VEngine::gal::DescriptorSetLayoutDx12::getDescriptorCount() const
{
	return m_totalDescriptorCount;
}

uint32_t VEngine::gal::DescriptorSetLayoutDx12::getOffset(uint32_t binding) const
{
	assert(binding < 32);
	return m_offsetFromTableStart[binding];
}

bool VEngine::gal::DescriptorSetLayoutDx12::needsSamplerHeap() const
{
	return m_samplerHeap;
}

VEngine::gal::DescriptorSetPoolDx12::DescriptorSetPoolDx12(
	ID3D12Device *device, 
	TLSFAllocator &gpuHeapAllocator, 
	const D3D12_CPU_DESCRIPTOR_HANDLE &heapCpuBaseHandle,
	const D3D12_GPU_DESCRIPTOR_HANDLE &heapGpuBaseHandle, 
	UINT incrementSize,
	const DescriptorSetLayout *layout, 
	uint32_t poolSize)
	:m_device(device),
	m_gpuHeapAllocator(gpuHeapAllocator),
	m_cpuBaseHandle(heapCpuBaseHandle),
	m_gpuBaseHandle(heapGpuBaseHandle),
	m_layout(dynamic_cast<const DescriptorSetLayoutDx12 *>(layout)),
	m_incSize(incrementSize),
	m_poolSize(poolSize),
	m_currentOffset()
{
	assert(m_layout);

	// allocate space in global gpu descriptor heap
	uint32_t spanOffset = 0;
	bool allocSucceeded = m_gpuHeapAllocator.alloc(m_layout->getDescriptorCount() * m_poolSize, 1, spanOffset, m_allocationHandle);

	if (!allocSucceeded)
	{
		Utility::fatalExit("Failed to allocate descriptors for descriptor set pool!", EXIT_FAILURE);
	}

	// adjust descriptor base handles to start of allocated range
	m_cpuBaseHandle.ptr += m_incSize * spanOffset;
	m_gpuBaseHandle.ptr += m_incSize * spanOffset;

	// allocate memory to placement new DescriptorSetDx12
	m_descriptorSetMemory = new char[sizeof(DescriptorSetDx12) * m_poolSize];
}

VEngine::gal::DescriptorSetPoolDx12::~DescriptorSetPoolDx12()
{
	// free allocated range in descriptor heap
	m_gpuHeapAllocator.free(m_allocationHandle);
	delete[] m_descriptorSetMemory;
	m_descriptorSetMemory = nullptr;
}

void *VEngine::gal::DescriptorSetPoolDx12::getNativeHandle() const
{
	// this class doesnt map to any native D3D12 concept
	return nullptr;
}

void VEngine::gal::DescriptorSetPoolDx12::allocateDescriptorSets(uint32_t count, DescriptorSetDx12 **sets)
{
	if (m_currentOffset + count > m_poolSize)
	{
		Utility::fatalExit("Tried to allocate more descriptor sets from descriptor set pool than available!", EXIT_FAILURE);
	}

	// placement new the sets and increment the offset
	for (size_t i = 0; i < count; ++i)
	{
		sets[i] = new (m_descriptorSetMemory + sizeof(DescriptorSetDx12) * m_currentOffset) DescriptorSetDx12(m_device, m_cpuBaseHandle, m_gpuBaseHandle, m_layout, m_incSize, m_layout->needsSamplerHeap());
		++m_currentOffset;
	}
}

void VEngine::gal::DescriptorSetPoolDx12::reset()
{
	// DescriptorSetDx12 is a POD class, so we can simply reset the allocator offset of the backing memory
	// and dont need to call the destructors
	m_currentOffset = 0;
}
