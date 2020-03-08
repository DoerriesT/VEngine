#include "SSRResolvePass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Utility/Utility.h"
#include <glm/detail/func_trigonometric.hpp>

namespace
{
	constexpr size_t numHaltonSamples = 1024;
	float haltonX[numHaltonSamples];
	float haltonY[numHaltonSamples];

	using namespace glm;
#include "../../../../../Application/Resources/Shaders/ssrResolve_bindings.h"
}

void VEngine::SSRResolvePass::addToGraph(RenderGraph &graph, const Data &data)
{
	static bool initialized = false;
	if (!initialized)
	{
		initialized = true;
		for (size_t i = 0; i < numHaltonSamples; ++i)
		{
			haltonX[i] = Utility::halton(i + 1, 2);
			haltonY[i] = Utility::halton(i + 1, 3);
		}
	}

	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_resultImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_resultMaskImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_rayHitPDFImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_maskImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_depthImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_normalImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_albedoImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_prevColorImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_velocityImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
	};

	graph.addPass("SSR Resolve", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			SpecEntry specEntries[]
			{
				SpecEntry(WIDTH_CONST_ID, width),
				SpecEntry(HEIGHT_CONST_ID, height),
				SpecEntry(TEXEL_WIDTH_CONST_ID, 1.0f / width),
				SpecEntry(TEXEL_HEIGHT_CONST_ID, 1.0f / height),
			};

			ComputePipelineDesc pipelineDesc;
			pipelineDesc.setComputeShader("Resources/Shaders/ssrResolve_comp.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
				VkSampler linearSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_resultImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_resultMaskImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_MASK_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_rayHitPDFImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER), RAY_HIT_PDF_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_maskImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER), MASK_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_depthImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER), DEPTH_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_normalImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER), NORMAL_IMAGE_BINDING);
				//writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_albedoImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER), ALBEDO_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_prevColorImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER), PREV_COLOR_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_velocityImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER), VELOCITY_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLER, { pointSamplerClamp }, POINT_SAMPLER_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLER, { linearSamplerClamp }, LINEAR_SAMPLER_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet };
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 2, descriptorSets, 0, nullptr);

			uint32_t maxLevel = 1;
			{
				uint32_t w = width;
				uint32_t h = height;
				while (w > 1 || h > 1)
				{
					++maxLevel;
					w /= 2;
					h /= 2;
				}
			}

			const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
			const auto &projMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredProjectionMatrix;

			PushConsts pushConsts;
			pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
			pushConsts.depthProjectParams = glm::vec2(projMatrix[2][2], projMatrix[3][2]);
			pushConsts.noiseScale = glm::vec2(1.0f / 64.0f);
			const size_t haltonIdx = data.m_passRecordContext->m_commonRenderData->m_frame % numHaltonSamples;
			pushConsts.noiseJitter = glm::vec2(haltonX[haltonIdx], haltonY[haltonIdx]);// *0.0f;
			pushConsts.noiseTexId = data.m_noiseTextureHandle - 1;
			pushConsts.diameterToScreen = 0.5f / tanf(glm::radians(data.m_passRecordContext->m_commonRenderData->m_fovy) * 0.5f) * glm::min(width, height);
			pushConsts.bias = data.m_bias * 0.5f;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			VKUtility::dispatchComputeHelper(cmdBuf, width, height, 1, 8, 8, 1);
		});
}
