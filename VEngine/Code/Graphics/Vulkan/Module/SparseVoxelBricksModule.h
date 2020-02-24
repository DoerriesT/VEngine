#pragma once
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInstanceData;
	struct SubMeshInfo;

	class SparseVoxelBricksModule
	{
	public:
		static constexpr uint32_t BRICK_VOLUME_WIDTH = 128;
		static constexpr uint32_t BRICK_VOLUME_HEIGHT = 64;
		static constexpr uint32_t BRICK_VOLUME_DEPTH = 128;
		static constexpr float BRICK_SIZE = 1.0f;
		static constexpr uint32_t BINARY_VIS_BRICK_SIZE = 16;
		static constexpr uint32_t COLOR_BRICK_SIZE = 4;
		static constexpr uint32_t BRICK_CACHE_WIDTH = 64;
		static constexpr uint32_t BRICK_CACHE_HEIGHT = 32;
		static constexpr uint32_t BRICK_CACHE_DEPTH = 32;

		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_enableVoxelization;
			bool m_forceVoxelization;
			uint32_t m_instanceDataCount;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			VkDescriptorBufferInfo m_directionalLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightZBinsBufferInfo;
			VkDescriptorBufferInfo m_shadowMatricesBufferInfo;
			ImageViewHandle m_irradianceVolumeImageViewHandle;
			ImageViewHandle m_irradianceVolumeDepthImageViewHandle;
			ImageViewHandle m_depthImageViewHandle;
			ImageViewHandle m_uvImageViewHandle;
			ImageViewHandle m_ddxyLengthImageViewHandle;
			ImageViewHandle m_ddxyRotMaterialIdImageViewHandle;
			ImageViewHandle m_tangentSpaceImageViewHandle;
			ImageViewHandle m_deferredShadowsImageViewHandle;
			ImageViewHandle m_shadowImageViewHandle;
			BufferViewHandle m_pointLightBitMaskBufferViewHandle;
		};

		struct DebugVisualizationData
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_colorImageViewHandle;
		};

		explicit SparseVoxelBricksModule();
		SparseVoxelBricksModule(const SparseVoxelBricksModule &) = delete;
		SparseVoxelBricksModule(const SparseVoxelBricksModule &&) = delete;
		SparseVoxelBricksModule &operator= (const SparseVoxelBricksModule &) = delete;
		SparseVoxelBricksModule &operator= (const SparseVoxelBricksModule &&) = delete;
		~SparseVoxelBricksModule();

		void readBackAllocatedBrickCount(uint32_t currentResourceIndex);
		void updateResourceHandles(RenderGraph &graph);
		void addVoxelizationToGraph(RenderGraph &graph, const Data &data);
		void addDebugVisualizationToGraph(RenderGraph &graph, const DebugVisualizationData &data);
		void addAllocatedBrickCountReadBackCopyToGraph(RenderGraph &graph, PassRecordContext *passRecordContext);
		ImageViewHandle getBrickPointerImageViewHandle();
		ImageViewHandle getBinVisImageViewHandle();
		ImageViewHandle getColorImageViewHandle();
		uint32_t getAllocatedBrickCount() const;

	private:
		ImageViewHandle m_brickPointerImageViewHandle;
		ImageViewHandle m_binVisBricksImageViewHandle;
		ImageViewHandle m_colorBricksImageViewHandle;
		BufferViewHandle m_freeBricksBufferViewHandle;

		VkImage m_brickPointerImage;
		VkImage m_binVisBricksImage;
		VkImage m_colorBricksImage;
		VkBuffer m_freeBricksBuffer;
		
		VkBuffer m_brickPoolStatsReadBackBuffers[RendererConsts::FRAMES_IN_FLIGHT];

		VKAllocationHandle m_brickPointerImageAllocHandle = nullptr;
		VKAllocationHandle m_binVisBricksImageAllocHandle = nullptr;
		VKAllocationHandle m_colorBricksImageAllocHandle = nullptr;
		VKAllocationHandle m_freeBricksBufferAllocHandle = nullptr;
		VKAllocationHandle m_brickPoolStatsReadBackBufferAllocHandles[RendererConsts::FRAMES_IN_FLIGHT];

		VkQueue m_brickPointerImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_brickPointerImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_binVisBricksImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_binVisBricksImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_colorBricksImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_colorBricksImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_freeBricksBufferQueue = RenderGraph::undefinedQueue;
		ResourceState m_freeBricksBufferResourceState = ResourceState::UNDEFINED;

		uint32_t m_allocatedBrickCount;
	};
}