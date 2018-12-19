#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawLists;
	class VKGeometryPipeline;
	class VKGeometryAlphaMaskPipeline;

	class VKGeometryRenderPass
	{
	public:
		explicit VKGeometryRenderPass(VKRenderResources *renderResources);
		VKGeometryRenderPass(const VKGeometryRenderPass &) = delete;
		VKGeometryRenderPass(const VKGeometryRenderPass &&) = delete;
		VKGeometryRenderPass &operator= (const VKGeometryRenderPass &) = delete;
		VKGeometryRenderPass &operator= (const VKGeometryRenderPass &&) = delete;
		~VKGeometryRenderPass();

		VkRenderPass get();
		void record(VKRenderResources *renderResources, const DrawLists &drawLists, uint32_t width, uint32_t height);
		void submit(VKRenderResources *renderResources);
		void setPipelines(VKGeometryPipeline *geometryPipeline, VKGeometryAlphaMaskPipeline *geometryAlphaMaskPipeline);

	private:
		VkRenderPass m_renderPass;
		VKGeometryPipeline *m_geometryPipeline;
		VKGeometryAlphaMaskPipeline *m_geometryAlphaMaskPipeline;
	};
}