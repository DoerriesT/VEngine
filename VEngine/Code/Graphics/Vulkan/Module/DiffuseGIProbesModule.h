#pragma once
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext;

	class DiffuseGIProbesModule
	{
	public:
		static constexpr uint32_t IRRADIANCE_VOLUME_CASCADES = 3;
		static constexpr uint32_t IRRADIANCE_VOLUME_WIDTH = 32;
		static constexpr uint32_t IRRADIANCE_VOLUME_HEIGHT = 16;
		static constexpr uint32_t IRRADIANCE_VOLUME_DEPTH = 32;
		static constexpr float IRRADIANCE_VOLUME_BASE_SIZE = 1.0f;
		static constexpr uint32_t IRRADIANCE_VOLUME_QUEUE_CAPACITY = 1024;
		static constexpr uint32_t IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH = 8;
		static constexpr uint32_t IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH = 16;
		static constexpr uint32_t IRRADIANCE_VOLUME_RAY_MARCHING_RAY_COUNT = 256;

		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_depthPyramidImageViewHandle;
			ImageViewHandle m_brickPointerImageViewHandle;
			BufferViewHandle m_binVisBricksBufferViewHandle;
			BufferViewHandle m_colorBricksBufferViewHandle;
		};

		struct DebugVisualizationData
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_cascadeIndex;
			bool m_showAge;
			ImageViewHandle m_colorImageViewHandle;
			ImageViewHandle m_depthImageViewHandle;
		};

		explicit DiffuseGIProbesModule();
		DiffuseGIProbesModule(const DiffuseGIProbesModule &) = delete;
		DiffuseGIProbesModule(const DiffuseGIProbesModule &&) = delete;
		DiffuseGIProbesModule &operator= (const DiffuseGIProbesModule &) = delete;
		DiffuseGIProbesModule &operator= (const DiffuseGIProbesModule &&) = delete;
		~DiffuseGIProbesModule();

		void readBackCulledProbesCount(uint32_t currentResourceIndex);
		void updateResourceHandles(RenderGraph &graph);
		void addProbeUpdateToGraph(RenderGraph &graph, const Data &data);
		void addDebugVisualizationToGraph(RenderGraph &graph, const DebugVisualizationData &data);
		ImageViewHandle getIrradianceVolumeImageViewHandle();
		ImageViewHandle getIrradianceVolumeDepthImageViewHandle();
		uint32_t getCulledProbeCount() const;

	private:
		ImageViewHandle m_irradianceVolumeImageViewHandle = 0;
		ImageViewHandle m_irradianceVolumeDepthImageViewHandle = 0;
		ImageViewHandle m_irradianceVolumeAgeImageViewHandle = 0;

		VkImage m_irradianceVolumeImage = VK_NULL_HANDLE;
		VkImage m_irradianceVolumeDepthImage = VK_NULL_HANDLE;
		VkImage m_irradianceVolumeAgeImage = VK_NULL_HANDLE;
		VkBuffer m_probeQueueBuffers[RendererConsts::FRAMES_IN_FLIGHT] = {};
		VkBuffer m_culledProbesReadBackBuffers[RendererConsts::FRAMES_IN_FLIGHT];

		VKAllocationHandle m_irradianceVolumeImageAllocHandle = nullptr;
		VKAllocationHandle m_irradianceVolumeDepthImageAllocHandle = nullptr;
		VKAllocationHandle m_irradianceVolumeAgeImageAllocHandle = nullptr;
		VKAllocationHandle m_probeQueueBuffersAllocHandles[RendererConsts::FRAMES_IN_FLIGHT] = {};
		VKAllocationHandle m_culledProbesReadBackBufferAllocHandles[RendererConsts::FRAMES_IN_FLIGHT];

		VkQueue m_probeQueueBuffersQueue[RendererConsts::FRAMES_IN_FLIGHT] = {};
		ResourceState m_probeQueueBuffersResourceState[RendererConsts::FRAMES_IN_FLIGHT] = {};
		VkQueue m_irradianceVolumeImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_irradianceVolumeImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_irradianceVolumeDepthImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_irradianceVolumeDepthImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_irradianceVolumeAgeImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_irradianceVolumeAgeImageResourceState = ResourceState::UNDEFINED;

		uint32_t m_culledProbeCount;
	};
}