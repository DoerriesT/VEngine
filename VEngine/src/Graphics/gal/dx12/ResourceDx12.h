#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>

namespace VEngine
{
	namespace gal
	{
		class SamplerDx12 : public Sampler
		{
		public:
			explicit SamplerDx12(ID3D12Device *device, const SamplerCreateInfo &createInfo);
			SamplerDx12(SamplerDx12 &) = delete;
			SamplerDx12(SamplerDx12 &&) = delete;
			SamplerDx12 &operator= (const SamplerDx12 &) = delete;
			SamplerDx12 &operator= (const SamplerDx12 &&) = delete;
			~SamplerDx12();
			void *getNativeHandle() const override;

		private:
			D3D12_CPU_DESCRIPTOR_HANDLE m_sampler;
		};

		class ImageDx12 : public Image
		{
		public:
			explicit ImageDx12(ID3D12Resource *image, void *allocHandle, const ImageCreateInfo &createInfo);
			void *getNativeHandle() const override;
			const ImageCreateInfo &getDescription() const override;
			void *getAllocationHandle();

		private:
			ID3D12Resource *m_image;
			void *m_allocHandle;
			ImageCreateInfo m_description;
		};

		class BufferDx12 : public Buffer
		{
		public:
			explicit BufferDx12(ID3D12Resource *buffer, void *allocHandle, const BufferCreateInfo &createInfo);
			void *getNativeHandle() const override;
			const BufferCreateInfo &getDescription() const override;
			void map(void **data) override;
			void unmap() override;
			void invalidate(uint32_t count, const MemoryRange *ranges) override;
			void flush(uint32_t count, const MemoryRange *ranges) override;
			void *getAllocationHandle();

		private:
			ID3D12Resource *m_buffer;
			void *m_allocHandle;
			BufferCreateInfo m_description;
		};

		class ImageViewDx12 : public ImageView
		{
		public:
			explicit ImageViewDx12(ID3D12Device *device, const ImageViewCreateInfo &createInfo);
			ImageViewDx12(ImageViewDx12 &) = delete;
			ImageViewDx12(ImageViewDx12 &&) = delete;
			ImageViewDx12 &operator= (const ImageViewDx12 &) = delete;
			ImageViewDx12 &operator= (const ImageViewDx12 &&) = delete;
			~ImageViewDx12();
			void *getNativeHandle() const override;
			const Image *getImage() const override;
			const ImageViewCreateInfo &getDescription() const override;
			D3D12_CPU_DESCRIPTOR_HANDLE getSRV() const;
			D3D12_CPU_DESCRIPTOR_HANDLE getUAV() const;
			D3D12_CPU_DESCRIPTOR_HANDLE getRTV() const;
			D3D12_CPU_DESCRIPTOR_HANDLE getDSV() const;

		private:
			D3D12_CPU_DESCRIPTOR_HANDLE m_srv;
			D3D12_CPU_DESCRIPTOR_HANDLE m_uav;
			D3D12_CPU_DESCRIPTOR_HANDLE m_rtv;
			D3D12_CPU_DESCRIPTOR_HANDLE m_dsv;
			ImageViewCreateInfo m_description;
		};

		class BufferViewDx12 : public BufferView
		{
		public:
			explicit BufferViewDx12(ID3D12Device *device, const BufferViewCreateInfo &createInfo);
			BufferViewDx12(BufferViewDx12 &) = delete;
			BufferViewDx12(BufferViewDx12 &&) = delete;
			BufferViewDx12 &operator= (const BufferViewDx12 &) = delete;
			BufferViewDx12 &operator= (const BufferViewDx12 &&) = delete;
			~BufferViewDx12();
			void *getNativeHandle() const override;
			const Buffer *getBuffer() const override;
			const BufferViewCreateInfo &getDescription() const override;
			D3D12_CPU_DESCRIPTOR_HANDLE getCBV() const;
			D3D12_CPU_DESCRIPTOR_HANDLE getSRV() const;
			D3D12_CPU_DESCRIPTOR_HANDLE getUAV() const;

		private:
			D3D12_CPU_DESCRIPTOR_HANDLE m_cbv;
			D3D12_CPU_DESCRIPTOR_HANDLE m_srv;
			D3D12_CPU_DESCRIPTOR_HANDLE m_uav;
			BufferViewCreateInfo m_description;
		};
	}
}