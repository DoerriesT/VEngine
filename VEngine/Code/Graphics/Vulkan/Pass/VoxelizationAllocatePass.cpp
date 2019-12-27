#include "VoxelizationAllocatePass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/voxelizationAllocate_bindings.h"
}

void VEngine::VoxelizationAllocatePass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		 { ResourceViewHandle(data.m_brickPointerImageHandle), ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER },
		 {ResourceViewHandle(data.m_markImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		 { ResourceViewHandle(data.m_freeBricksBufferHandle), ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
	};

	graph.addPass("Voxelization Allocate", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// create pipeline description
			SpecEntry specEntries[]
			{
				SpecEntry(BRICK_VOLUME_WIDTH_CONST_ID, RendererConsts::BRICK_VOLUME_WIDTH),
				SpecEntry(BRICK_VOLUME_HEIGHT_CONST_ID, RendererConsts::BRICK_VOLUME_HEIGHT),
				SpecEntry(BRICK_VOLUME_DEPTH_CONST_ID, RendererConsts::BRICK_VOLUME_DEPTH),
			};

			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/voxelizationAllocate_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_brickPointerImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER), BRICK_PTR_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_markImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX]), MARK_IMAGE_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_freeBricksBufferHandle), FREE_BRICKS_BUFFER_BINDING);

				writer.commit();
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			vkCmdDispatch(cmdBuf, (RendererConsts::BRICK_VOLUME_WIDTH + 7) / 8, (RendererConsts::BRICK_VOLUME_HEIGHT + 7) / 8, RendererConsts::BRICK_VOLUME_DEPTH);

		}, true);
}
