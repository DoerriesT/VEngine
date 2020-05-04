#include "ResourceDx12.h"
#include "UtilityDx12.h"
#include <cassert>

VEngine::gal::SamplerDx12::SamplerDx12(ID3D12Device *device, const SamplerCreateInfo &createInfo)
{
	// TODO: allocate m_sampler

	float borderColors[][4]
	{
		{0.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 1.0f},
	};
	const size_t borderColorIdx = static_cast<size_t>(createInfo.m_borderColor);

	D3D12_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = UtilityDx12::translate(createInfo.m_magFilter, createInfo.m_minFilter, createInfo.m_mipmapMode, createInfo.m_compareEnable);
	samplerDesc.AddressU = UtilityDx12::translate(createInfo.m_addressModeU);
	samplerDesc.AddressV = UtilityDx12::translate(createInfo.m_addressModeV);
	samplerDesc.AddressW = UtilityDx12::translate(createInfo.m_addressModeW);
	samplerDesc.MipLODBias = createInfo.m_mipLodBias;
	samplerDesc.MaxAnisotropy = createInfo.m_anisotropyEnable ? static_cast<UINT>(createInfo.m_maxAnisotropy) : 1;
	samplerDesc.ComparisonFunc = createInfo.m_compareEnable ? UtilityDx12::translate(createInfo.m_compareOp) : D3D12_COMPARISON_FUNC_ALWAYS;
	samplerDesc.BorderColor[0] = borderColors[borderColorIdx][0];
	samplerDesc.BorderColor[1] = borderColors[borderColorIdx][1];
	samplerDesc.BorderColor[2] = borderColors[borderColorIdx][2];
	samplerDesc.BorderColor[3] = borderColors[borderColorIdx][3];
	samplerDesc.MinLOD = createInfo.m_minLod;
	samplerDesc.MaxLOD = createInfo.m_maxLod;

	device->CreateSampler(&samplerDesc, m_sampler);
}

VEngine::gal::SamplerDx12::~SamplerDx12()
{
	// TODO: free descriptor handle allocation
}

void *VEngine::gal::SamplerDx12::getNativeHandle() const
{
	return (void *)&m_sampler;
}

VEngine::gal::ImageDx12::ImageDx12(ID3D12Resource *image, void *allocHandle, const ImageCreateInfo &createInfo)
	:m_image(image),
	m_allocHandle(allocHandle),
	m_description(createInfo)
{
}

void *VEngine::gal::ImageDx12::getNativeHandle() const
{
	return m_image;
}

const VEngine::gal::ImageCreateInfo &VEngine::gal::ImageDx12::getDescription() const
{
	return m_description;
}

void *VEngine::gal::ImageDx12::getAllocationHandle()
{
	return m_allocHandle;
}

VEngine::gal::BufferDx12::BufferDx12(ID3D12Resource *buffer, void *allocHandle, const BufferCreateInfo &createInfo)
	:m_buffer(buffer),
	m_allocHandle(allocHandle),
	m_description(createInfo)
{
}

void *VEngine::gal::BufferDx12::getNativeHandle() const
{
	return m_buffer;
}

const VEngine::gal::BufferCreateInfo &VEngine::gal::BufferDx12::getDescription() const
{
	return m_description;
}

void VEngine::gal::BufferDx12::map(void **data)
{
	m_buffer->Map(0, nullptr, data);
}

void VEngine::gal::BufferDx12::unmap()
{
	m_buffer->Unmap(0, nullptr);
}

void VEngine::gal::BufferDx12::invalidate(uint32_t count, const MemoryRange *ranges)
{
	// no implementation
	// TODO: ensure same behavior between vk and dx12
}

void VEngine::gal::BufferDx12::flush(uint32_t count, const MemoryRange *ranges)
{
	// no implementation
	// TODO: ensure same behavior between vk and dx12
}

void *VEngine::gal::BufferDx12::getAllocationHandle()
{
	return m_allocHandle;
}

VEngine::gal::ImageViewDx12::ImageViewDx12(ID3D12Device *device, const ImageViewCreateInfo &createInfo)
	:m_srv(),
	m_uav(),
	m_rtv(),
	m_dsv(),
	m_description(createInfo)
{
	// TODO: alloc descriptor handle(s)

	const auto &imageDesc = createInfo.m_image->getDescription();
	
	ID3D12Resource *resource = (ID3D12Resource *)createInfo.m_image->getNativeHandle();

	DXGI_FORMAT format = UtilityDx12::translate(createInfo.m_format);

	// SRV
	if (imageDesc.m_usageFlags & ImageUsageFlagBits::SAMPLED_BIT)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = format;
		viewDesc.Shader4ComponentMapping = UtilityDx12::translate(createInfo.m_components);

		switch (createInfo.m_viewType)
		{
		case ImageViewType::_1D:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MostDetailedMip = createInfo.m_baseMipLevel;
			viewDesc.Texture1D.MipLevels = createInfo.m_levelCount;
			viewDesc.Texture1D.ResourceMinLODClamp = 0.0f;
			break;
		case ImageViewType::_2D:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				viewDesc.Texture2D.MostDetailedMip = createInfo.m_baseMipLevel;
				viewDesc.Texture2D.MipLevels = createInfo.m_levelCount;
				viewDesc.Texture2D.PlaneSlice = 0;
				viewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
			}
			break;
		case ImageViewType::_3D:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			viewDesc.Texture3D.MostDetailedMip = createInfo.m_baseMipLevel;
			viewDesc.Texture3D.MipLevels = createInfo.m_levelCount;
			viewDesc.Texture3D.ResourceMinLODClamp = 0.0f;
			break;
		case ImageViewType::CUBE:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			viewDesc.TextureCube.MostDetailedMip = createInfo.m_baseMipLevel;
			viewDesc.TextureCube.MipLevels = createInfo.m_levelCount;
			viewDesc.TextureCube.ResourceMinLODClamp = 0.0f;
			break;
		case ImageViewType::_1D_ARRAY:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
			viewDesc.Texture1DArray.MostDetailedMip = createInfo.m_baseMipLevel;
			viewDesc.Texture1DArray.MipLevels = createInfo.m_levelCount;
			viewDesc.Texture1DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
			viewDesc.Texture1DArray.ArraySize = createInfo.m_layerCount;
			viewDesc.Texture1DArray.ResourceMinLODClamp = 0.0f;
			break;
		case ImageViewType::_2D_ARRAY:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				viewDesc.Texture2DArray.MostDetailedMip = createInfo.m_baseMipLevel;
				viewDesc.Texture2DArray.MipLevels = createInfo.m_levelCount;
				viewDesc.Texture2DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DArray.ArraySize = createInfo.m_layerCount;
				viewDesc.Texture2DArray.PlaneSlice = 0;
				viewDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
				viewDesc.Texture2DMSArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DMSArray.ArraySize = createInfo.m_layerCount;
			}
			break;
		case ImageViewType::CUBE_ARRAY:
			assert(createInfo.m_layerCount % 6 == 0);
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
			viewDesc.TextureCubeArray.MostDetailedMip = createInfo.m_baseMipLevel;
			viewDesc.TextureCubeArray.MipLevels = createInfo.m_levelCount;
			viewDesc.TextureCubeArray.First2DArrayFace = createInfo.m_baseArrayLayer;
			viewDesc.TextureCubeArray.NumCubes = createInfo.m_layerCount / 6;
			viewDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
			break;
		default:
			assert(false);
			break;
		}

		device->CreateShaderResourceView(resource, &viewDesc, m_srv);
	}

	// UAV
	if (imageDesc.m_usageFlags & ImageUsageFlagBits::STORAGE_BIT && createInfo.m_viewType != ImageViewType::CUBE && createInfo.m_viewType != ImageViewType::CUBE_ARRAY && imageDesc.m_samples == SampleCount::_1)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
		viewDesc.Format = format;

		switch (createInfo.m_viewType)
		{
		case ImageViewType::_1D:
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MipSlice = createInfo.m_baseMipLevel;
			break;
		case ImageViewType::_2D:
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture2D.PlaneSlice = 0;
			break;
		case ImageViewType::_3D:
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			viewDesc.Texture3D.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture3D.FirstWSlice = 0;
			viewDesc.Texture3D.WSize = imageDesc.m_depth;
			break;
		case ImageViewType::_1D_ARRAY:
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			viewDesc.Texture1DArray.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture1DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
			viewDesc.Texture1DArray.ArraySize = createInfo.m_layerCount;
			break;
		case ImageViewType::_2D_ARRAY:
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			viewDesc.Texture2DArray.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture2DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
			viewDesc.Texture2DArray.ArraySize = createInfo.m_layerCount;
			viewDesc.Texture2DArray.PlaneSlice = 0;
			break;
		default:
			assert(false);
			break;
		}

		device->CreateUnorderedAccessView(resource, nullptr, &viewDesc, m_uav);
	}

	// RTV
	if (imageDesc.m_usageFlags & ImageUsageFlagBits::COLOR_ATTACHMENT_BIT && createInfo.m_viewType != ImageViewType::CUBE && createInfo.m_viewType != ImageViewType::CUBE_ARRAY)
	{
		D3D12_RENDER_TARGET_VIEW_DESC viewDesc{};
		viewDesc.Format = format;

		switch (createInfo.m_viewType)
		{
		case ImageViewType::_1D:
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MipSlice = createInfo.m_baseMipLevel;
			break;
		case ImageViewType::_2D:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				viewDesc.Texture2D.MipSlice = createInfo.m_baseMipLevel;
				viewDesc.Texture2D.PlaneSlice = 0;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
			}
			break;
		case ImageViewType::_3D:
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			viewDesc.Texture3D.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture3D.FirstWSlice = 0;
			viewDesc.Texture3D.WSize = imageDesc.m_depth;
			break;
		case ImageViewType::_1D_ARRAY:
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
			viewDesc.Texture1DArray.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture1DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
			viewDesc.Texture1DArray.ArraySize = createInfo.m_layerCount;
			break;
		case ImageViewType::_2D_ARRAY:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				viewDesc.Texture2DArray.MipSlice = createInfo.m_baseMipLevel;
				viewDesc.Texture2DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DArray.ArraySize = createInfo.m_layerCount;
				viewDesc.Texture2DArray.PlaneSlice = 0;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
				viewDesc.Texture2DMSArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DMSArray.ArraySize = createInfo.m_layerCount;
			}
			break;
		default:
			assert(false);
			break;
		}

		device->CreateRenderTargetView(resource, &viewDesc, m_rtv);
	}

	// DSV
	if (imageDesc.m_usageFlags & ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT_BIT && createInfo.m_viewType != ImageViewType::CUBE && createInfo.m_viewType != ImageViewType::CUBE_ARRAY && createInfo.m_viewType != ImageViewType::_3D)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
		viewDesc.Format = format;
		viewDesc.Flags = D3D12_DSV_FLAG_NONE;

		switch (createInfo.m_viewType)
		{
		case ImageViewType::_1D:
			viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MipSlice = createInfo.m_baseMipLevel;
			break;
		case ImageViewType::_2D:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				viewDesc.Texture2D.MipSlice = createInfo.m_baseMipLevel;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
			}
			break;
		case ImageViewType::_1D_ARRAY:
			viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
			viewDesc.Texture1DArray.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture1DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
			viewDesc.Texture1DArray.ArraySize = createInfo.m_layerCount;
			break;
		case ImageViewType::_2D_ARRAY:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				viewDesc.Texture2DArray.MipSlice = createInfo.m_baseMipLevel;
				viewDesc.Texture2DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DArray.ArraySize = createInfo.m_layerCount;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
				viewDesc.Texture2DMSArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DMSArray.ArraySize = createInfo.m_layerCount;
			}
			break;
		default:
			assert(false);
			break;
		}

		device->CreateDepthStencilView(resource, &viewDesc, m_dsv);
	}
}

VEngine::gal::ImageViewDx12::~ImageViewDx12()
{
	// TODO: free descriptor handle(s)
}

void *VEngine::gal::ImageViewDx12::getNativeHandle() const
{
	return nullptr;
}

const VEngine::gal::Image *VEngine::gal::ImageViewDx12::getImage() const
{
	return m_description.m_image;
}

const VEngine::gal::ImageViewCreateInfo &VEngine::gal::ImageViewDx12::getDescription() const
{
	return m_description;
}

D3D12_CPU_DESCRIPTOR_HANDLE VEngine::gal::ImageViewDx12::getSRV() const
{
	return m_srv;
}

D3D12_CPU_DESCRIPTOR_HANDLE VEngine::gal::ImageViewDx12::getUAV() const
{
	return m_uav;
}

D3D12_CPU_DESCRIPTOR_HANDLE VEngine::gal::ImageViewDx12::getRTV() const
{
	return m_rtv;
}

D3D12_CPU_DESCRIPTOR_HANDLE VEngine::gal::ImageViewDx12::getDSV() const
{
	return m_dsv;
}

VEngine::gal::BufferViewDx12::BufferViewDx12(ID3D12Device *device, const BufferViewCreateInfo &createInfo)
	:m_cbv(),
	m_srv(),
	m_uav(),
	m_description(createInfo)
{
	// TODO: alloc descriptor handle(s)

	const auto &bufferDesc = createInfo.m_buffer->getDescription();

	ID3D12Resource *resource = (ID3D12Resource *)createInfo.m_buffer->getNativeHandle();

	DXGI_FORMAT format = UtilityDx12::translate(createInfo.m_format);

	// CBV
	if (bufferDesc.m_usageFlags & BufferUsageFlagBits::UNIFORM_BUFFER_BIT)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
		viewDesc.BufferLocation = resource->GetGPUVirtualAddress() + createInfo.m_offset;
		viewDesc.SizeInBytes = static_cast<UINT>(createInfo.m_range);

		device->CreateConstantBufferView(&viewDesc, m_cbv);
	}

	// SRV
	if (bufferDesc.m_usageFlags & BufferUsageFlagBits::UNIFORM_TEXEL_BUFFER_BIT) // TODO
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = format;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.Buffer.FirstElement; // TODO
		viewDesc.Buffer.NumElements;
		viewDesc.Buffer.StructureByteStride;
		viewDesc.Buffer.Flags;
		
		device->CreateShaderResourceView(resource, &viewDesc, m_srv);
	}

	// UAV
	if (bufferDesc.m_usageFlags & BufferUsageFlagBits::STORAGE_BUFFER_BIT)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
		viewDesc.Format = format;
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		viewDesc.Buffer.FirstElement; // TODO
		viewDesc.Buffer.NumElements;
		viewDesc.Buffer.StructureByteStride;
		viewDesc.Buffer.CounterOffsetInBytes = 0;
		viewDesc.Buffer.Flags;

		device->CreateUnorderedAccessView(resource, nullptr, &viewDesc, m_uav);
	}
}

VEngine::gal::BufferViewDx12::~BufferViewDx12()
{
	// TODO: free descriptor handle(s)
}

void *VEngine::gal::BufferViewDx12::getNativeHandle() const
{
	return nullptr;
}

const VEngine::gal::Buffer *VEngine::gal::BufferViewDx12::getBuffer() const
{
	return m_description.m_buffer;
}

const VEngine::gal::BufferViewCreateInfo &VEngine::gal::BufferViewDx12::getDescription() const
{
	return m_description;
}

D3D12_CPU_DESCRIPTOR_HANDLE VEngine::gal::BufferViewDx12::getCBV() const
{
	return m_cbv;
}

D3D12_CPU_DESCRIPTOR_HANDLE VEngine::gal::BufferViewDx12::getSRV() const
{
	return m_srv;
}

D3D12_CPU_DESCRIPTOR_HANDLE VEngine::gal::BufferViewDx12::getUAV() const
{
	return m_uav;
}
