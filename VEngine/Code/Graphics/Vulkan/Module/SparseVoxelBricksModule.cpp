#include "SparseVoxelBricksModule.h"
#include "Graphics/Vulkan/Pass/InitBrickPoolPass.h"
#include "Graphics/Vulkan/Pass/ClearBricksPass.h"
#include "Graphics/Vulkan/Pass/VoxelizationMarkPass.h"
#include "Graphics/Vulkan/Pass/VoxelizationAllocatePass.h"
#include "Graphics/Vulkan/Pass/VoxelizationFillPass.h"
#include "Graphics/Vulkan/Pass/ScreenSpaceVoxelizationPass.h"
#include "Graphics/Vulkan/Pass/BrickDebugPass.h"
#include "Graphics/Vulkan/Pass/ReadBackCopyPass.h"
#include <glm/vec3.hpp>
#include "Utility/Utility.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Utility/AxisAlignedBoundingBox.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKBuffer.h"

VEngine::SparseVoxelBricksModule::SparseVoxelBricksModule()
{
	// brick pointer image
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		imageCreateInfo.format = VK_FORMAT_R32_UINT;
		imageCreateInfo.extent.width = BRICK_VOLUME_WIDTH;
		imageCreateInfo.extent.height = BRICK_VOLUME_HEIGHT;
		imageCreateInfo.extent.depth = BRICK_VOLUME_DEPTH;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_brickPointerImage, m_brickPointerImageAllocHandle) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image!", EXIT_FAILURE);
		}
	}

	uint32_t queueFamilyIndices[] =
	{
		g_context.m_queueFamilyIndices.m_graphicsFamily,
		g_context.m_queueFamilyIndices.m_computeFamily,
		g_context.m_queueFamilyIndices.m_transferFamily
	};

	// free bricks list
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = sizeof(uint32_t) * (BRICK_CACHE_WIDTH * BRICK_CACHE_HEIGHT * BRICK_CACHE_DEPTH + 1);
		bufferCreateInfo.size = bufferCreateInfo.size < 32 ? 32 : bufferCreateInfo.size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (g_context.m_allocator.createBuffer(allocCreateInfo, bufferCreateInfo, m_freeBricksBuffer, m_freeBricksBufferAllocHandle) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create buffer!", EXIT_FAILURE);
		}
	}

	// bin vis bricks
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		imageCreateInfo.format = VK_FORMAT_R32_UINT;
		imageCreateInfo.extent.width = BRICK_CACHE_WIDTH * BINARY_VIS_BRICK_SIZE / 4;
		imageCreateInfo.extent.height = BRICK_CACHE_HEIGHT * 2 * BINARY_VIS_BRICK_SIZE / 4;
		imageCreateInfo.extent.depth = BRICK_CACHE_DEPTH * BINARY_VIS_BRICK_SIZE / 4;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_binVisBricksImage, m_binVisBricksImageAllocHandle) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image!", EXIT_FAILURE);
		}
	}


	// color bricks
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageCreateInfo.extent.width = BRICK_CACHE_WIDTH * COLOR_BRICK_SIZE;
		imageCreateInfo.extent.height = BRICK_CACHE_HEIGHT * COLOR_BRICK_SIZE;
		imageCreateInfo.extent.depth = BRICK_CACHE_DEPTH * COLOR_BRICK_SIZE;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_colorBricksImage, m_colorBricksImageAllocHandle) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image!", EXIT_FAILURE);
		}
	}

	// brick pool readback buffers
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = sizeof(uint32_t);
		bufferCreateInfo.size = bufferCreateInfo.size < 32 ? 32 : bufferCreateInfo.size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			if (g_context.m_allocator.createBuffer(allocCreateInfo, bufferCreateInfo, m_brickPoolStatsReadBackBuffers[i], m_brickPoolStatsReadBackBufferAllocHandles[i]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create buffer!", EXIT_FAILURE);
			}
		}
	}
}

VEngine::SparseVoxelBricksModule::~SparseVoxelBricksModule()
{
	g_context.m_allocator.destroyImage(m_brickPointerImage, m_brickPointerImageAllocHandle);
	g_context.m_allocator.destroyImage(m_binVisBricksImage, m_binVisBricksImageAllocHandle);
	g_context.m_allocator.destroyImage(m_colorBricksImage, m_colorBricksImageAllocHandle);
	g_context.m_allocator.destroyBuffer(m_freeBricksBuffer, m_freeBricksBufferAllocHandle);
	

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		g_context.m_allocator.destroyBuffer(m_brickPoolStatsReadBackBuffers[i], m_brickPoolStatsReadBackBufferAllocHandles[i]);
	}
}

void VEngine::SparseVoxelBricksModule::readBackAllocatedBrickCount(uint32_t currentResourceIndex)
{
	const auto allocHandle = m_brickPoolStatsReadBackBufferAllocHandles[currentResourceIndex];
	const auto allocInfo = g_context.m_allocator.getAllocationInfo(allocHandle);
	uint32_t *data;
	g_context.m_allocator.mapMemory(allocHandle, (void **)&data);

	VkMappedMemoryRange range{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
	range.memory = allocInfo.m_memory;
	range.offset = Utility::alignDown(allocInfo.m_offset, g_context.m_properties.limits.nonCoherentAtomSize);
	range.size = Utility::alignUp(sizeof(uint32_t), g_context.m_properties.limits.nonCoherentAtomSize);

	vkInvalidateMappedMemoryRanges(g_context.m_device, 1, &range);
	m_allocatedBrickCount = *data;

	g_context.m_allocator.unmapMemory(m_brickPoolStatsReadBackBufferAllocHandles[currentResourceIndex]);
}

void VEngine::SparseVoxelBricksModule::updateResourceHandles(RenderGraph &graph)
{
	// brick ptr image
	{
		ImageDescription desc = {};
		desc.m_name = "Brick Ptr Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = BRICK_VOLUME_WIDTH;
		desc.m_height = BRICK_VOLUME_HEIGHT;
		desc.m_depth = BRICK_VOLUME_DEPTH;
		desc.m_format = VK_FORMAT_R32_UINT;
		desc.m_imageType = VK_IMAGE_TYPE_3D;

		ImageHandle imageHandle = graph.importImage(desc, m_brickPointerImage, &m_brickPointerImageQueue, &m_brickPointerImageResourceState);
		m_brickPointerImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });
	}

	// binary visibility bricks image
	{
		ImageDescription desc = {};
		desc.m_name = "Bin Vis Bricks Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = BRICK_CACHE_WIDTH * BINARY_VIS_BRICK_SIZE / 4;
		desc.m_height = BRICK_CACHE_HEIGHT * 2 * BINARY_VIS_BRICK_SIZE / 4;
		desc.m_depth = BRICK_CACHE_DEPTH * BINARY_VIS_BRICK_SIZE / 4;
		desc.m_format = VK_FORMAT_R32_UINT;
		desc.m_imageType = VK_IMAGE_TYPE_3D;

		ImageHandle imageHandle = graph.importImage(desc, m_binVisBricksImage, &m_binVisBricksImageQueue, &m_binVisBricksImageResourceState);
		m_binVisBricksImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });
	}

	// color bricks image 
	{
		ImageDescription desc = {};
		desc.m_name = "Color Bricks Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = BRICK_CACHE_WIDTH * COLOR_BRICK_SIZE;
		desc.m_height = BRICK_CACHE_HEIGHT * COLOR_BRICK_SIZE;
		desc.m_depth = BRICK_CACHE_DEPTH * COLOR_BRICK_SIZE;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		desc.m_imageType = VK_IMAGE_TYPE_3D;

		ImageHandle imageHandle = graph.importImage(desc, m_colorBricksImage, &m_colorBricksImageQueue, &m_colorBricksImageResourceState);
		m_colorBricksImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });
	}

	// free bricks buffer
	{
		BufferDescription desc = {};
		desc.m_name = "Free Bricks Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(uint32_t) * (BRICK_CACHE_WIDTH * BRICK_CACHE_HEIGHT * BRICK_CACHE_DEPTH + 1);

		BufferHandle bufferHandle = graph.importBuffer(desc, m_freeBricksBuffer, 0, &m_freeBricksBufferQueue, &m_freeBricksBufferResourceState);
		m_freeBricksBufferViewHandle = graph.createBufferView({ desc.m_name, bufferHandle, 0, desc.m_size });
	}
}

void VEngine::SparseVoxelBricksModule::addVoxelizationToGraph(RenderGraph &graph, const Data &data)
{
	auto &commonData = *data.m_passRecordContext->m_commonRenderData;
	auto &renderResources = *data.m_passRecordContext->m_renderResources;

	if (commonData.m_frame == 0)
	{
		InitBrickPoolPass::Data initBrickPoolPassData;
		initBrickPoolPassData.m_passRecordContext = data.m_passRecordContext;
		initBrickPoolPassData.m_freeBricksBufferHandle = m_freeBricksBufferViewHandle;
		initBrickPoolPassData.m_binVisBricksImageHandle = m_binVisBricksImageViewHandle;
		initBrickPoolPassData.m_colorBricksImageHandle = m_colorBricksImageViewHandle;

		InitBrickPoolPass::addToGraph(graph, initBrickPoolPassData);
	}

	const glm::vec3 camPos = commonData.m_cameraPosition;
	const glm::vec3 prevCamPos = commonData.m_prevInvViewMatrix[3];
	const glm::ivec3 cameraCoord = glm::ivec3(round(camPos / BRICK_SIZE));
	const glm::ivec3 prevCameraCoord = glm::ivec3(round(prevCamPos / BRICK_SIZE));

	bool couldRevoxelize = cameraCoord != prevCameraCoord;

	if (couldRevoxelize && data.m_enableVoxelization || data.m_forceVoxelization)
	{
		ClearBricksPass::Data clearBricksPassData;
		clearBricksPassData.m_passRecordContext = data.m_passRecordContext;
		clearBricksPassData.m_brickPointerImageHandle = m_brickPointerImageViewHandle;
		clearBricksPassData.m_binVisBricksImageHandle = m_binVisBricksImageViewHandle;
		clearBricksPassData.m_colorBricksImageHandle = m_colorBricksImageViewHandle;
		clearBricksPassData.m_freeBricksBufferHandle = m_freeBricksBufferViewHandle;

		ClearBricksPass::addToGraph(graph, clearBricksPassData);

		ImageDescription markImageDesc = {};
		markImageDesc.m_name = "Brick Mark Image";
		markImageDesc.m_concurrent = false;
		markImageDesc.m_clear = true;
		markImageDesc.m_clearValue.m_imageClearValue.color.uint32[0] = 0;
		markImageDesc.m_clearValue.m_imageClearValue.color.uint32[1] = 0;
		markImageDesc.m_clearValue.m_imageClearValue.color.uint32[2] = 0;
		markImageDesc.m_clearValue.m_imageClearValue.color.uint32[3] = 0;
		markImageDesc.m_width = BRICK_VOLUME_WIDTH;
		markImageDesc.m_height = BRICK_VOLUME_HEIGHT;
		markImageDesc.m_depth = BRICK_VOLUME_DEPTH;
		markImageDesc.m_layers = 1;
		markImageDesc.m_levels = 1;
		markImageDesc.m_samples = 1;
		markImageDesc.m_imageType = VK_IMAGE_TYPE_3D;
		markImageDesc.m_format = VK_FORMAT_R8_UINT;

		ImageViewHandle markImageHandle = graph.createImageView({ markImageDesc.m_name, graph.createImage(markImageDesc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });


		// mark
		VoxelizationMarkPass::Data voxelizeMarkPassData;
		voxelizeMarkPassData.m_passRecordContext = data.m_passRecordContext;
		voxelizeMarkPassData.m_instanceDataCount = data.m_instanceDataCount;
		voxelizeMarkPassData.m_instanceData = data.m_instanceData;
		voxelizeMarkPassData.m_subMeshInfo = data.m_subMeshInfo;
		voxelizeMarkPassData.m_transformDataBufferInfo = data.m_transformDataBufferInfo;
		voxelizeMarkPassData.m_markImageHandle = markImageHandle;

		VoxelizationMarkPass::addToGraph(graph, voxelizeMarkPassData);


		// allocate
		VoxelizationAllocatePass::Data voxelizeAllocatePassData;
		voxelizeAllocatePassData.m_passRecordContext = data.m_passRecordContext;
		voxelizeAllocatePassData.m_brickPointerImageHandle = m_brickPointerImageViewHandle;
		voxelizeAllocatePassData.m_freeBricksBufferHandle = m_freeBricksBufferViewHandle;
		voxelizeAllocatePassData.m_markImageHandle = markImageHandle;

		VoxelizationAllocatePass::addToGraph(graph, voxelizeAllocatePassData);


		// fill
		VoxelizationFillPass::Data voxelizationFillPassData;
		voxelizationFillPassData.m_passRecordContext = data.m_passRecordContext;
		voxelizationFillPassData.m_lighting = false;
		voxelizationFillPassData.m_instanceDataBufferInfo = {};// TODO: instanceDataBufferInfo is currently unused;
		voxelizationFillPassData.m_materialDataBufferInfo = { renderResources.m_materialBuffer.getBuffer(), 0, renderResources.m_materialBuffer.getSize() };
		voxelizationFillPassData.m_transformDataBufferInfo = data.m_transformDataBufferInfo;
		voxelizationFillPassData.m_subMeshInfoBufferInfo = { renderResources.m_subMeshDataInfoBuffer.getBuffer(), 0, renderResources.m_subMeshDataInfoBuffer.getSize() };
		voxelizationFillPassData.m_brickPointerImageHandle = m_brickPointerImageViewHandle;
		voxelizationFillPassData.m_binVisBricksImageHandle = m_binVisBricksImageViewHandle;
		voxelizationFillPassData.m_colorBricksImageHandle = m_colorBricksImageViewHandle;
		voxelizationFillPassData.m_instanceDataCount = data.m_instanceDataCount;
		voxelizationFillPassData.m_instanceData = data.m_instanceData;
		voxelizationFillPassData.m_subMeshInfo = data.m_subMeshInfo;
		voxelizationFillPassData.m_shadowImageViewHandle = data.m_shadowImageViewHandle;
		voxelizationFillPassData.m_lightDataBufferInfo = data.m_directionalLightDataBufferInfo;
		voxelizationFillPassData.m_shadowMatricesBufferInfo = data.m_shadowMatricesBufferInfo;
		voxelizationFillPassData.m_irradianceVolumeImageHandle = data.m_irradianceVolumeImageViewHandle;
		voxelizationFillPassData.m_irradianceVolumeDepthImageHandle = data.m_irradianceVolumeDepthImageViewHandle;

		VoxelizationFillPass::addToGraph(graph, voxelizationFillPassData);

	}

	ScreenSpaceVoxelizationPass::Data ssVoxelPassData;
	ssVoxelPassData.m_passRecordContext = data.m_passRecordContext;
	ssVoxelPassData.m_directionalLightDataBufferInfo = data.m_directionalLightDataBufferInfo;
	ssVoxelPassData.m_pointLightDataBufferInfo = data.m_pointLightDataBufferInfo;
	ssVoxelPassData.m_pointLightZBinsBufferInfo = data.m_pointLightZBinsBufferInfo;
	ssVoxelPassData.m_materialDataBufferInfo = { renderResources.m_materialBuffer.getBuffer(), 0, renderResources.m_materialBuffer.getSize() };
	ssVoxelPassData.m_pointLightBitMaskBufferHandle = data.m_pointLightBitMaskBufferViewHandle;
	ssVoxelPassData.m_depthImageHandle = data.m_depthImageViewHandle;
	ssVoxelPassData.m_uvImageHandle = data.m_uvImageViewHandle;
	ssVoxelPassData.m_ddxyLengthImageHandle = data.m_ddxyLengthImageViewHandle;
	ssVoxelPassData.m_ddxyRotMaterialIdImageHandle = data.m_ddxyRotMaterialIdImageViewHandle;
	ssVoxelPassData.m_tangentSpaceImageHandle = data.m_tangentSpaceImageViewHandle;
	ssVoxelPassData.m_deferredShadowImageViewHandle = data.m_deferredShadowsImageViewHandle;
	ssVoxelPassData.m_brickPointerImageHandle = m_brickPointerImageViewHandle;
	ssVoxelPassData.m_colorBricksImageHandle = m_colorBricksImageViewHandle;
	ssVoxelPassData.m_irradianceVolumeImageHandle = data.m_irradianceVolumeImageViewHandle;
	ssVoxelPassData.m_irradianceVolumeDepthImageHandle = data.m_irradianceVolumeDepthImageViewHandle;

	ScreenSpaceVoxelizationPass::addToGraph(graph, ssVoxelPassData);
}

void VEngine::SparseVoxelBricksModule::addDebugVisualizationToGraph(RenderGraph &graph, const DebugVisualizationData &data)
{
	BrickDebugPass::Data brickDebugPassData;
	brickDebugPassData.m_passRecordContext = data.m_passRecordContext;
	brickDebugPassData.m_brickPtrImageHandle = m_brickPointerImageViewHandle;
	brickDebugPassData.m_colorImageHandle = data.m_colorImageViewHandle;
	brickDebugPassData.m_binVisBricksImageHandle = m_binVisBricksImageViewHandle;
	brickDebugPassData.m_colorBricksImageHandle = m_colorBricksImageViewHandle;

	BrickDebugPass::addToGraph(graph, brickDebugPassData);
}

void VEngine::SparseVoxelBricksModule::addAllocatedBrickCountReadBackCopyToGraph(RenderGraph &graph, PassRecordContext *passRecordContext)
{
	ReadBackCopyPass::Data readBackCopyPassData;
	readBackCopyPassData.m_passRecordContext = passRecordContext;
	readBackCopyPassData.m_bufferCopy = { 0, 0, sizeof(uint32_t) };
	readBackCopyPassData.m_srcBuffer = m_freeBricksBufferViewHandle;
	readBackCopyPassData.m_dstBuffer = m_brickPoolStatsReadBackBuffers[passRecordContext->m_commonRenderData->m_curResIdx];

	ReadBackCopyPass::addToGraph(graph, readBackCopyPassData);
}

VEngine::ImageViewHandle VEngine::SparseVoxelBricksModule::getBrickPointerImageViewHandle()
{
	return m_brickPointerImageViewHandle;
}

VEngine::ImageViewHandle VEngine::SparseVoxelBricksModule::getBinVisImageViewHandle()
{
	return m_binVisBricksImageViewHandle;
}

VEngine::ImageViewHandle VEngine::SparseVoxelBricksModule::getColorImageViewHandle()
{
	return m_colorBricksImageViewHandle;
}

uint32_t VEngine::SparseVoxelBricksModule::getAllocatedBrickCount() const
{
	return m_allocatedBrickCount;
}
