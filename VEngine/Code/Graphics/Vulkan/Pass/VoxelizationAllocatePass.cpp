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
#include "../../../../../Application/Resources/Shaders/clearBricks_bindings.h"
}

void VEngine::VoxelizationAllocatePass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		 { ResourceViewHandle(data.m_brickPointerImageHandle), ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER },
		 { ResourceViewHandle(data.m_freeBricksBufferHandle), ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER },
	};

	graph.addPass("Voxelization Allocate", QueueType::GRAPHICS, 1, passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// create pipeline description
			SpecEntry specEntries[]
			{
				SpecEntry(VOXEL_GRID_WIDTH_CONST_ID, RendererConsts::VOXEL_SCENE_WIDTH),
				SpecEntry(VOXEL_GRID_HEIGHT_CONST_ID, RendererConsts::VOXEL_SCENE_HEIGHT),
				SpecEntry(VOXEL_GRID_DEPTH_CONST_ID, RendererConsts::VOXEL_SCENE_DEPTH),
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

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_brickPointerImageHandle, ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER), VOXEL_PTR_IMAGE_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_freeBricksBufferHandle), FREE_BRICKS_BUFFER_BINDING);

				writer.commit();
			}

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			vkCmdDispatch(cmdBuf, (RendererConsts::VOXEL_SCENE_WIDTH + 7) / 8, (RendererConsts::VOXEL_SCENE_HEIGHT + 7) / 8, RendererConsts::VOXEL_SCENE_DEPTH);

		}, true);
}
