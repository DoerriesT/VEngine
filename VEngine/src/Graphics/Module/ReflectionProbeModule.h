#pragma once
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext;
	struct RenderResources;
	struct LightData;
	struct RenderData;
	struct DirectionalLight;
	struct SubMeshInstanceData;
	struct SubMeshInfo;

	class ReflectionProbeModule
	{
	public:

		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_ignoreHistory;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_velocityImageViewHandle;
		};

		struct ShadowRenderingData
		{
			PassRecordContext *m_passRecordContext;
			const RenderData *m_renderData;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			const DirectionalLight *m_directionalLightsShadowedProbe;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
		};

		struct GBufferRenderingData
		{
			PassRecordContext *m_passRecordContext;
			const RenderData *m_renderData;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
		};

		struct RelightingData
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_relightCount;
			const uint32_t *m_relightProbeIndices;
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
			const LightData *m_lightData;
		};

		explicit ReflectionProbeModule(gal::GraphicsDevice *graphicsDevice, RenderResources *renderResources);
		ReflectionProbeModule(const ReflectionProbeModule &) = delete;
		ReflectionProbeModule(const ReflectionProbeModule &&) = delete;
		ReflectionProbeModule &operator= (const ReflectionProbeModule &) = delete;
		ReflectionProbeModule &operator= (const ReflectionProbeModule &&) = delete;
		~ReflectionProbeModule();

		void addGBufferRenderingToGraph(rg::RenderGraph &graph, const GBufferRenderingData &data);
		void addShadowRenderingToGraph(rg::RenderGraph &graph, const ShadowRenderingData &data);
		void addRelightingToGraph(rg::RenderGraph &graph, const RelightingData &data);
		gal::ImageView *getCubeArrayView();
		

	private:
		gal::GraphicsDevice *m_graphicsDevice;

		gal::Image *m_probeDepthArrayImage = {};
		gal::Image *m_probeAlbedoRoughnessArrayImage = {};
		gal::Image *m_probeNormalArrayImage = {};
		gal::Image *m_probeArrayImage = {};
		gal::Image *m_probeTmpImage = {};
		//gal::Image *m_probeUncompressedLitImage = {};
		//gal::Image *m_probeCompressedTmpLitImage = {};
		//gal::Image *m_probeFilterCoeffsImage = {};

		gal::ImageView *m_probeTmpCubeViews[RendererConsts::REFLECTION_PROBE_MIPS] = {};
		gal::ImageView *m_probeDepthArrayView = {};
		gal::ImageView *m_probeDepthSliceViews[RendererConsts::REFLECTION_PROBE_CACHE_SIZE * 6] = {};
		gal::ImageView *m_probeAlbedoRoughnessArrayView = {};
		gal::ImageView *m_probeAlbedoRoughnessSliceViews[RendererConsts::REFLECTION_PROBE_CACHE_SIZE * 6] = {};
		gal::ImageView *m_probeNormalArrayView = {};
		gal::ImageView *m_probeNormalSliceViews[RendererConsts::REFLECTION_PROBE_CACHE_SIZE * 6] = {};
		gal::ImageView *m_probeCubeArrayView = {};
		gal::ImageView *m_probeMipViews[RendererConsts::REFLECTION_PROBE_CACHE_SIZE][RendererConsts::REFLECTION_PROBE_MIPS] = {};
		//gal::ImageView *m_probeCompressedTmpMipViews[RendererConsts::REFLECTION_PROBE_MIPS] = {};
		//gal::ImageView *m_probeUncompressedMipViews[RendererConsts::REFLECTION_PROBE_MIPS] = {};
		//gal::ImageView *m_probeFilterCoeffsImageView = {};

		rg::ImageViewHandle m_probeShadowImageViewHandle = 0;
		rg::ResourceStateData m_probeTmpImageState[6 * RendererConsts::REFLECTION_PROBE_MIPS] = {}; // 6 faces and REFLECTION_PROBE_MIPS mip levels
	};
}