#include "FourierOpacityLocalLightPass.h"
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

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/fourierOpacityLocal.hlsli"
}

void VEngine::FourierOpacityLocalLightPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *ssboBuffer = data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[commonData->m_curResIdx].get();

	// light info
	DescriptorBufferInfo lightBufferInfo{ nullptr, 0, sizeof(LightInfo) * data.m_drawCount, sizeof(LightInfo) };
	uint64_t alignment = graph.getGraphicsDevice()->getBufferAlignment(DescriptorType2::STRUCTURED_BUFFER, sizeof(LightInfo));
	uint8_t *lightDataPtr = nullptr;
	ssboBuffer->allocate(alignment, lightBufferInfo.m_range, lightBufferInfo.m_offset, lightBufferInfo.m_buffer, lightDataPtr);

	for (size_t i = 0; i < data.m_drawCount; ++i)
	{
		const auto &info = data.m_drawInfo[i];

		uint32_t resolution = info.m_pointLight ? (info.m_size - 2) : info.m_size;

		LightInfo lightInfo;
		lightInfo.viewProjection = info.m_shadowMatrix;
		lightInfo.invViewProjection = glm::inverse(info.m_shadowMatrix);
		lightInfo.position = info.m_lightPosition;
		lightInfo.radius = info.m_lightRadius;
		lightInfo.texelSize = 1.0f / resolution;
		lightInfo.resolution = resolution;
		lightInfo.offsetX = info.m_offsetX + (info.m_pointLight ? 1 : 0);
		lightInfo.offsetY = info.m_offsetY + (info.m_pointLight ? 1 : 0);
		lightInfo.isPointLight = info.m_pointLight;

		((LightInfo *)lightDataPtr)[i] = lightInfo;
	}

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_fomImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("FOM Local Lights", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// render volume opacity
			{
				// create pipeline description
				ComputePipelineCreateInfo pipelineCreateInfo;
				ComputePipelineBuilder builder(pipelineCreateInfo);
				builder.setComputeShader("Resources/Shaders/hlsl/fourierOpacityLocal_cs");

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *resultImageView = registry.getImageView(data.m_fomImageViewHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
						Initializers::structuredBuffer(&lightBufferInfo, LIGHT_INFO_BINDING),
						Initializers::structuredBuffer(&data.m_localMediaBufferInfo, LOCAL_MEDIA_BINDING),
						Initializers::structuredBuffer(&data.m_globalMediaBufferInfo, GLOBAL_MEDIA_BINDING),
						Initializers::structuredBuffer(&data.m_particleBufferInfo, PARTICLE_DATA_BINDING),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);

					DescriptorSet *sets[]
					{ 
						descriptorSet, 
						data.m_passRecordContext->m_renderResources->m_computeTexture3DDescriptorSet, 
						data.m_passRecordContext->m_renderResources->m_computeTextureDescriptorSet,
						data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet
					};
					cmdList->bindDescriptorSets(pipeline, 0, 4, sets);
				}

				PushConsts pushConsts;
				pushConsts.lightIndex = 0;
				pushConsts.globalMediaCount = commonData->m_globalParticipatingMediaCount;
				pushConsts.localVolumeCount = commonData->m_localParticipatingMediaCount;
				pushConsts.particleCount = data.m_particleCount;

				for (size_t i = 0; i < data.m_drawCount; ++i)
				{
					pushConsts.lightIndex = static_cast<uint32_t>(i);
					cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

					const auto &info = data.m_drawInfo[i];
					uint32_t res = info.m_pointLight ? (info.m_size - 2) : info.m_size;
					cmdList->dispatch((res + 7) / 8, (res + 7) / 8, 1);
				}
			}
		}, true);
}
