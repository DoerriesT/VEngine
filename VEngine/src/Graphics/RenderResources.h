#pragma once
#include "gal/FwdDecl.h"
#include "RendererConsts.h"
#include "MappableBufferBlock.h"
#include <memory>
#include "RenderGraph.h"

namespace VEngine
{
	struct BVHNode;
	struct Triangle;

	struct RenderResources
	{
		gal::GraphicsDevice *m_graphicsDevice = nullptr;
		gal::CommandListPool *m_commandListPool = nullptr;
		gal::CommandList *m_commandList = nullptr;

		// images
		gal::Image *m_shadowAtlasImage = {};
		gal::Image *m_depthImages[RendererConsts::FRAMES_IN_FLIGHT] = {};
		gal::Image *m_lightImages[RendererConsts::FRAMES_IN_FLIGHT] = {};
		gal::Image *m_taaHistoryTextures[RendererConsts::FRAMES_IN_FLIGHT] = {};
		gal::Image *m_imGuiFontsTexture = {};
		gal::Image *m_brdfLUT = {};

		// views
		gal::ImageView *m_imGuiFontsTextureView = {};

		// buffers
		gal::Buffer *m_lightProxyVertexBuffer = {};
		gal::Buffer *m_lightProxyIndexBuffer = {};
		gal::Buffer *m_avgLuminanceBuffer = {};
		gal::Buffer *m_exposureDataBuffer = {};
		gal::Buffer *m_luminanceHistogramReadBackBuffers[RendererConsts::FRAMES_IN_FLIGHT] = {};
		gal::Buffer *m_occlusionCullStatsReadBackBuffers[RendererConsts::FRAMES_IN_FLIGHT] = {};
		gal::Buffer *m_stagingBuffer = {};
		gal::Buffer *m_materialBuffer = {};
		gal::Buffer *m_vertexBuffer = {};
		gal::Buffer *m_indexBuffer = {};
		gal::Buffer *m_subMeshDataInfoBuffer = {};
		gal::Buffer *m_subMeshBoundingBoxBuffer = {};
		gal::Buffer *m_uboBuffers[RendererConsts::FRAMES_IN_FLIGHT] = {};
		gal::Buffer *m_ssboBuffers[RendererConsts::FRAMES_IN_FLIGHT] = {};

		// mappable buffer blocks
		std::unique_ptr<MappableBufferBlock> m_mappableUBOBlock[RendererConsts::FRAMES_IN_FLIGHT];
		std::unique_ptr<MappableBufferBlock> m_mappableSSBOBlock[RendererConsts::FRAMES_IN_FLIGHT];

		// samplers
		gal::Sampler *m_samplers[4] = {};
		gal::Sampler *m_shadowSampler = {};

		// rendergraph external info
		rg::ResourceStateData m_shadowAtlasImageState = {};
		rg::ResourceStateData m_depthImageState[RendererConsts::FRAMES_IN_FLIGHT] = {};
		rg::ResourceStateData m_lightImageState[RendererConsts::FRAMES_IN_FLIGHT][14] = {};
		rg::ResourceStateData m_taaHistoryTextureState[RendererConsts::FRAMES_IN_FLIGHT] = {};
		rg::ResourceStateData m_avgLuminanceBufferState = {};
		rg::ResourceStateData m_exposureDataBufferState = {};
		rg::ResourceStateData m_brdfLutImageState = {};

		gal::DescriptorSetLayout *m_textureDescriptorSetLayout = {};
		gal::DescriptorSetLayout *m_computeTextureDescriptorSetLayout = {};
		gal::DescriptorSet *m_textureDescriptorSet = {};
		gal::DescriptorSet *m_computeTextureDescriptorSet = {};
		gal::DescriptorSetLayout *m_imGuiDescriptorSetLayout = {};
		gal::DescriptorSet *m_imGuiDescriptorSet = {};
		gal::DescriptorSetPool *m_textureDescriptorSetPool = {};
		gal::DescriptorSetPool *m_computeTextureDescriptorSetPool = {};
		gal::DescriptorSetPool *m_imguiDescriptorSetPool = {};

		// proxy mesh info
		uint32_t m_pointLightProxyMeshIndexCount;
		uint32_t m_pointLightProxyMeshFirstIndex;
		uint32_t m_pointLightProxyMeshVertexOffset;
		uint32_t m_spotLightProxyMeshIndexCount;
		uint32_t m_spotLightProxyMeshFirstIndex;
		uint32_t m_spotLightProxyMeshVertexOffset;
		uint32_t m_boxProxyMeshIndexCount;
		uint32_t m_boxProxyMeshFirstIndex;
		uint32_t m_boxProxyMeshVertexOffset;

		explicit RenderResources(gal::GraphicsDevice *graphicsDevice);
		RenderResources(const RenderResources &) = delete;
		RenderResources(const RenderResources &&) = delete;
		RenderResources &operator=(const RenderResources &) = delete;
		RenderResources &operator=(const RenderResources &&) = delete;
		~RenderResources();
		void init(uint32_t width, uint32_t height);
		void resize(uint32_t width, uint32_t height);
		void updateTextureArray(uint32_t count, gal::ImageView **data);
		void createImGuiFontsTexture();
		void setBVH(uint32_t nodeCount, const BVHNode *nodes, uint32_t triangleCount, const Triangle *triangles);
	};
}