#include "FourierOpacityPass.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/RenderResources.h"
#include "Graphics/LightData.h"
#include <glm/gtx/transform.hpp>
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <glm/vec3.hpp>
#include <glm/ext.hpp>
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

extern glm::vec3 g_lightPos;
extern glm::vec3 g_lightDir;
extern float g_lightAngle;
extern float g_lightRadius;
extern glm::vec3 g_volumePos;
extern glm::quat g_volumeRot;
extern glm::vec3 g_volumeSize;
extern float g_volumeExtinction;

extern glm::mat4 g_shadowMatrix;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/fourierOpacityVolume.hlsli"
}

void VEngine::FourierOpacityPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *ssboBuffer = data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[commonData->m_curResIdx].get();

	// light info
	DescriptorBufferInfo lightBufferInfo{ nullptr, 0, sizeof(LightInfo) * data.m_drawCount };
	uint8_t *lightDataPtr = nullptr;
	ssboBuffer->allocate(lightBufferInfo.m_range, lightBufferInfo.m_offset, lightBufferInfo.m_buffer, lightDataPtr);

	for (size_t i = 0; i < data.m_drawCount; ++i)
	{
		const auto &info = data.m_drawInfo[i];

		uint32_t resolution = info.m_pointLight ? (info.m_size - 2) : info.m_size;
		
		LightInfo lightInfo;
		lightInfo.invViewProjection = glm::inverse(info.m_shadowMatrix);
		lightInfo.position = info.m_lightPosition;
		lightInfo.depthScale = 1.0f / (info.m_lightRadius - 0.01f);
		lightInfo.depthBias = lightInfo.depthScale * -0.01f;
		lightInfo.radius = info.m_lightRadius;
		lightInfo.texelSize = 1.0f / resolution;
		lightInfo.resolution = resolution;
		lightInfo.offsetX = info.m_offsetX + (info.m_pointLight ? 1 : 0);
		lightInfo.offsetY = info.m_offsetY + (info.m_pointLight ? 1 : 0);
		lightInfo.isPointLight = info.m_pointLight;

		((LightInfo *)lightDataPtr)[i] = lightInfo;
	}

	

	

	//const glm::mat4 vulkanCorrection =
	//{
	//	{ 1.0f, 0.0f, 0.0f, 0.0f },
	//	{ 0.0f, -1.0f, 0.0f, 0.0f },
	//	{ 0.0f, 0.0f, 0.5f, 0.0f },
	//	{ 0.0f, 0.0f, 0.5f, 1.0f }
	//};
	//
	//glm::vec3 upDir(0.0f, 1.0f, 0.0f);
	//// choose different up vector if light direction would be linearly dependent otherwise
	//if (abs(g_lightDir.x) < 0.001f && abs(g_lightDir.z) < 0.001f)
	//{
	//	upDir = glm::vec3(1.0f, 0.0f, 0.0f);
	//}
	//
	//glm::mat4 shadowMatrix = vulkanCorrection * glm::perspective(g_lightAngle, 1.0f, 0.01f, g_lightRadius)
	//	* glm::lookAt(g_lightPos, g_lightPos + g_lightDir, upDir);
	//
	//g_shadowMatrix = shadowMatrix;
	//
	//bool pointLight = true;
	//uint32_t resolution = pointLight ? 128 - 2 : 128;
	//
	//LightInfo lightInfo;
	//lightInfo.invViewProjection = glm::inverse(shadowMatrix);
	//lightInfo.position = g_lightPos;
	//lightInfo.depthScale = 1.0f / (g_lightRadius - 0.01f);
	//lightInfo.depthBias = lightInfo.depthScale * -0.01f;
	//lightInfo.radius = g_lightRadius;
	//lightInfo.texelSize = 1.0f / resolution;
	//lightInfo.resolution = resolution;
	//lightInfo.offsetX = pointLight ? 1 : 0;
	//lightInfo.offsetY = pointLight ? 1 : 0;
	//lightInfo.isPointLight = pointLight;
	//
	//memcpy(lightDataPtr, &lightInfo, sizeof(lightInfo));

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_fomImageViewHandle0), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_fomImageViewHandle1), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Fourier Opacity", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/fourierOpacityVolume_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView0 = registry.getImageView(data.m_fomImageViewHandle0);
				ImageView *resultImageView1 = registry.getImageView(data.m_fomImageViewHandle1);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView0, RESULT_0_IMAGE_BINDING),
					Initializers::storageImage(&resultImageView1, RESULT_1_IMAGE_BINDING),
					Initializers::storageBuffer(&lightBufferInfo, LIGHT_INFO_BINDING),
					Initializers::storageBuffer(&data.m_localMediaBufferInfo, LOCAL_MEDIA_BINDING),
					Initializers::storageBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			PushConsts pushConsts;
			pushConsts.lightIndex = 0;
			pushConsts.globalMediaCount = commonData->m_globalParticipatingMediaCount;
			pushConsts.localVolumeCount = commonData->m_localParticipatingMediaCount;

			for (size_t i = 0; i < data.m_drawCount; ++i)
			{
				pushConsts.lightIndex = static_cast<uint32_t>(i);
				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				const auto &info = data.m_drawInfo[i];
				uint32_t res = info.m_pointLight ? (info.m_size - 2) : info.m_size;
				cmdList->dispatch(res, res, 1);
			}
		}, true);
}
