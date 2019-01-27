#include "Passes.h"
#include "GlobalVar.h"

void VEngine::FrameGraphPasses::addGBufferFillPass(FrameGraph::Graph &graph,
	FrameGraph::ImageHandle &depthTextureHandle,
	FrameGraph::ImageHandle &albedoTextureHandle,
	FrameGraph::ImageHandle &normalTextureHandle,
	FrameGraph::ImageHandle &materialTextureHandle,
	FrameGraph::ImageHandle &velocityTextureHandle)
{
	graph.addGraphicsPass("GBuffer Fill", [&](FrameGraph::PassBuilder &builder)
	{
		builder.setDimensions(g_windowWidth, g_windowHeight);

		if (!depthTextureHandle)
		{
			FrameGraph::ImageDesc depthDesc = {};
			depthDesc.m_name = "DepthTexture";
			depthDesc.m_initialState = FrameGraph::ImageInitialState::CLEAR;
			depthDesc.m_width = g_windowWidth;
			depthDesc.m_height = g_windowHeight;
			depthDesc.m_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			depthTextureHandle = builder.createImage(depthDesc);
		}

		if (!albedoTextureHandle)
		{
			FrameGraph::ImageDesc albedoDesc = {};
			albedoDesc.m_name = "AlbedoTexture";
			albedoDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			albedoDesc.m_width = g_windowWidth;
			albedoDesc.m_height = g_windowHeight;
			albedoDesc.m_format = VK_FORMAT_R8G8B8A8_UNORM;
			albedoTextureHandle = builder.createImage(albedoDesc);
		}

		if (!normalTextureHandle)
		{
			FrameGraph::ImageDesc normalDesc = {};
			normalDesc.m_name = "NormalTexture";
			normalDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			normalDesc.m_width = g_windowWidth;
			normalDesc.m_height = g_windowHeight;
			normalDesc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
			normalTextureHandle = builder.createImage(normalDesc);
		}

		if (!materialTextureHandle)
		{
			FrameGraph::ImageDesc materialDesc = {};
			materialDesc.m_name = "MaterialTexture";
			materialDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			materialDesc.m_width = g_windowWidth;
			materialDesc.m_height = g_windowHeight;
			materialDesc.m_format = VK_FORMAT_R8G8B8A8_UNORM;
			materialTextureHandle = builder.createImage(materialDesc);
		}

		if (!velocityTextureHandle)
		{
			FrameGraph::ImageDesc velocityDesc = {};
			velocityDesc.m_name = "VelocityTexture";
			velocityDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			velocityDesc.m_width = g_windowWidth;
			velocityDesc.m_height = g_windowHeight;
			velocityDesc.m_format = VK_FORMAT_R16G16_SFLOAT;
			velocityTextureHandle = builder.createImage(velocityDesc);
		}

		builder.writeDepthStencil(depthTextureHandle);
		builder.writeColorAttachment(albedoTextureHandle, 0);
		builder.writeColorAttachment(normalTextureHandle, 1);
		builder.writeColorAttachment(materialTextureHandle, 2);
		builder.writeColorAttachment(velocityTextureHandle, 3);

		return [=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
		{
		};
	});
}

void VEngine::FrameGraphPasses::addGBufferFillAlphaPass(FrameGraph::Graph &graph, 
	FrameGraph::ImageHandle &depthTextureHandle,
	FrameGraph::ImageHandle &albedoTextureHandle, 
	FrameGraph::ImageHandle &normalTextureHandle, 
	FrameGraph::ImageHandle &materialTextureHandle, 
	FrameGraph::ImageHandle &velocityTextureHandle)
{
	graph.addGraphicsPass("GBuffer Fill Alpha", [&](FrameGraph::PassBuilder &builder)
	{
		builder.setDimensions(g_windowWidth, g_windowHeight);

		if (!depthTextureHandle)
		{
			FrameGraph::ImageDesc depthDesc = {};
			depthDesc.m_name = "DepthTexture";
			depthDesc.m_initialState = FrameGraph::ImageInitialState::CLEAR;
			depthDesc.m_width = g_windowWidth;
			depthDesc.m_height = g_windowHeight;
			depthDesc.m_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			depthTextureHandle = builder.createImage(depthDesc);
		}

		if (!albedoTextureHandle)
		{
			FrameGraph::ImageDesc albedoDesc = {};
			albedoDesc.m_name = "AlbedoTexture";
			albedoDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			albedoDesc.m_width = g_windowWidth;
			albedoDesc.m_height = g_windowHeight;
			albedoDesc.m_format = VK_FORMAT_R8G8B8A8_UNORM;
			albedoTextureHandle = builder.createImage(albedoDesc);
		}

		if (!normalTextureHandle)
		{
			FrameGraph::ImageDesc normalDesc = {};
			normalDesc.m_name = "NormalTexture";
			normalDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			normalDesc.m_width = g_windowWidth;
			normalDesc.m_height = g_windowHeight;
			normalDesc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
			normalTextureHandle = builder.createImage(normalDesc);
		}

		if (!materialTextureHandle)
		{
			FrameGraph::ImageDesc materialDesc = {};
			materialDesc.m_name = "MaterialTexture";
			materialDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			materialDesc.m_width = g_windowWidth;
			materialDesc.m_height = g_windowHeight;
			materialDesc.m_format = VK_FORMAT_R8G8B8A8_UNORM;
			materialTextureHandle = builder.createImage(materialDesc);
		}

		if (!velocityTextureHandle)
		{
			FrameGraph::ImageDesc velocityDesc = {};
			velocityDesc.m_name = "VelocityTexture";
			velocityDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			velocityDesc.m_width = g_windowWidth;
			velocityDesc.m_height = g_windowHeight;
			velocityDesc.m_format = VK_FORMAT_R16G16_SFLOAT;
			velocityTextureHandle = builder.createImage(velocityDesc);
		}

		builder.writeDepthStencil(depthTextureHandle);
		builder.writeColorAttachment(albedoTextureHandle, 0);
		builder.writeColorAttachment(normalTextureHandle, 1);
		builder.writeColorAttachment(materialTextureHandle, 2);
		builder.writeColorAttachment(velocityTextureHandle, 3);

		return [=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
		{
		};
	});
}

void VEngine::FrameGraphPasses::addTilingPass(FrameGraph::Graph &graph,
	FrameGraph::BufferHandle &tiledLightBufferHandle)
{
	graph.addComputePass("Tiling", FrameGraph::QueueType::GRAPHICS, [&](FrameGraph::PassBuilder &builder)
	{
		FrameGraph::BufferDesc bufferDesc = { "TilingBuffer", 8192 * sizeof(uint32_t) };
		tiledLightBufferHandle = builder.createBuffer(bufferDesc);

		builder.writeStorageBuffer(tiledLightBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		return [=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
		{
		};
	});
}

void VEngine::FrameGraphPasses::addShadowPass(FrameGraph::Graph &graph,
	FrameGraph::ImageHandle &shadowTextureHandle)
{
	graph.addGraphicsPass("Shadows", [&](FrameGraph::PassBuilder &builder)
	{
		builder.setDimensions(g_shadowAtlasSize, g_shadowAtlasSize);

		builder.writeDepthStencil(shadowTextureHandle);

		return [=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
		{
		};
	});
}

void VEngine::FrameGraphPasses::addLightingPass(FrameGraph::Graph &graph,
	FrameGraph::ImageHandle depthTextureHandle,
	FrameGraph::ImageHandle albedoTextureHandle,
	FrameGraph::ImageHandle normalTextureHandle,
	FrameGraph::ImageHandle materialTextureHandle,
	FrameGraph::ImageHandle shadowTextureHandle,
	FrameGraph::BufferHandle tiledLightBufferHandle,
	FrameGraph::ImageHandle &lightTextureHandle)
{
	graph.addComputePass("Lighting", FrameGraph::QueueType::GRAPHICS, [&](FrameGraph::PassBuilder &builder)
	{
		if (!lightTextureHandle)
		{
			FrameGraph::ImageDesc lightDesc = {};
			lightDesc.m_name = "LightTexture";
			lightDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			lightDesc.m_width = g_windowWidth;
			lightDesc.m_height = g_windowHeight;
			lightDesc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
			lightTextureHandle = builder.createImage(lightDesc);
		}

		builder.readTexture(depthTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(albedoTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(normalTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(materialTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(shadowTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readStorageBuffer(tiledLightBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.writeStorageImage(lightTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		return [=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
		{
		};
	});
}

void VEngine::FrameGraphPasses::addForwardPass(FrameGraph::Graph &graph,
	FrameGraph::ImageHandle shadowTextureHandle,
	FrameGraph::BufferHandle tiledLightBufferHandle,
	FrameGraph::ImageHandle &depthTextureHandle,
	FrameGraph::ImageHandle &velocityTextureHandle,
	FrameGraph::ImageHandle &lightTextureHandle)
{
	graph.addGraphicsPass("Forward", [&](FrameGraph::PassBuilder &builder)
	{
		builder.setDimensions(g_windowWidth, g_windowHeight);

		if (!lightTextureHandle)
		{
			FrameGraph::ImageDesc lightDesc = {};
			lightDesc.m_name = "LightTexture";
			lightDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			lightDesc.m_width = g_windowWidth;
			lightDesc.m_height = g_windowHeight;
			lightDesc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
			lightTextureHandle = builder.createImage(lightDesc);
		}

		if (!depthTextureHandle)
		{
			FrameGraph::ImageDesc depthDesc = {};
			depthDesc.m_name = "DepthTexture";
			depthDesc.m_initialState = FrameGraph::ImageInitialState::CLEAR;
			depthDesc.m_width = g_windowWidth;
			depthDesc.m_height = g_windowHeight;
			depthDesc.m_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			depthTextureHandle = builder.createImage(depthDesc);
		}

		if (!velocityTextureHandle)
		{
			FrameGraph::ImageDesc velocityDesc = {};
			velocityDesc.m_name = "VelocityTexture";
			velocityDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
			velocityDesc.m_width = g_windowWidth;
			velocityDesc.m_height = g_windowHeight;
			velocityDesc.m_format = VK_FORMAT_R16G16_SFLOAT;
			velocityTextureHandle = builder.createImage(velocityDesc);
		}

		builder.readTexture(shadowTextureHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readStorageBuffer(tiledLightBufferHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.writeDepthStencil(depthTextureHandle);
		builder.writeColorAttachment(lightTextureHandle, 0);
		builder.writeColorAttachment(velocityTextureHandle, 1);

		return [=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
		{
		};
	});
}
