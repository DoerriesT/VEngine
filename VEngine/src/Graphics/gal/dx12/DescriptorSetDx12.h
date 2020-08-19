#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>

namespace VEngine
{
	class TLSFAllocator;

	namespace gal
	{
		class DescriptorSetLayoutDx12 : public DescriptorSetLayout
		{
		public:
			explicit DescriptorSetLayoutDx12(uint32_t descriptorCount, bool samplerHeap);
			DescriptorSetLayoutDx12(DescriptorSetLayoutDx12 &) = delete;
			DescriptorSetLayoutDx12(DescriptorSetLayoutDx12 &&) = delete;
			DescriptorSetLayoutDx12 &operator= (const DescriptorSetLayoutDx12 &) = delete;
			DescriptorSetLayoutDx12 &operator= (const DescriptorSetLayoutDx12 &&) = delete;
			~DescriptorSetLayoutDx12();
			void *getNativeHandle() const override;
			uint32_t getDescriptorCount() const;
			bool needsSamplerHeap() const;

		private:
			uint32_t m_totalDescriptorCount;
			bool m_samplerHeap;
		};

		class DescriptorSetDx12 : public DescriptorSet
		{
		public:
			explicit DescriptorSetDx12(ID3D12Device *device,
				const D3D12_CPU_DESCRIPTOR_HANDLE &cpuBaseHandle,
				const D3D12_GPU_DESCRIPTOR_HANDLE &gpuBaseHandle,
				const DescriptorSetLayoutDx12 *layout,
				UINT incSize,
				bool samplerHeap);
			void *getNativeHandle() const override;
			void update(uint32_t count, const DescriptorSetUpdate2 *updates) override;
		private:
			ID3D12Device *m_device;
			D3D12_CPU_DESCRIPTOR_HANDLE m_cpuBaseHandle;
			D3D12_GPU_DESCRIPTOR_HANDLE m_gpuBaseHandle;
			const DescriptorSetLayoutDx12 *m_layout;
			UINT m_incSize;
			bool m_samplerHeap;
		};

		class DescriptorSetPoolDx12 : public DescriptorSetPool
		{
		public:
			explicit DescriptorSetPoolDx12(ID3D12Device *device,
				TLSFAllocator &gpuHeapAllocator,
				const D3D12_CPU_DESCRIPTOR_HANDLE &heapCpuBaseHandle,
				const D3D12_GPU_DESCRIPTOR_HANDLE &heapGpuBaseHandle,
				UINT incrementSize,
				const DescriptorSetLayoutDx12 *layout,
				uint32_t poolSize);
			DescriptorSetPoolDx12(DescriptorSetPoolDx12 &) = delete;
			DescriptorSetPoolDx12(DescriptorSetPoolDx12 &&) = delete;
			DescriptorSetPoolDx12 &operator= (const DescriptorSetPoolDx12 &) = delete;
			DescriptorSetPoolDx12 &operator= (const DescriptorSetPoolDx12 &&) = delete;
			~DescriptorSetPoolDx12();
			void *getNativeHandle() const override;
			void allocateDescriptorSets(uint32_t count, DescriptorSet **sets) override;
			void reset() override;
		private:
			ID3D12Device *m_device;
			TLSFAllocator &m_gpuHeapAllocator;
			void *m_allocationHandle;
			D3D12_CPU_DESCRIPTOR_HANDLE m_cpuBaseHandle;
			D3D12_GPU_DESCRIPTOR_HANDLE m_gpuBaseHandle;
			const DescriptorSetLayoutDx12 *m_layout;
			UINT m_incSize;
			uint32_t m_poolSize;
			uint32_t m_currentOffset;
			char *m_descriptorSetMemory;
		};
	}
}