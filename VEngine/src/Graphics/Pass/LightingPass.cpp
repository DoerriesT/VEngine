//#include "LightingPass.h"
//#include "Graphics/RenderResources.h"
//#include "Graphics/PipelineCache.h"
//#include "Graphics/DescriptorSetCache.h"
//#include "Graphics/PassRecordContext.h"
//#include "Graphics/RenderData.h"
//#include "Graphics/gal/Initializers.h"
//
//using namespace VEngine::gal;
//
//namespace
//{
//	using namespace glm;
//#include "../../../../Application/Resources/Shaders/lighting_bindings.h"
//}
//
//
//void VEngine::LightingPass::addToGraph(rg::RenderGraph &graph, const Data &data)
//{
//	rg::ResourceUsageDescription passUsages[]
//	{
//		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
//		{rg::ResourceViewHandle(data.m_albedoImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
//		{rg::ResourceViewHandle(data.m_normalImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
//		{rg::ResourceViewHandle(data.m_pointLightBitMaskBufferHandle), {gal::ResourceState::READ_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
//		{rg::ResourceViewHandle(data.m_depthImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
//		{rg::ResourceViewHandle(data.m_uvImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
//		{rg::ResourceViewHandle(data.m_ddxyLengthImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
//		{rg::ResourceViewHandle(data.m_ddxyRotMaterialIdImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
//		{rg::ResourceViewHandle(data.m_tangentSpaceImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
//		{rg::ResourceViewHandle(data.m_deferredShadowImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
//	};
//
//	graph.addPass("Lighting", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
//		{
//			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
//			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;
//
//			// create pipeline description
//			ComputePipelineCreateInfo pipelineCreateInfo;
//			ComputePipelineBuilder builder(pipelineCreateInfo);
//			builder.setComputeShader("Resources/Shaders/lighting_comp.spv");
//
//			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);
//
//			cmdList->bindPipeline(pipeline);
//
//			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));
//
//			// update descriptor sets
//			{
//				ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);
//				ImageView *albedoImageView = registry.getImageView(data.m_albedoImageHandle);
//				ImageView *normalImageView = registry.getImageView(data.m_normalImageHandle);
//				ImageView *depthImageView = registry.getImageView(data.m_depthImageHandle);
//				ImageView *uvImageView = registry.getImageView(data.m_uvImageHandle);
//				ImageView *ddxyLengthImageView = registry.getImageView(data.m_ddxyLengthImageHandle);
//				ImageView *ddxyRotMatImageView = registry.getImageView(data.m_ddxyRotMaterialIdImageHandle);
//				ImageView *tangentSpaceImageView = registry.getImageView(data.m_tangentSpaceImageHandle);
//				ImageView *shadowImageView = registry.getImageView(data.m_deferredShadowImageViewHandle);
//				DescriptorBufferInfo pointLightMaskBufferInfo = registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle);
//
//				DescriptorSetUpdate updates[] =
//				{
//					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
//					Initializers::storageImage(&albedoImageView, ALBEDO_IMAGE_BINDING),
//					Initializers::storageImage(&normalImageView, NORMAL_IMAGE_BINDING),
//					Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
//					Initializers::sampledImage(&uvImageView, UV_IMAGE_BINDING),
//					Initializers::sampledImage(&ddxyLengthImageView, DDXY_LENGTH_IMAGE_BINDING),
//					Initializers::sampledImage(&ddxyRotMatImageView, DDXY_ROT_MATERIAL_ID_IMAGE_BINDING),
//					Initializers::sampledImage(&tangentSpaceImageView, TANGENT_SPACE_IMAGE_BINDING),
//					Initializers::sampledImage(&shadowImageView, DEFERRED_SHADOW_IMAGE_BINDING),
//					Initializers::storageBuffer(&data.m_directionalLightDataBufferInfo, DIRECTIONAL_LIGHT_DATA_BINDING),
//					Initializers::storageBuffer(&data.m_pointLightDataBufferInfo, POINT_LIGHT_DATA_BINDING),
//					Initializers::storageBuffer(&data.m_pointLightZBinsBufferInfo, POINT_LIGHT_Z_BINS_BINDING),
//					Initializers::storageBuffer(&pointLightMaskBufferInfo, POINT_LIGHT_MASK_BINDING),
//					Initializers::storageBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
//					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
//				};
//
//				descriptorSet->update(15, updates);
//			}
//
//			DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet };
//			cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);
//
//			const auto &invProjMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
//
//			PushConsts pushConsts;
//			pushConsts.unprojectParams = glm::vec4(invProjMatrix[0][0], invProjMatrix[1][1], invProjMatrix[2][3], invProjMatrix[3][3]);
//			pushConsts.pointLightCount = data.m_passRecordContext->m_commonRenderData->m_pointLightCount;
//
//			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);
//
//			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
//		});
//}
