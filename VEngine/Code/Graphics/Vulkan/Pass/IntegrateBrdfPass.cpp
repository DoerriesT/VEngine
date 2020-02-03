#include "IntegrateBrdfPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"

namespace
{
#include "../../../../../Application/Resources/Shaders/integrateBrdf_bindings.h"
}

void VEngine::IntegrateBrdfPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_resultImageViewHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER, true, ResourceState::READ_TEXTURE_COMPUTE_SHADER},
	};

	graph.addPass("Integrate BRDF", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			const uint32_t width = 128;
			const uint32_t height = 128;

			// create pipeline description
			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/integrateBrdf_comp.spv");
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
				VkSampler linearSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_resultImageViewHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER, pointSamplerClamp), RESULT_IMAGE_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			PushConsts pushConsts;
			pushConsts.width = width;
			pushConsts.height = height;
			pushConsts.texelWidth = 1.0f / width;
			pushConsts.texelHeight = 1.0f / height;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatch(cmdBuf, (width + 7) / 8, (height + 7) / 8, 1);

			// transition image layout to READ_ONLY_OPTIMAL
			VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.image = registry.getImage(data.m_resultImageViewHandle);
			imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
		}, true);
}
