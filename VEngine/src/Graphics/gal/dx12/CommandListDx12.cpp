#include "CommandListDx12.h"
#include <cassert>
#include <vector>
#include "UtilityDx12.h"
#include "PipelineDx12.h"
#include "ResourceDx12.h"
#include "QueryPoolDx12.h"
#include "./../Initializers.h"
#include "Utility/TLSFAllocator.h"
#include "Utility/Utility.h"
#define USE_PIX
#include <WinPixEventRuntime/pix3.h>


VEngine::gal::CommandListDx12::CommandListDx12(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE cmdListType, const CommandListRecordContextDx12 *recordContext)
	:m_commandList(),
	m_commandAllocator(),
	m_recordContext(recordContext),
	m_currentGraphicsPipeline()
{
	UtilityDx12::checkResult(device->CreateCommandAllocator(cmdListType, __uuidof(ID3D12CommandAllocator), (void **)&m_commandAllocator), "Failed to create command list allocator!");
	UtilityDx12::checkResult(device->CreateCommandList(0, cmdListType, m_commandAllocator, nullptr, __uuidof(ID3D12GraphicsCommandList5), (void **)&m_commandList), "Failed to create command list!");
	m_commandList->Close();
}

VEngine::gal::CommandListDx12::~CommandListDx12()
{
	m_commandList->Release();
	m_commandAllocator->Release();
}

void *VEngine::gal::CommandListDx12::getNativeHandle() const
{
	return m_commandList;
}

void VEngine::gal::CommandListDx12::begin()
{
	m_commandList->Reset(m_commandAllocator, nullptr);
	ID3D12DescriptorHeap *heaps[] = { m_recordContext->m_gpuDescriptorHeap, m_recordContext->m_gpuSamplerDescriptorHeap };
	m_commandList->SetDescriptorHeaps(2, heaps);
}

void VEngine::gal::CommandListDx12::end()
{
	m_commandList->Close();
}

void VEngine::gal::CommandListDx12::bindPipeline(const GraphicsPipeline *pipeline)
{
	const GraphicsPipelineDx12 *pipelineDx = dynamic_cast<const GraphicsPipelineDx12 *>(pipeline);
	assert(pipelineDx);

	m_currentGraphicsPipeline = pipeline;

	m_commandList->SetPipelineState((ID3D12PipelineState *)pipelineDx->getNativeHandle());
	m_commandList->SetGraphicsRootSignature(pipelineDx->getRootSignature());

	pipelineDx->initializeState(m_commandList);
}

void VEngine::gal::CommandListDx12::bindPipeline(const ComputePipeline *pipeline)
{
	const ComputePipelineDx12 *pipelineDx = dynamic_cast<const ComputePipelineDx12 *>(pipeline);
	assert(pipelineDx);

	m_commandList->SetPipelineState((ID3D12PipelineState *)pipelineDx->getNativeHandle());
	m_commandList->SetComputeRootSignature(pipelineDx->getRootSignature());
}

void VEngine::gal::CommandListDx12::setViewport(uint32_t firstViewport, uint32_t viewportCount, const Viewport *viewports)
{
	assert(viewportCount <= ViewportState::MAX_VIEWPORTS);

	m_commandList->RSSetViewports(viewportCount, reinterpret_cast<const D3D12_VIEWPORT *>(viewports));
}

void VEngine::gal::CommandListDx12::setScissor(uint32_t firstScissor, uint32_t scissorCount, const Rect *scissors)
{
	assert(scissorCount <= ViewportState::MAX_VIEWPORTS);

	D3D12_RECT rects[ViewportState::MAX_VIEWPORTS];
	for (size_t i = 0; i < scissorCount; ++i)
	{
		auto &rect = rects[i];
		rect = {};
		rect.left = scissors[i].m_offset.m_x;
		rect.top = scissors[i].m_offset.m_y;
		rect.right = rect.left + scissors[i].m_extent.m_width;
		rect.bottom = rect.top + scissors[i].m_extent.m_height;
	}

	m_commandList->RSSetScissorRects(scissorCount, rects);
}

void VEngine::gal::CommandListDx12::setLineWidth(float lineWidth)
{
	// no implementation
}

void VEngine::gal::CommandListDx12::setDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
	// no implementation
}

void VEngine::gal::CommandListDx12::setBlendConstants(const float blendConstants[4])
{
	m_commandList->OMSetBlendFactor(blendConstants);
}

void VEngine::gal::CommandListDx12::setDepthBounds(float minDepthBounds, float maxDepthBounds)
{
	m_commandList->OMSetDepthBounds(minDepthBounds, maxDepthBounds);
}

void VEngine::gal::CommandListDx12::setStencilCompareMask(StencilFaceFlags faceMask, uint32_t compareMask)
{
	// no implementation
}

void VEngine::gal::CommandListDx12::setStencilWriteMask(StencilFaceFlags faceMask, uint32_t writeMask)
{
	// no implementation
}

void VEngine::gal::CommandListDx12::setStencilReference(StencilFaceFlags faceMask, uint32_t reference)
{
	m_commandList->OMSetStencilRef(reference);
}

void VEngine::gal::CommandListDx12::bindDescriptorSets(const GraphicsPipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet *const *sets)
{
	const GraphicsPipelineDx12 *pipelineDx = dynamic_cast<const GraphicsPipelineDx12 *>(pipeline);
	assert(pipelineDx);

	const uint32_t baseOffset = firstSet + pipelineDx->getDescriptorTableOffset();
	for (uint32_t i = 0; i < count; ++i)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE baseHandle = *(D3D12_GPU_DESCRIPTOR_HANDLE *)sets[i]->getNativeHandle();
		m_commandList->SetGraphicsRootDescriptorTable(baseOffset + i, baseHandle);
	}
}

void VEngine::gal::CommandListDx12::bindDescriptorSets(const ComputePipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet *const *sets)
{
	const ComputePipelineDx12 *pipelineDx = dynamic_cast<const ComputePipelineDx12 *>(pipeline);
	assert(pipelineDx);

	const uint32_t baseOffset = firstSet + pipelineDx->getDescriptorTableOffset();
	for (uint32_t i = 0; i < count; ++i)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE baseHandle = *(D3D12_GPU_DESCRIPTOR_HANDLE *)sets[i]->getNativeHandle();
		m_commandList->SetComputeRootDescriptorTable(baseOffset + i, baseHandle);
	}
}

void VEngine::gal::CommandListDx12::bindIndexBuffer(const Buffer *buffer, uint64_t offset, IndexType indexType)
{
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	indexBufferView.BufferLocation = ((ID3D12Resource *)buffer->getNativeHandle())->GetGPUVirtualAddress() + offset;
	indexBufferView.SizeInBytes = static_cast<UINT>(buffer->getDescription().m_size);
	indexBufferView.Format = indexType == IndexType::UINT32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

	m_commandList->IASetIndexBuffer(&indexBufferView);
}

void VEngine::gal::CommandListDx12::bindVertexBuffers(uint32_t firstBinding, uint32_t count, const Buffer *const *buffers, uint64_t *offsets)
{
	assert(firstBinding + count <= 32);

	const GraphicsPipelineDx12 *pipelineDx = dynamic_cast<const GraphicsPipelineDx12 *>(m_currentGraphicsPipeline);
	assert(pipelineDx);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[32];

	for (uint32_t i = 0; i < count; ++i)
	{
		auto &view = vertexBufferViews[i];
		view = {};
		view.BufferLocation = ((ID3D12Resource *)buffers[i]->getNativeHandle())->GetGPUVirtualAddress() + offsets[i];
		view.SizeInBytes = static_cast<UINT>(buffers[i]->getDescription().m_size);
		view.StrideInBytes = pipelineDx->getVertexBufferStride(firstBinding + i);
	}

	m_commandList->IASetVertexBuffers(firstBinding, count, vertexBufferViews);
}

void VEngine::gal::CommandListDx12::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	m_commandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void VEngine::gal::CommandListDx12::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	m_commandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VEngine::gal::CommandListDx12::drawIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
{
	m_commandList->ExecuteIndirect(m_recordContext->m_drawIndirectSignature, drawCount, (ID3D12Resource *)buffer->getNativeHandle(), offset, nullptr, 0);
}

void VEngine::gal::CommandListDx12::drawIndexedIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
{
	m_commandList->ExecuteIndirect(m_recordContext->m_drawIndexedIndirectSignature, drawCount, (ID3D12Resource *)buffer->getNativeHandle(), offset, nullptr, 0);
}

void VEngine::gal::CommandListDx12::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	m_commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void VEngine::gal::CommandListDx12::dispatchIndirect(const Buffer *buffer, uint64_t offset)
{
	m_commandList->ExecuteIndirect(m_recordContext->m_dispatchIndirectSignature, 1, (ID3D12Resource *)buffer->getNativeHandle(), offset, nullptr, 0);
}

void VEngine::gal::CommandListDx12::copyBuffer(const Buffer *srcBuffer, const Buffer *dstBuffer, uint32_t regionCount, const BufferCopy *regions)
{
	for (size_t i = 0; i < regionCount; ++i)
	{
		m_commandList->CopyBufferRegion((ID3D12Resource *)dstBuffer->getNativeHandle(), regions[i].m_dstOffset, (ID3D12Resource *)srcBuffer->getNativeHandle(), regions[i].m_srcOffset, regions[i].m_size);
	}
}

void VEngine::gal::CommandListDx12::copyImage(const Image *srcImage, const Image *dstImage, uint32_t regionCount, const ImageCopy *regions)
{
	for (size_t i = 0; i < regionCount; ++i)
	{
		D3D12_TEXTURE_COPY_LOCATION dstCpyLoc{ (ID3D12Resource *)dstImage->getNativeHandle(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
		dstCpyLoc.SubresourceIndex = regions[i].m_dstMipLevel + (regions[i].m_dstBaseLayer * dstImage->getDescription().m_levels);

		D3D12_TEXTURE_COPY_LOCATION srcCpyLoc{ (ID3D12Resource *)srcImage->getNativeHandle(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
		srcCpyLoc.SubresourceIndex = regions[i].m_srcMipLevel + (regions[i].m_srcBaseLayer * srcImage->getDescription().m_levels);

		D3D12_BOX srcBox{};
		srcBox.left = regions[i].m_srcOffset.m_x;
		srcBox.top = regions[i].m_srcOffset.m_y;
		srcBox.front = regions[i].m_srcOffset.m_z;
		srcBox.right = srcBox.left + regions[i].m_extent.m_width;
		srcBox.bottom = srcBox.top + regions[i].m_extent.m_height;
		srcBox.back = srcBox.front + regions[i].m_extent.m_depth;

		m_commandList->CopyTextureRegion(&dstCpyLoc, regions[i].m_dstOffset.m_x, regions[i].m_dstOffset.m_y, regions[i].m_dstOffset.m_z, &srcCpyLoc, &srcBox);
	}
}

void VEngine::gal::CommandListDx12::copyBufferToImage(const Buffer *srcBuffer, const Image *dstImage, uint32_t regionCount, const BufferImageCopy *regions)
{
	Format format = dstImage->getDescription().m_format;
	DXGI_FORMAT formatDx = UtilityDx12::translate(format);
	UINT texelSize = UtilityDx12::formatByteSize(format);
	bool compressedFormat = Initializers::getFormatInfo(format).m_compressed;
	UINT texelsPerBlock = compressedFormat ? 4 : 1;

	for (size_t i = 0; i < regionCount; ++i)
	{
		D3D12_TEXTURE_COPY_LOCATION dstCpyLoc{ (ID3D12Resource *)dstImage->getNativeHandle(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
		dstCpyLoc.SubresourceIndex = regions[i].m_imageMipLevel + (regions[i].m_imageBaseLayer * dstImage->getDescription().m_levels);

		D3D12_SUBRESOURCE_FOOTPRINT srcFootprint{};
		srcFootprint.Format = formatDx;
		srcFootprint.Width = compressedFormat ? max(regions[i].m_extent.m_width, 4) : regions[i].m_extent.m_width;
		srcFootprint.Height = compressedFormat ? max(regions[i].m_extent.m_height, 4) : regions[i].m_extent.m_height;
		srcFootprint.Depth = regions[i].m_extent.m_depth;
		srcFootprint.RowPitch = regions[i].m_bufferRowLength / texelsPerBlock * texelSize;

		D3D12_TEXTURE_COPY_LOCATION srcCpyLoc{ (ID3D12Resource *)srcBuffer->getNativeHandle(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT };
		srcCpyLoc.PlacedFootprint = { regions[i].m_bufferOffset, srcFootprint };

		m_commandList->CopyTextureRegion(&dstCpyLoc, regions[i].m_offset.m_x, regions[i].m_offset.m_y, regions[i].m_offset.m_z, &srcCpyLoc, nullptr);
	}
}

void VEngine::gal::CommandListDx12::copyImageToBuffer(const Image *srcImage, const Buffer *dstBuffer, uint32_t regionCount, const BufferImageCopy *regions)
{
	Format format = srcImage->getDescription().m_format;
	DXGI_FORMAT formatDx = UtilityDx12::translate(format);
	UINT texelSize = UtilityDx12::formatByteSize(format);
	bool compressedFormat = Initializers::getFormatInfo(format).m_compressed;
	UINT texelsPerBlock = compressedFormat ? 4 : 1;

	for (size_t i = 0; i < regionCount; ++i)
	{
		D3D12_SUBRESOURCE_FOOTPRINT dstFootprint{};
		dstFootprint.Format = formatDx;
		dstFootprint.Width = regions[i].m_extent.m_width;
		dstFootprint.Height = regions[i].m_extent.m_height;
		dstFootprint.Depth = regions[i].m_extent.m_depth;
		dstFootprint.RowPitch = regions[i].m_bufferRowLength / texelsPerBlock * texelSize;

		D3D12_TEXTURE_COPY_LOCATION dstCpyLoc{ (ID3D12Resource *)dstBuffer->getNativeHandle(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT };
		dstCpyLoc.PlacedFootprint = { regions[i].m_bufferOffset, dstFootprint };

		D3D12_TEXTURE_COPY_LOCATION srcCpyLoc{ (ID3D12Resource *)srcImage->getNativeHandle(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
		srcCpyLoc.SubresourceIndex = regions[i].m_imageMipLevel + (regions[i].m_imageBaseLayer * srcImage->getDescription().m_levels);

		m_commandList->CopyTextureRegion(&dstCpyLoc, regions[i].m_offset.m_x, regions[i].m_offset.m_y, regions[i].m_offset.m_z, &srcCpyLoc, nullptr);
	}
}

void VEngine::gal::CommandListDx12::updateBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t dataSize, const void *data)
{
	// dataSize needs to be evenly divisible by 4
	assert((dataSize & 0x3) == 0);
	// dstOffset needs to be evenly divisible by 4
	assert((dstOffset & 0x3) == 0);

	std::vector<D3D12_WRITEBUFFERIMMEDIATE_PARAMETER> params;
	params.reserve(dataSize / 4);

	D3D12_GPU_VIRTUAL_ADDRESS baseAddress = ((ID3D12Resource *)dstBuffer->getNativeHandle())->GetGPUVirtualAddress() + dstOffset;

	for (size_t i = 0; i < dataSize / 4; ++i)
	{
		params.push_back({ baseAddress + i * 4, ((uint32_t *)data)[i] });
	}

	m_commandList->WriteBufferImmediate(static_cast<UINT>(params.size()), params.data(), nullptr);
}

void VEngine::gal::CommandListDx12::fillBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t size, uint32_t data)
{
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
	ID3D12Resource *resource = (ID3D12Resource *)dstBuffer->getNativeHandle();

	// create descriptors for clearing
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
		viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		viewDesc.Buffer.FirstElement = dstOffset / 4;
		viewDesc.Buffer.NumElements = static_cast<UINT>(size / 4);
		viewDesc.Buffer.StructureByteStride = 0;
		viewDesc.Buffer.CounterOffsetInBytes = 0;
		viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

		createClearDescriptors(viewDesc, resource, gpuDescriptorHandle, cpuDescriptorHandle);
	}

	UINT values[4] = { data, data, data, data };

	m_commandList->ClearUnorderedAccessViewUint(gpuDescriptorHandle, cpuDescriptorHandle, resource, values, 0, nullptr);
}

void VEngine::gal::CommandListDx12::clearColorImage(const Image *image, const ClearColorValue *color, uint32_t rangeCount, const ImageSubresourceRange *ranges)
{
	for (size_t i = 0; i < rangeCount; ++i)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
		ID3D12Resource *resource = (ID3D12Resource *)image->getNativeHandle();

		// create descriptors for clearing
		{
			const auto &imageDesc = image->getDescription();

			D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
			viewDesc.Format = UtilityDx12::translate(imageDesc.m_format);

			switch (imageDesc.m_imageType)
			{
			case ImageType::_1D:
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
				viewDesc.Texture1D.MipSlice = ranges[i].m_baseMipLevel;
				if (ranges[i].m_layerCount > 1)
				{
					viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
					viewDesc.Texture1DArray.MipSlice = ranges[i].m_baseMipLevel;
					viewDesc.Texture1DArray.FirstArraySlice = ranges[i].m_baseArrayLayer;
					viewDesc.Texture1DArray.ArraySize = ranges[i].m_layerCount;
				}

				break;
			case ImageType::_2D:
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				viewDesc.Texture2D.MipSlice = ranges[i].m_baseMipLevel;
				viewDesc.Texture2D.PlaneSlice = 0;
				if (ranges[i].m_layerCount > 1)
				{
					viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					viewDesc.Texture2DArray.MipSlice = ranges[i].m_baseMipLevel;
					viewDesc.Texture2DArray.FirstArraySlice = ranges[i].m_baseArrayLayer;
					viewDesc.Texture2DArray.ArraySize = ranges[i].m_layerCount;
					viewDesc.Texture2DArray.PlaneSlice = 0;
				}
				break;
			case ImageType::_3D:
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
				viewDesc.Texture3D.MipSlice = ranges[i].m_baseMipLevel;
				viewDesc.Texture3D.FirstWSlice = 0;
				viewDesc.Texture3D.WSize = imageDesc.m_depth;
				break;
			default:
				assert(false);
				break;
			}

			createClearDescriptors(viewDesc, resource, gpuDescriptorHandle, cpuDescriptorHandle);
		}

		if (Initializers::getFormatInfo(image->getDescription().m_format).m_float)
		{
			m_commandList->ClearUnorderedAccessViewFloat(gpuDescriptorHandle, cpuDescriptorHandle, resource, color->m_float32, 0, nullptr);
		}
		else
		{
			m_commandList->ClearUnorderedAccessViewUint(gpuDescriptorHandle, cpuDescriptorHandle, resource, color->m_uint32, 0, nullptr);
		}
	}
}

void VEngine::gal::CommandListDx12::clearDepthStencilImage(const Image *image, const ClearDepthStencilValue *depthStencil, uint32_t rangeCount, const ImageSubresourceRange *ranges)
{
	for (size_t i = 0; i < rangeCount; ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
		ID3D12Resource *resource = (ID3D12Resource *)image->getNativeHandle();

		// create descriptor for clearing
		{
			const auto &imageDesc = image->getDescription();

			D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
			viewDesc.Format = UtilityDx12::translate(imageDesc.m_format);
			viewDesc.Flags = D3D12_DSV_FLAG_NONE;

			switch (imageDesc.m_imageType)
			{
			case ImageType::_1D:
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
				viewDesc.Texture1D.MipSlice = ranges[i].m_baseMipLevel;
				if (ranges[i].m_layerCount > 1)
				{
					viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
					viewDesc.Texture1DArray.MipSlice = ranges[i].m_baseMipLevel;
					viewDesc.Texture1DArray.FirstArraySlice = ranges[i].m_baseArrayLayer;
					viewDesc.Texture1DArray.ArraySize = ranges[i].m_layerCount;
				}
				break;
			case ImageType::_2D:
				if (imageDesc.m_samples == SampleCount::_1)
				{
					viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					viewDesc.Texture2D.MipSlice = ranges[i].m_baseMipLevel;

					if (ranges[i].m_layerCount > 1)
					{
						viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
						viewDesc.Texture2DArray.MipSlice = ranges[i].m_baseMipLevel;
						viewDesc.Texture2DArray.FirstArraySlice = ranges[i].m_baseArrayLayer;
						viewDesc.Texture2DArray.ArraySize = ranges[i].m_layerCount;
					}
				}
				else
				{
					viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
					if (ranges[i].m_layerCount > 1)
					{
						viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
						viewDesc.Texture2DMSArray.FirstArraySlice = ranges[i].m_baseArrayLayer;
						viewDesc.Texture2DMSArray.ArraySize = ranges[i].m_layerCount;
					}
				}
				break;
			default:
				assert(false);
				break;
			}

			// allocate cpu descriptor
			{
				void *allocHandle;
				uint32_t offset = 0;
				bool allocSucceeded = m_recordContext->m_dsvDescriptorAllocator->alloc(1, 1, offset, allocHandle);

				if (!allocSucceeded)
				{
					Utility::fatalExit("Failed to allocate CPU descriptor for DSV!", EXIT_FAILURE);
				}

				cpuDescriptorHandle = { m_recordContext->m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + m_recordContext->m_descriptorIncrementSizes[3] * offset };
				m_recordContext->m_device->CreateDepthStencilView(resource, &viewDesc, cpuDescriptorHandle);

				m_cpuDescriptorHandleAllocs.push_back(allocHandle);
			}
		}

		D3D12_CLEAR_FLAGS clearFlags = {};
		Format format = image->getDescription().m_format;
		clearFlags |= Initializers::isDepthFormat(format) ? D3D12_CLEAR_FLAG_DEPTH : clearFlags;
		clearFlags |= Initializers::isStencilFormat(format) ? D3D12_CLEAR_FLAG_STENCIL : clearFlags;

		m_commandList->ClearDepthStencilView(cpuDescriptorHandle, clearFlags, depthStencil->m_depth, depthStencil->m_stencil, 0, nullptr);
	}
}

void VEngine::gal::CommandListDx12::barrier(uint32_t count, const Barrier *barriers)
{
	auto getResourceStateDx12 = [](ResourceState state, PipelineStageFlags stageFlags, Format format) -> D3D12_RESOURCE_STATES
	{
		// determine if resource is used in pixel shader or non-pixel shader
		D3D12_RESOURCE_STATES shaderResourceState = D3D12_RESOURCE_STATE_COMMON;
		if ((stageFlags & PipelineStageFlagBits::FRAGMENT_SHADER_BIT) != 0)
		{
			shaderResourceState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
		if ((stageFlags
			& (PipelineStageFlagBits::VERTEX_SHADER_BIT
				| PipelineStageFlagBits::TESSELLATION_CONTROL_SHADER_BIT
				| PipelineStageFlagBits::TESSELLATION_EVALUATION_SHADER_BIT
				| PipelineStageFlagBits::GEOMETRY_SHADER_BIT
				| PipelineStageFlagBits::COMPUTE_SHADER_BIT)) != 0)
		{
			shaderResourceState |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		if (shaderResourceState == D3D12_RESOURCE_STATE_COMMON)
		{
			shaderResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		switch (state)
		{
		case ResourceState::UNDEFINED:
			return D3D12_RESOURCE_STATE_COMMON;

		case ResourceState::READ_IMAGE_HOST:
		case ResourceState::READ_BUFFER_HOST:
			return D3D12_RESOURCE_STATE_COMMON;

		case ResourceState::READ_DEPTH_STENCIL:
			return D3D12_RESOURCE_STATE_DEPTH_READ;

		case ResourceState::READ_DEPTH_STENCIL_SHADER:
			return shaderResourceState | D3D12_RESOURCE_STATE_DEPTH_READ;

		case ResourceState::READ_TEXTURE:
		case ResourceState::READ_BUFFER:
			return shaderResourceState;

		case ResourceState::READ_RW_TEXTURE:
		case ResourceState::READ_RW_BUFFER:
			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		case ResourceState::READ_CONSTANT_BUFFER:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		case ResourceState::READ_VERTEX_BUFFER:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		case ResourceState::READ_INDEX_BUFFER:
			return D3D12_RESOURCE_STATE_INDEX_BUFFER;

		case ResourceState::READ_INDIRECT_BUFFER:
			return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

		case ResourceState::READ_BUFFER_TRANSFER:
		case ResourceState::READ_IMAGE_TRANSFER:
			return D3D12_RESOURCE_STATE_COPY_SOURCE;

		case ResourceState::READ_WRITE_IMAGE_HOST:
		case ResourceState::READ_WRITE_BUFFER_HOST:
			return D3D12_RESOURCE_STATE_COMMON;

		case ResourceState::READ_WRITE_RW_TEXTURE:
		case ResourceState::READ_WRITE_RW_BUFFER:
			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		case ResourceState::READ_WRITE_DEPTH_STENCIL:
			return D3D12_RESOURCE_STATE_DEPTH_WRITE;

		case ResourceState::WRITE_IMAGE_HOST:
		case ResourceState::WRITE_BUFFER_HOST:
			return D3D12_RESOURCE_STATE_COMMON;

		case ResourceState::WRITE_ATTACHMENT:
			return D3D12_RESOURCE_STATE_RENDER_TARGET;

		case ResourceState::WRITE_RW_IMAGE:
		case ResourceState::WRITE_RW_BUFFER:
			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		case ResourceState::WRITE_BUFFER_TRANSFER:
		case ResourceState::WRITE_IMAGE_TRANSFER:
			return D3D12_RESOURCE_STATE_COPY_DEST;

		case ResourceState::CLEAR_BUFFER:
			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		case ResourceState::CLEAR_IMAGE:
		{
			bool isDepth = Initializers::isDepthFormat(format);
			return isDepth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		case ResourceState::PRESENT_IMAGE:
			return D3D12_RESOURCE_STATE_PRESENT;

		default:
			assert(false);
			break;
		}
		return {};
	};

	auto discardResource = [](ID3D12GraphicsCommandList *cmdList, const Barrier &barrier)
	{
		// we can assume that the resource is an image
		ID3D12Resource *resouceDx = (ID3D12Resource *)barrier.m_image->getNativeHandle();
		const auto &imageDesc = barrier.m_image->getDescription();
		const uint32_t baseLayer = barrier.m_imageSubresourceRange.m_baseArrayLayer;
		const uint32_t layerCount = barrier.m_imageSubresourceRange.m_layerCount;
		const uint32_t baseLevel = barrier.m_imageSubresourceRange.m_baseMipLevel;
		const uint32_t levelCount = barrier.m_imageSubresourceRange.m_levelCount;

		// collapse individual discards for each subresource into a single discard for the whole resource if all subresources are discarded
		if (baseLayer == 0 && baseLevel == 0 && layerCount == imageDesc.m_layers && levelCount == imageDesc.m_levels)
		{
			cmdList->DiscardResource(resouceDx, nullptr);
		}
		else
		{
			for (uint32_t layer = 0; layer < layerCount; ++layer)
			{
				D3D12_DISCARD_REGION region{};
				region.FirstSubresource = baseLevel + ((layer + baseLayer) * imageDesc.m_levels);
				region.NumSubresources = levelCount;

				cmdList->DiscardResource(resouceDx, &region);
			}
		}
	};

	// barrier design:
	// 1.) barriers for transitioning resources to proper state for discard
	// 2.) discard resources
	// 3.) uav barriers + barriers for transitioning to original barriers after state
	// 4.) discard resources where the original barrier wanted to transitition to the proper discard state anyways

	std::vector<D3D12_RESOURCE_BARRIER> discardTransitionBarriers;
	std::vector<uint32_t> discardBarrierIndicesBefore;
	std::vector<D3D12_RESOURCE_BARRIER> barriersDx;
	std::vector<uint32_t> discardBarrierIndicesAfter;

	barriersDx.reserve(count);

	for (size_t i = 0; i < count; ++i)
	{
		const auto &barrier = barriers[i];

		// d3d12 doesnt have queue ownership transfers, but there might still be resource transitions
		// in the barriers, so we only ignore the acquire barrier
		if (barrier.m_queueOwnershipAcquireBarrier)
		{
			continue;
		}

		ImageCreateInfo imageDesc;
		if (barrier.m_image)
		{
			imageDesc = barrier.m_image->getDescription();
		}

		auto beforeState = getResourceStateDx12(barrier.m_stateBefore, barrier.m_stagesBefore, imageDesc.m_format);
		auto afterState = getResourceStateDx12(barrier.m_stateAfter, barrier.m_stagesAfter, imageDesc.m_format);

		bool uploadHeapResource = false;
		bool readbackHeapResource = false;
		bool isCommittedImageResource = false;

		if (barrier.m_image)
		{
			const ImageDx12 *imageDx = dynamic_cast<const ImageDx12 *>(barrier.m_image);
			assert(imageDx);
			uploadHeapResource = imageDx->isUploadHeapResource();
			readbackHeapResource = imageDx->isReadbackHeapResource();
			isCommittedImageResource = imageDx->isCommittedResource();
		}
		else
		{
			const BufferDx12 *bufferDx = dynamic_cast<const BufferDx12 *>(barrier.m_buffer);
			assert(bufferDx);
			uploadHeapResource = bufferDx->isUploadHeapResource();
			readbackHeapResource = bufferDx->isReadbackHeapResource();
		}

		// resources on the upload heap must always be in D3D12_RESOURCE_STATE_GENERIC_READ
		if (uploadHeapResource)
		{
			beforeState = afterState = D3D12_RESOURCE_STATE_GENERIC_READ;
		}
		// resources on the readback heap must always be in D3D12_RESOURCE_STATE_COPY_DEST
		else if (readbackHeapResource)
		{
			beforeState = afterState = D3D12_RESOURCE_STATE_COPY_DEST;
		}

		ID3D12Resource *resouceDx = barrier.m_image ? (ID3D12Resource *)barrier.m_image->getNativeHandle() : (ID3D12Resource *)barrier.m_buffer->getNativeHandle();

		// UAV barrier
		if (beforeState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && afterState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			D3D12_RESOURCE_BARRIER uavBarrierDx{};
			uavBarrierDx.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			uavBarrierDx.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			uavBarrierDx.UAV.pResource = resouceDx;

			// check if such a barrier is already in the list
			bool duplicateBarrier = false;
			for (const auto &b : barriersDx)
			{
				if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV && b.UAV.pResource == resouceDx)
				{
					duplicateBarrier = true;
					break;
				}
			}

			if (!duplicateBarrier)
			{
				barriersDx.push_back(uavBarrierDx);
			}
		}
		// transition barrier
		else
		{
			// transitions on placed render targets / depth-stencil textures from UNDEFINED require us to discard the resource
			if (!uploadHeapResource && !readbackHeapResource && barrier.m_image && !isCommittedImageResource && barrier.m_stateBefore == ResourceState::UNDEFINED)
			{
				bool dsvImage = (imageDesc.m_usageFlags & ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
				bool rtvImage = (imageDesc.m_usageFlags & ImageUsageFlagBits::COLOR_ATTACHMENT_BIT) != 0;

				D3D12_RESOURCE_STATES newAfterState = D3D12_RESOURCE_STATE_COMMON;
				if (dsvImage && afterState != D3D12_RESOURCE_STATE_DEPTH_WRITE)
				{
					newAfterState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				}
				else if (rtvImage && afterState != D3D12_RESOURCE_STATE_RENDER_TARGET)
				{
					newAfterState = D3D12_RESOURCE_STATE_RENDER_TARGET;
				}

				// add discard transition barriers to list
				if (newAfterState != D3D12_RESOURCE_STATE_COMMON)
				{
					const uint32_t baseLayer = barrier.m_imageSubresourceRange.m_baseArrayLayer;
					const uint32_t layerCount = barrier.m_imageSubresourceRange.m_layerCount;
					const uint32_t baseLevel = barrier.m_imageSubresourceRange.m_baseMipLevel;
					const uint32_t levelCount = barrier.m_imageSubresourceRange.m_levelCount;

					D3D12_RESOURCE_BARRIER barrierDx{};
					barrierDx.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
					barrierDx.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
					barrierDx.Transition.pResource = resouceDx;
					barrierDx.Transition.StateBefore = beforeState;
					barrierDx.Transition.StateAfter = newAfterState;

					// collapse individual barriers for each subresource into a single barrier if all subresources are transitioned
					if (baseLayer == 0 && baseLevel == 0 && layerCount == imageDesc.m_layers && levelCount == imageDesc.m_levels)
					{
						barrierDx.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

						discardTransitionBarriers.push_back(barrierDx);
					}
					else
					{
						for (uint32_t layer = 0; layer < layerCount; ++layer)
						{
							for (uint32_t level = 0; level < levelCount; ++level)
							{
								const uint32_t index = (layer + baseLayer) * imageDesc.m_levels + (level + baseLevel);
								barrierDx.Transition.Subresource = index;

								discardTransitionBarriers.push_back(barrierDx);
							}
						}
					}

					// the discard is done after the first batch of transitions, because we might have this situation:
					// COMMON -> RTV -> Discard -> some other state that the barrier wants to transition to
					discardBarrierIndicesBefore.push_back((uint32_t)i);

					// update beforeState
					beforeState = newAfterState;
				}
				else
				{
					// the discard is done after the second batch of transitions, because the barrier wants to transition to the required state anyways
					discardBarrierIndicesAfter.push_back((uint32_t)i);
				}
			}

			D3D12_RESOURCE_BARRIER barrierDx{};
			barrierDx.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrierDx.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrierDx.Transition.pResource = resouceDx;
			barrierDx.Transition.StateBefore = beforeState;
			barrierDx.Transition.StateAfter = afterState;

			if (beforeState != afterState)
			{
				if (barrier.m_image)
				{
					const uint32_t baseLayer = barrier.m_imageSubresourceRange.m_baseArrayLayer;
					const uint32_t layerCount = barrier.m_imageSubresourceRange.m_layerCount;
					const uint32_t baseLevel = barrier.m_imageSubresourceRange.m_baseMipLevel;
					const uint32_t levelCount = barrier.m_imageSubresourceRange.m_levelCount;

					// collapse individual barriers for each subresource into a single barrier if all subresources are transitioned
					if (baseLayer == 0 && baseLevel == 0 && layerCount == imageDesc.m_layers && levelCount == imageDesc.m_levels)
					{
						barrierDx.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
						barriersDx.push_back(barrierDx);
					}
					else
					{
						for (uint32_t layer = 0; layer < layerCount; ++layer)
						{
							for (uint32_t level = 0; level < levelCount; ++level)
							{
								const uint32_t index = (layer + baseLayer) * imageDesc.m_levels + (level + baseLevel);
								barrierDx.Transition.Subresource = index;
								barriersDx.push_back(barrierDx);
							}
						}
					}
				}
				else
				{
					barrierDx.Transition.Subresource = 0;
					barriersDx.push_back(barrierDx);
				}
			}
		}
	}

	// 1.) transition for discards
	if (!discardTransitionBarriers.empty())
	{
		m_commandList->ResourceBarrier(static_cast<UINT>(discardTransitionBarriers.size()), discardTransitionBarriers.data());
	}

	// 2.) first batch of discards
	for (auto i : discardBarrierIndicesBefore)
	{
		discardResource(m_commandList, barriers[i]);
	}

	// 3.) barriers
	if (!barriersDx.empty())
	{
		m_commandList->ResourceBarrier(static_cast<UINT>(barriersDx.size()), barriersDx.data());
	}

	// 4.) second batch of discards
	for (auto i : discardBarrierIndicesAfter)
	{
		discardResource(m_commandList, barriers[i]);
	}
}

void VEngine::gal::CommandListDx12::beginQuery(const QueryPool *queryPool, uint32_t query)
{
	const QueryPoolDx12 *queryPoolDx = dynamic_cast<const QueryPoolDx12 *>(queryPool);
	assert(queryPoolDx);

	const auto heapType = queryPoolDx->getHeapType();
	D3D12_QUERY_TYPE queryType = D3D12_QUERY_TYPE_BINARY_OCCLUSION;

	switch (heapType)
	{
	case D3D12_QUERY_HEAP_TYPE_OCCLUSION:
		queryType = D3D12_QUERY_TYPE_BINARY_OCCLUSION;
		break;
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:
		queryType = D3D12_QUERY_TYPE_TIMESTAMP;
		break;
	case D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS:
		queryType = D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
		break;
	default:
		assert(false);
		break;
	}

	m_commandList->BeginQuery((ID3D12QueryHeap *)queryPoolDx->getNativeHandle(), queryType, query);
}

void VEngine::gal::CommandListDx12::endQuery(const QueryPool *queryPool, uint32_t query)
{
	const QueryPoolDx12 *queryPoolDx = dynamic_cast<const QueryPoolDx12 *>(queryPool);
	assert(queryPoolDx);

	m_commandList->EndQuery((ID3D12QueryHeap *)queryPoolDx->getNativeHandle(), queryPoolDx->getQueryType(), query);
}

void VEngine::gal::CommandListDx12::resetQueryPool(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount)
{
	// no implementation
}

void VEngine::gal::CommandListDx12::writeTimestamp(PipelineStageFlags pipelineStage, const QueryPool *queryPool, uint32_t query)
{
	m_commandList->EndQuery((ID3D12QueryHeap *)queryPool->getNativeHandle(), D3D12_QUERY_TYPE_TIMESTAMP, query);
}

void VEngine::gal::CommandListDx12::copyQueryPoolResults(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount, const Buffer *dstBuffer, uint64_t dstOffset)
{
	const QueryPoolDx12 *queryPoolDx = dynamic_cast<const QueryPoolDx12 *>(queryPool);
	assert(queryPoolDx);

	m_commandList->ResolveQueryData((ID3D12QueryHeap *)queryPoolDx->getNativeHandle(), queryPoolDx->getQueryType(), firstQuery, queryCount, (ID3D12Resource *)dstBuffer->getNativeHandle(), dstOffset);
}

void VEngine::gal::CommandListDx12::pushConstants(const GraphicsPipeline *pipeline, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values)
{
	// size needs to be evenly divisible by 4
	assert((size & 0x3) == 0);
	// offset needs to be evenly divisible by 4
	assert((offset & 0x3) == 0);

	// root constants (push consts), if present, always use the first slot in the root signature
	m_commandList->SetGraphicsRoot32BitConstants(0, size / 4, values, offset / 4);
}

void VEngine::gal::CommandListDx12::pushConstants(const ComputePipeline *pipeline, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values)
{
	// size needs to be evenly divisible by 4
	assert((size & 0x3) == 0);
	// offset needs to be evenly divisible by 4
	assert((offset & 0x3) == 0);

	// root constants (push consts), if present, always use the first slot in the root signature
	m_commandList->SetComputeRoot32BitConstants(0, size / 4, values, offset / 4);
}

void VEngine::gal::CommandListDx12::beginRenderPass(uint32_t colorAttachmentCount, ColorAttachmentDescription *colorAttachments, DepthStencilAttachmentDescription *depthStencilAttachment, const Rect &renderArea)
{
	auto translateLoadOp = [](AttachmentLoadOp loadOp)
	{
		switch (loadOp)
		{
		case AttachmentLoadOp::LOAD:
			return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
		case AttachmentLoadOp::CLEAR:
			return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		case AttachmentLoadOp::DONT_CARE:
			return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
		default:
			return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
		}
	};

	auto translateStoreOp = [](AttachmentStoreOp storeOp)
	{
		switch (storeOp)
		{
		case AttachmentStoreOp::STORE:
			return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		case AttachmentStoreOp::DONT_CARE:
			return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
		default:
			return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		}
	};

	D3D12_RENDER_PASS_RENDER_TARGET_DESC renderTargetDescs[8];

	for (size_t i = 0; i < colorAttachmentCount; ++i)
	{
		auto &desc = renderTargetDescs[i];
		const auto &attachment = colorAttachments[i];
		const ImageViewDx12 *imageViewDx = dynamic_cast<const ImageViewDx12 *>(attachment.m_imageView);
		assert(imageViewDx);

		desc = {};
		desc.cpuDescriptor = imageViewDx->getRTV();
		desc.BeginningAccess.Type = translateLoadOp(attachment.m_loadOp);
		desc.BeginningAccess.Clear.ClearValue.Format = UtilityDx12::translate(attachment.m_imageView->getDescription().m_format);
		desc.BeginningAccess.Clear.ClearValue.Color[0] = attachment.m_clearValue.m_float32[0];
		desc.BeginningAccess.Clear.ClearValue.Color[1] = attachment.m_clearValue.m_float32[1];
		desc.BeginningAccess.Clear.ClearValue.Color[2] = attachment.m_clearValue.m_float32[2];
		desc.BeginningAccess.Clear.ClearValue.Color[3] = attachment.m_clearValue.m_float32[3];
		desc.EndingAccess.Type = translateStoreOp(attachment.m_storeOp);
		desc.EndingAccess.Resolve = {};
	}

	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depthStencilDesc{};
	if (depthStencilAttachment)
	{
		const auto &attachment = *depthStencilAttachment;
		const ImageViewDx12 *imageViewDx = dynamic_cast<const ImageViewDx12 *>(attachment.m_imageView);
		assert(imageViewDx);

		depthStencilDesc.cpuDescriptor = attachment.m_readOnly ? imageViewDx->getDSVDepthReadOnly() : imageViewDx->getDSV();
		depthStencilDesc.DepthBeginningAccess.Type = translateLoadOp(attachment.m_loadOp);
		depthStencilDesc.DepthBeginningAccess.Clear.ClearValue.Format = UtilityDx12::translate(imageViewDx->getDescription().m_format);
		depthStencilDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil = { attachment.m_clearValue.m_depth, static_cast<UINT8>(attachment.m_clearValue.m_stencil) };
		depthStencilDesc.StencilBeginningAccess.Type = translateLoadOp(attachment.m_stencilLoadOp);
		depthStencilDesc.StencilBeginningAccess.Clear.ClearValue.Format = depthStencilDesc.DepthBeginningAccess.Clear.ClearValue.Format;
		depthStencilDesc.StencilBeginningAccess.Clear.ClearValue.DepthStencil = { attachment.m_clearValue.m_depth, static_cast<UINT8>(attachment.m_clearValue.m_stencil) };
		depthStencilDesc.DepthEndingAccess.Type = translateStoreOp(attachment.m_storeOp);
		depthStencilDesc.DepthEndingAccess.Resolve = {};
		depthStencilDesc.StencilEndingAccess.Type = translateStoreOp(attachment.m_stencilStoreOp);
		depthStencilDesc.StencilEndingAccess.Resolve = {};

		// TODO FIXME workaround for being able to use D3D12_RESOURCE_STATE_DEPTH_READ.
		// need to implement support for arbitrary read/write combinations of depth/stencil planes
		if (attachment.m_readOnly)
		{
			depthStencilDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
			depthStencilDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
		}
	}

	m_commandList->BeginRenderPass(colorAttachmentCount, renderTargetDescs, depthStencilAttachment ? &depthStencilDesc : nullptr, D3D12_RENDER_PASS_FLAG_NONE);
}

void VEngine::gal::CommandListDx12::endRenderPass()
{
	m_commandList->EndRenderPass();
}

void VEngine::gal::CommandListDx12::insertDebugLabel(const char *label)
{
	PIXSetMarker(m_commandList, PIX_COLOR(0, 255, 0), label);
}

void VEngine::gal::CommandListDx12::beginDebugLabel(const char *label)
{
	PIXBeginEvent(m_commandList, PIX_COLOR(0, 255, 0), label);
}

void VEngine::gal::CommandListDx12::endDebugLabel()
{
	PIXEndEvent(m_commandList);
}

void VEngine::gal::CommandListDx12::reset()
{
	m_commandAllocator->Reset();

	// reset() must only be called after the gpu is done with this command list.
	// this is why we can now free the allocated clear descriptors

	for (auto handle : m_cpuDescriptorHandleAllocs)
	{
		m_recordContext->m_cpuDescriptorAllocator->free(handle);
	}
	m_cpuDescriptorHandleAllocs.clear();

	for (auto handle : m_dsvDescriptorHandleAllocs)
	{
		m_recordContext->m_dsvDescriptorAllocator->free(handle);
	}
	m_dsvDescriptorHandleAllocs.clear();

	for (auto handle : m_gpuDescriptorHandleAllocs)
	{
		m_recordContext->m_gpuDescriptorAllocator->free(handle);
	}
	m_gpuDescriptorHandleAllocs.clear();
}

void VEngine::gal::CommandListDx12::createClearDescriptors(const D3D12_UNORDERED_ACCESS_VIEW_DESC &viewDesc, ID3D12Resource *resource, D3D12_GPU_DESCRIPTOR_HANDLE &gpuDescriptor, D3D12_CPU_DESCRIPTOR_HANDLE &cpuDescriptor)
{
	// allocate cpu descriptor
	{
		void *allocHandle;
		uint32_t offset = 0;
		bool allocSucceeded = m_recordContext->m_cpuDescriptorAllocator->alloc(1, 1, offset, allocHandle);

		if (!allocSucceeded)
		{
			Utility::fatalExit("Failed to allocate CPU descriptor for UAV!", EXIT_FAILURE);
		}

		cpuDescriptor = { m_recordContext->m_cpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + m_recordContext->m_descriptorIncrementSizes[0] * offset };
		m_recordContext->m_device->CreateUnorderedAccessView(resource, nullptr, &viewDesc, cpuDescriptor);

		m_cpuDescriptorHandleAllocs.push_back(allocHandle);
	}

	// allocate gpu descriptor
	{
		void *allocHandle;
		uint32_t offset = 0;
		bool allocSucceeded = m_recordContext->m_gpuDescriptorAllocator->alloc(1, 1, offset, allocHandle);

		if (!allocSucceeded)
		{
			Utility::fatalExit("Failed to allocate GPU descriptor for UAV!", EXIT_FAILURE);
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dstCpuHandle{ m_recordContext->m_gpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + m_recordContext->m_descriptorIncrementSizes[0] * offset };
		m_recordContext->m_device->CopyDescriptorsSimple(1, dstCpuHandle, cpuDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gpuDescriptor = { m_recordContext->m_gpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + m_recordContext->m_descriptorIncrementSizes[0] * offset };

		m_gpuDescriptorHandleAllocs.push_back(allocHandle);
	}
}