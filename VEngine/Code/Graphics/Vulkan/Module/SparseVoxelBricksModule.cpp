#include "SparseVoxelBricksModule.h"
#include "Graphics/Vulkan/Pass/InitBrickPoolPass.h"
#include "Graphics/Vulkan/Pass/ClearBricksPass.h"
#include "Graphics/Vulkan/Pass/VoxelizationMarkPass.h"
#include "Graphics/Vulkan/Pass/VoxelizationAllocatePass.h"
#include "Graphics/Vulkan/Pass/VoxelizationFillPass.h"
#include "Graphics/Vulkan/Pass/ScreenSpaceVoxelization2Pass.h"
#include "Graphics/Vulkan/Pass/BrickDebugPass.h"
#include "Graphics/Vulkan/Pass/ReadBackCopyPass.h"
#include <glm/vec3.hpp>
#include "Utility/Utility.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Utility/AxisAlignedBoundingBox.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKBuffer.h"

static constexpr uint32_t brickCount = 1024 * 64;

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
		bufferCreateInfo.size = sizeof(uint32_t) * (brickCount + 1);
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
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = sizeof(uint32_t) * (BINARY_VIS_BRICK_SIZE * BINARY_VIS_BRICK_SIZE * BINARY_VIS_BRICK_SIZE / 32) * brickCount;
		bufferCreateInfo.size = bufferCreateInfo.size < 32 ? 32 : bufferCreateInfo.size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (g_context.m_allocator.createBuffer(allocCreateInfo, bufferCreateInfo, m_binVisBricksBuffer, m_binVisBricksBufferAllocHandle) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create buffer!", EXIT_FAILURE);
		}
	}


	// color bricks
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = sizeof(uint32_t) * 2 * (COLOR_BRICK_SIZE * COLOR_BRICK_SIZE * COLOR_BRICK_SIZE) * brickCount;
		bufferCreateInfo.size = bufferCreateInfo.size < 32 ? 32 : bufferCreateInfo.size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (g_context.m_allocator.createBuffer(allocCreateInfo, bufferCreateInfo, m_colorBricksBuffer, m_colorBricksBufferAllocHandle) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create buffer!", EXIT_FAILURE);
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
	g_context.m_allocator.destroyBuffer(m_freeBricksBuffer, m_freeBricksBufferAllocHandle);
	g_context.m_allocator.destroyBuffer(m_binVisBricksBuffer, m_binVisBricksBufferAllocHandle);
	g_context.m_allocator.destroyBuffer(m_colorBricksBuffer, m_colorBricksBufferAllocHandle);

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

	// free bricks buffer
	{
		BufferDescription desc = {};
		desc.m_name = "Free Bricks Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(uint32_t) * (brickCount + 1);

		BufferHandle bufferHandle = graph.importBuffer(desc, m_freeBricksBuffer, 0, &m_freeBricksBufferQueue, &m_freeBricksBufferResourceState);
		m_freeBricksBufferViewHandle = graph.createBufferView({ desc.m_name, bufferHandle, 0, desc.m_size });
	}

	// binary visibility bricks buffer
	{
		BufferDescription desc = {};
		desc.m_name = "Bin Vis Bricks Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(uint32_t) * (BINARY_VIS_BRICK_SIZE * BINARY_VIS_BRICK_SIZE * BINARY_VIS_BRICK_SIZE / 32) * brickCount;

		BufferHandle bufferHandle = graph.importBuffer(desc, m_binVisBricksBuffer, 0, &m_binVisBricksBufferQueue, &m_binVisBricksBufferResourceState);
		m_binVisBricksBufferViewHandle = graph.createBufferView({ desc.m_name, bufferHandle, 0, desc.m_size, VK_FORMAT_R32_UINT });
	}

	// color bricks buffer
	{
		BufferDescription desc = {};
		desc.m_name = "Color Bricks Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(uint32_t) * 2 * (COLOR_BRICK_SIZE * COLOR_BRICK_SIZE * COLOR_BRICK_SIZE) * brickCount;

		BufferHandle bufferHandle = graph.importBuffer(desc, m_colorBricksBuffer, 0, &m_colorBricksBufferQueue, &m_colorBricksBufferResourceState);
		m_colorBricksBufferViewHandle = graph.createBufferView({ desc.m_name, bufferHandle, 0, desc.m_size, VK_FORMAT_R16G16B16A16_SFLOAT });
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
		initBrickPoolPassData.m_binVisBricksBufferHandle = m_binVisBricksBufferViewHandle;
		initBrickPoolPassData.m_colorBricksBufferHandle = m_colorBricksBufferViewHandle;

		InitBrickPoolPass::addToGraph(graph, initBrickPoolPassData);
	}

	const glm::vec3 camPos = commonData.m_cameraPosition;
	const glm::vec3 prevCamPos = commonData.m_prevInvViewMatrix[3];
	const glm::ivec3 cameraCoord = glm::ivec3(round(camPos / BRICK_SIZE));
	const glm::ivec3 prevCameraCoord = glm::ivec3(round(prevCamPos / BRICK_SIZE));

	glm::ivec3 prevAabbMin = prevCameraCoord - (glm::ivec3(BRICK_VOLUME_WIDTH, BRICK_VOLUME_HEIGHT, BRICK_VOLUME_DEPTH) / 2);
	glm::ivec3 prevAabbMax = prevAabbMin + glm::ivec3(BRICK_VOLUME_WIDTH, BRICK_VOLUME_HEIGHT, BRICK_VOLUME_DEPTH);

	glm::ivec3 curAabbMin = cameraCoord - (glm::ivec3(BRICK_VOLUME_WIDTH, BRICK_VOLUME_HEIGHT, BRICK_VOLUME_DEPTH) / 2);
	glm::ivec3 curAabbMax = curAabbMin + glm::ivec3(BRICK_VOLUME_WIDTH, BRICK_VOLUME_HEIGHT, BRICK_VOLUME_DEPTH);

	AxisAlignedBoundingBox aabbs[3];
	size_t aabbCount = 0;

	for (size_t i = 0; i < 3; ++i)
	{
		int diff = cameraCoord[i] - prevCameraCoord[i];
		if (diff != 0)
		{
			glm::ivec3 minCorner = curAabbMin;
			glm::ivec3 maxCorner = curAabbMax;
			if (diff > 0)
			{
				minCorner[i] = prevAabbMax[i];
			}
			else
			{
				maxCorner[i] = prevAabbMin[i];
			}
			aabbs[aabbCount++] = { glm::vec3(minCorner) * BRICK_SIZE, glm::vec3(maxCorner) * BRICK_SIZE };
		}
	}

	//if (aabbCount)
	{
		ClearBricksPass::Data clearBricksPassData;
		clearBricksPassData.m_passRecordContext = data.m_passRecordContext;
		clearBricksPassData.m_brickPointerImageHandle = m_brickPointerImageViewHandle;
		clearBricksPassData.m_binVisBricksBufferHandle = m_binVisBricksBufferViewHandle;
		clearBricksPassData.m_colorBricksBufferHandle = m_colorBricksBufferViewHandle;
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
		voxelizationFillPassData.m_instanceDataBufferInfo = {};// TODO: instanceDataBufferInfo is currently unused;
		voxelizationFillPassData.m_materialDataBufferInfo = { renderResources.m_materialBuffer.getBuffer(), 0, renderResources.m_materialBuffer.getSize() };
		voxelizationFillPassData.m_transformDataBufferInfo = data.m_transformDataBufferInfo;
		voxelizationFillPassData.m_subMeshInfoBufferInfo = { renderResources.m_subMeshDataInfoBuffer.getBuffer(), 0, renderResources.m_subMeshDataInfoBuffer.getSize() };
		voxelizationFillPassData.m_brickPointerImageHandle = m_brickPointerImageViewHandle;
		voxelizationFillPassData.m_binVisBricksBufferHandle = m_binVisBricksBufferViewHandle;
		voxelizationFillPassData.m_colorBricksBufferHandle = m_colorBricksBufferViewHandle;
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

	ScreenSpaceVoxelization2Pass::Data ssVoxelPass2Data;
	ssVoxelPass2Data.m_passRecordContext = data.m_passRecordContext;
	ssVoxelPass2Data.m_directionalLightDataBufferInfo = data.m_directionalLightDataBufferInfo;
	ssVoxelPass2Data.m_pointLightDataBufferInfo = data.m_pointLightDataBufferInfo;
	ssVoxelPass2Data.m_pointLightZBinsBufferInfo = data.m_pointLightZBinsBufferInfo;
	ssVoxelPass2Data.m_materialDataBufferInfo = { renderResources.m_materialBuffer.getBuffer(), 0, renderResources.m_materialBuffer.getSize() };
	ssVoxelPass2Data.m_pointLightBitMaskBufferHandle = data.m_pointLightBitMaskBufferViewHandle;
	ssVoxelPass2Data.m_depthImageHandle = data.m_depthImageViewHandle;
	ssVoxelPass2Data.m_uvImageHandle = data.m_uvImageViewHandle;
	ssVoxelPass2Data.m_ddxyLengthImageHandle = data.m_ddxyLengthImageViewHandle;
	ssVoxelPass2Data.m_ddxyRotMaterialIdImageHandle = data.m_ddxyRotMaterialIdImageViewHandle;
	ssVoxelPass2Data.m_tangentSpaceImageHandle = data.m_tangentSpaceImageViewHandle;
	ssVoxelPass2Data.m_deferredShadowImageViewHandle = data.m_deferredShadowsImageViewHandle;
	ssVoxelPass2Data.m_brickPointerImageHandle = m_brickPointerImageViewHandle;
	ssVoxelPass2Data.m_colorBricksBufferHandle = m_colorBricksBufferViewHandle;
	ssVoxelPass2Data.m_irradianceVolumeImageHandle = data.m_irradianceVolumeImageViewHandle;
	ssVoxelPass2Data.m_irradianceVolumeDepthImageHandle = data.m_irradianceVolumeDepthImageViewHandle;

	ScreenSpaceVoxelization2Pass::addToGraph(graph, ssVoxelPass2Data);
}

void VEngine::SparseVoxelBricksModule::addDebugVisualizationToGraph(RenderGraph &graph, const DebugVisualizationData &data)
{
	BrickDebugPass::Data brickDebugPassData;
	brickDebugPassData.m_passRecordContext = data.m_passRecordContext;
	brickDebugPassData.m_brickPtrImageHandle = m_brickPointerImageViewHandle;
	brickDebugPassData.m_colorImageHandle = data.m_colorImageViewHandle;
	brickDebugPassData.m_binVisBricksBufferHandle = m_binVisBricksBufferViewHandle;
	brickDebugPassData.m_colorBricksBufferHandle = m_colorBricksBufferViewHandle;

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

VEngine::BufferViewHandle VEngine::SparseVoxelBricksModule::getBinVisBufferViewHandle()
{
	return m_binVisBricksBufferViewHandle;
}

VEngine::BufferViewHandle VEngine::SparseVoxelBricksModule::getColorBufferViewHandle()
{
	return m_colorBricksBufferViewHandle;
}

uint32_t VEngine::SparseVoxelBricksModule::getAllocatedBrickCount() const
{
	return m_allocatedBrickCount;
}
