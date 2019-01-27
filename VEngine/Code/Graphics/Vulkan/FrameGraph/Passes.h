#pragma once
#include "FrameGraph.h"

namespace VEngine
{
	namespace FrameGraphPasses
	{
		void addGBufferFillPass(FrameGraph::Graph &graph,
			FrameGraph::ImageHandle &depthTextureHandle,
			FrameGraph::ImageHandle &albedoTextureHandle,
			FrameGraph::ImageHandle &normalTextureHandle,
			FrameGraph::ImageHandle &materialTextureHandle,
			FrameGraph::ImageHandle &velocityTextureHandle);

		void addGBufferFillAlphaPass(FrameGraph::Graph &graph,
			FrameGraph::ImageHandle &depthTextureHandle,
			FrameGraph::ImageHandle &albedoTextureHandle,
			FrameGraph::ImageHandle &normalTextureHandle,
			FrameGraph::ImageHandle &materialTextureHandle,
			FrameGraph::ImageHandle &velocityTextureHandle);

		void addTilingPass(FrameGraph::Graph &graph, 
			FrameGraph::BufferHandle &tiledLightBufferHandle);
	
		void addShadowPass(FrameGraph::Graph &graph,
			FrameGraph::ImageHandle &shadowTextureHandle);

		void addLightingPass(FrameGraph::Graph &graph,
			FrameGraph::ImageHandle depthTextureHandle,
			FrameGraph::ImageHandle albedoTextureHandle,
			FrameGraph::ImageHandle normalTextureHandle,
			FrameGraph::ImageHandle materialTextureHandle,
			FrameGraph::ImageHandle shadowTextureHandle,
			FrameGraph::BufferHandle tiledLightBufferHandle,
			FrameGraph::ImageHandle &lightTextureHandle);

		void addForwardPass(FrameGraph::Graph &graph,
			FrameGraph::ImageHandle shadowTextureHandle,
			FrameGraph::BufferHandle tiledLightBufferHandle,
			FrameGraph::ImageHandle &depthTextureHandle,
			FrameGraph::ImageHandle &velocityTextureHandle,
			FrameGraph::ImageHandle &lightTextureHandle);
	}
}