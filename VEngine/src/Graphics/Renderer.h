#pragma once
#include "Graphics/RendererConsts.h"
#include "Handles.h"
#include "Graphics/PassTimingInfo.h"
#include "gal/FwdDecl.h"

namespace VEngine
{
	namespace rg
	{
		class RenderGraph;
	}

	struct RenderResources;
	class TextureLoader;
	class TextureManager;
	class MaterialManager;
	class MeshManager;
	class PipelineCache;
	class DescriptorSetCache;
	struct CommonRenderData;
	struct RenderData;
	struct LightData;
	struct Material;
	struct SubMesh;
	struct BVHNode;
	struct Triangle;
	class GTAOModule;
	class SSRModule;
	class VolumetricFogModule;
	class ReflectionProbeModule;
	class AtmosphericScatteringModule;

	class Renderer
	{
	public:
		explicit Renderer(uint32_t width, uint32_t height, void *windowHandle);
		~Renderer();
		void render(const CommonRenderData &commonData, const RenderData &renderData, const LightData &lightData);
		Texture2DHandle loadTexture(const char *filepath);
		Texture3DHandle loadTexture3D(const char *filepath);
		void freeTexture(Texture2DHandle handle);
		void freeTexture(Texture3DHandle handle);
		void createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void destroyMaterials(uint32_t count, MaterialHandle *handles);
		void createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles);
		void destroySubMeshes(uint32_t count, SubMeshHandle *handles);
		void updateTextureData();
		void updateTexture3DData();
		const uint32_t *getLuminanceHistogram() const;
		void getTimingInfo(size_t *count, const PassTimingInfo **data) const;
		void getOcclusionCullingStats(uint32_t &draws, uint32_t &totalDraws) const;
		void setBVH(uint32_t nodeCount, const BVHNode *nodes, uint32_t triangleCount, const Triangle *triangles);
		void resize(uint32_t width, uint32_t height);

	private:
		gal::GraphicsDevice *m_graphicsDevice;
		gal::SwapChain *m_swapChain;
		gal::Semaphore *m_semaphores[3];
		uint64_t m_semaphoreValues[3];
		RenderResources *m_renderResources;
		PipelineCache *m_pipelineCache;
		DescriptorSetCache *m_descriptorSetCache;
		TextureLoader *m_textureLoader;
		TextureManager *m_textureManager;
		MaterialManager *m_materialManager;
		MeshManager *m_meshManager;
		rg::RenderGraph *m_frameGraphs[RendererConsts::FRAMES_IN_FLIGHT];
		GTAOModule *m_gtaoModule;
		SSRModule *m_ssrModule;
		VolumetricFogModule *m_volumetricFogModule;
		ReflectionProbeModule *m_reflectionProbeModule;
		AtmosphericScatteringModule *m_atmosphericScatteringModule;

		uint32_t m_luminanceHistogram[RendererConsts::LUMINANCE_HISTOGRAM_SIZE];

		uint32_t m_width;
		uint32_t m_height;
		Texture2DHandle m_blueNoiseTextureIndex;
		size_t m_passTimingCount;
		const PassTimingInfo *m_passTimingData;
		uint32_t m_opaqueDraws;
		uint32_t m_totalOpaqueDraws;
		uint32_t m_totalOpaqueDrawsPending[RendererConsts::FRAMES_IN_FLIGHT];
		uint32_t m_framesSinceLastResize;
	};
}