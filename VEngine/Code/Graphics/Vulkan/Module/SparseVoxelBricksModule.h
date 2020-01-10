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

		struct Data
		{
			PassRecordContext *m_passRecordContext;
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
		BufferViewHandle getBinVisBufferViewHandle();
		BufferViewHandle getColorBufferViewHandle();
		uint32_t getAllocatedBrickCount() const;

	private:
		ImageViewHandle m_brickPointerImageViewHandle;
		BufferViewHandle m_freeBricksBufferViewHandle;
		BufferViewHandle m_binVisBricksBufferViewHandle;
		BufferViewHandle m_colorBricksBufferViewHandle;

		VkImage m_brickPointerImage;
		VkBuffer m_freeBricksBuffer;
		VkBuffer m_binVisBricksBuffer;
		VkBuffer m_colorBricksBuffer;
		VkBuffer m_brickPoolStatsReadBackBuffers[RendererConsts::FRAMES_IN_FLIGHT];

		VKAllocationHandle m_brickPointerImageAllocHandle = nullptr;
		VKAllocationHandle m_freeBricksBufferAllocHandle = nullptr;
		VKAllocationHandle m_binVisBricksBufferAllocHandle = nullptr;
		VKAllocationHandle m_colorBricksBufferAllocHandle = nullptr;
		VKAllocationHandle m_brickPoolStatsReadBackBufferAllocHandles[RendererConsts::FRAMES_IN_FLIGHT];

		VkQueue m_brickPointerImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_brickPointerImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_freeBricksBufferQueue = RenderGraph::undefinedQueue;
		ResourceState m_freeBricksBufferResourceState = ResourceState::UNDEFINED;
		VkQueue m_binVisBricksBufferQueue = RenderGraph::undefinedQueue;
		ResourceState m_binVisBricksBufferResourceState = ResourceState::UNDEFINED;
		VkQueue m_colorBricksBufferQueue = RenderGraph::undefinedQueue;
		ResourceState m_colorBricksBufferResourceState = ResourceState::UNDEFINED;

		uint32_t m_allocatedBrickCount;
	};
}