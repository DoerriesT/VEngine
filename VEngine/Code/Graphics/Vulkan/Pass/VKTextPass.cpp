#include "VKTextPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"

VEngine::VKTextPass::VKTextPass(VkPipeline pipeline, VkPipelineLayout pipelineLayout, VKRenderResources *renderResources, uint32_t width, uint32_t height, uint32_t atlasTextureIndex, size_t stringCount, const String *strings)
	:m_pipeline(pipeline),
	m_pipelineLayout(pipelineLayout),
	m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_atlasTextureIndex(atlasTextureIndex),
	m_stringCount(stringCount),
	m_strings(strings)
{
}

void VEngine::VKTextPass::addToGraph(FrameGraph::Graph &graph, FrameGraph::ImageHandle colorTextureHandle)
{
	auto builder = graph.addPass("Text Pass", FrameGraph::PassType::GRAPHICS, FrameGraph::QueueType::GRAPHICS, this);

	builder.setDimensions(m_width, m_height);
	builder.writeColorAttachment(colorTextureHandle, 0);
}

void VEngine::VKTextPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_width);
	viewport.height = static_cast<float>(m_height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { static_cast<int32_t>(viewport.x), static_cast<int32_t>(viewport.y) };
	scissor.extent = { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };

	vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_renderResources->m_textureDescriptorSet, 0, nullptr);

	const size_t symbolHeight = 48;
	const size_t symbolWidth = 24;
	const size_t atlasWidth = 1024;
	const size_t atlasHeight = 512;
	const size_t symbolsPerRow = atlasWidth / symbolWidth;
	const float invAtlasWidth = 1.0f / atlasWidth;
	const float invAtlasHeight = 1.0f / atlasHeight;
	const float invWidth = 1.0f / m_width;
	const float invHeight = 1.0f / m_height;
	const float texCoordSizeX = symbolWidth * invAtlasWidth;
	const float texCoordSizeY = symbolHeight * invAtlasHeight;
	const float scaleX = 0.4f;
	const float scaleY = 0.4f;
	const float charScaleX = symbolWidth * scaleX * invWidth;
	const float charScaleY = symbolHeight * scaleY * invHeight;

	for (size_t i = 0; i < m_stringCount; ++i)
	{
		const String &str = m_strings[i];
		const char *c = str.m_chars;

		size_t offsetX = str.m_positionX;
		size_t offsetY = str.m_positionY;

		while (*c)
		{
			if (*c == '\n')
			{
				offsetY += static_cast<size_t>(symbolHeight * scaleY);
				offsetX = str.m_positionX;
			}
			else
			{
				struct PushConsts
				{
					float scaleBias[4];
					float color[4];
					float texCoordOffset[2];
					float texCoordSize[2];
					uint32_t atlasTextureIndex;
				} pushConsts;

				pushConsts.scaleBias[0] = charScaleX;
				pushConsts.scaleBias[1] = charScaleY;
				pushConsts.scaleBias[2] = offsetX * invWidth;
				pushConsts.scaleBias[3] = offsetY * invHeight;

				pushConsts.color[0] = 1.0f;
				pushConsts.color[1] = 1.0f;
				pushConsts.color[2] = 1.0f;
				pushConsts.color[3] = 1.0f;

				size_t atlasIndex = *c - 32; // atlas starts at symbol 32
				pushConsts.texCoordOffset[0] = ((atlasIndex % symbolsPerRow) * symbolWidth) * invAtlasWidth;
				pushConsts.texCoordOffset[1] = ((atlasIndex / symbolsPerRow) * symbolHeight) * invAtlasHeight;
				pushConsts.texCoordSize[0] = texCoordSizeX;
				pushConsts.texCoordSize[1] = texCoordSizeY;

				pushConsts.atlasTextureIndex = m_atlasTextureIndex;

				vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);
				vkCmdDraw(cmdBuf, 6, 1, 0, 0);

				offsetX += static_cast<size_t>(symbolWidth * scaleX);
			}

			++c;
		}
	}
}
