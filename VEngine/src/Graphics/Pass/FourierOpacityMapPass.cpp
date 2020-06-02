#include "FourierOpacityMapPass.h"
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

glm::mat4 g_shadowMatrix;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/fomVolume.hlsli"
}

void VEngine::FourierOpacityMapPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	const auto *commonData = data.m_passRecordContext->m_commonRenderData;
	auto *ssboBuffer = data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[commonData->m_curResIdx].get();

	// light info
	DescriptorBufferInfo lightBufferInfo{ nullptr, 0, sizeof(LightInfo) };
	uint8_t *lightDataPtr = nullptr;
	ssboBuffer->allocate(lightBufferInfo.m_range, lightBufferInfo.m_offset, lightBufferInfo.m_buffer, lightDataPtr);

	const glm::mat4 vulkanCorrection =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.5f, 0.0f },
		{ 0.0f, 0.0f, 0.5f, 1.0f }
	};

	glm::vec3 upDir(0.0f, 1.0f, 0.0f);
	// choose different up vector if light direction would be linearly dependent otherwise
	if (abs(g_lightDir.x) < 0.001f && abs(g_lightDir.z) < 0.001f)
	{
		upDir = glm::vec3(1.0f, 0.0f, 0.0f);
	}

	glm::mat4 shadowMatrix = vulkanCorrection * glm::perspective(g_lightAngle, 1.0f, 0.01f, g_lightRadius)
		* glm::lookAt(g_lightPos, g_lightPos + g_lightDir, upDir);

	g_shadowMatrix = shadowMatrix;

	LightInfo lightInfo;
	lightInfo.viewProjection = shadowMatrix;
	lightInfo.invProjection = glm::inverse(vulkanCorrection * glm::perspective(g_lightAngle, 1.0f, 0.01f, g_lightRadius));
	lightInfo.position = g_lightPos;
	lightInfo.depthScale = 1.0f / (g_lightRadius - 0.01f);
	lightInfo.depthBias = lightInfo.depthScale * -0.01f;

	memcpy(lightDataPtr, &lightInfo, sizeof(lightInfo));


	// volume info
	DescriptorBufferInfo volumeBufferInfo{ nullptr, 0, sizeof(VolumeInfo) };
	uint8_t *volumeDataPtr = nullptr;
	ssboBuffer->allocate(volumeBufferInfo.m_range, volumeBufferInfo.m_offset, volumeBufferInfo.m_buffer, volumeDataPtr);

	glm::mat4 worldToLocalTransposed = glm::transpose(glm::scale(1.0f / g_volumeSize) * glm::mat4_cast(glm::inverse(g_volumeRot)) * glm::translate(-g_volumePos));
	glm::mat4 localToWorldTransposed = glm::transpose(glm::inverse(glm::transpose(worldToLocalTransposed)));

	VolumeInfo volumeInfo;
	volumeInfo.localToWorld0 = localToWorldTransposed[0];
	volumeInfo.localToWorld1 = localToWorldTransposed[1];
	volumeInfo.localToWorld2 = localToWorldTransposed[2];
	volumeInfo.worldToLocal0 = worldToLocalTransposed[0];
	volumeInfo.worldToLocal1 = worldToLocalTransposed[1];
	volumeInfo.worldToLocal2 = worldToLocalTransposed[2];
	volumeInfo.extinction = g_volumeExtinction;

	memcpy(volumeDataPtr, &volumeInfo, sizeof(volumeInfo));


	// ray is in world space
	float3 ray = glm::normalize(g_lightDir);

	float3 localRay = ray;
	localRay.x = dot(volumeInfo.worldToLocal0, float4(ray, 0.0));
	localRay.y = dot(volumeInfo.worldToLocal1, float4(ray, 0.0));
	localRay.z = dot(volumeInfo.worldToLocal2, float4(ray, 0.0));

	float3 origin = lightInfo.position;
	float3 localOrigin = origin;
	localOrigin.x = dot(volumeInfo.worldToLocal0, float4(origin, 1.0));
	localOrigin.y = dot(volumeInfo.worldToLocal1, float4(origin, 1.0));
	localOrigin.z = dot(volumeInfo.worldToLocal2, float4(origin, 1.0));


	// find intersections with volume
	float intersect0;
	float intersect1;

	float tMin = 0.0f;
	float tMax = INFINITY;
	int hit = 1;
	for (int a = 0; a < 3; ++a)
	{
		float invD = 1.0f / localRay[a];
		float t0 = (-1.0f - localOrigin[a]) * invD;
		float t1 = (1.0f - localOrigin[a]) * invD;
		if (invD < 0.0f)
		{
			float tmp = t0;
			t0 = t1;
			t1 = tmp;
		}
		tMin = t0 > tMin ? t0 : tMin;
		tMax = t1 < tMax ? t1 : tMax;

		if (tMax <= tMin)
		{
			hit = 0;
			break;
		}
	}

	//float3 invDir = 1.0f / (localRay);
	//float3 origin = lightInfo.position;
	//float3 localOrigin = origin;
	////localOrigin.x = dot(volumeInfo.worldToLocal0, float4(origin, 1.0));
	////localOrigin.y = dot(volumeInfo.worldToLocal1, float4(origin, 1.0));
	////localOrigin.z = dot(volumeInfo.worldToLocal2, float4(origin, 1.0));
	//float3 originDivDir = origin * invDir;
	//const float3 tMin = -1.0 * invDir - originDivDir;
	//const float3 tMax = 1.0 * invDir - originDivDir;
	//const float3 tNear = min(tMin, tMax);
	//const float3 tFar = max(tMin, tMax);
	//t0 = glm::max(glm::max(tNear.x, tNear.y), tNear.z);
	//t1 = glm::min(glm::min(tFar.x, tFar.y), tFar.z);
	printf("%f %f %d\n", tMin, tMax, hit);


	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_fomImageViewHandle0), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_fomImageViewHandle1), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
	};

	graph.addPass("Fourier Opacity Map", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };

			PipelineColorBlendAttachmentState additiveBlendState{};
			additiveBlendState.m_blendEnable = true;
			additiveBlendState.m_srcColorBlendFactor = BlendFactor::ONE;
			additiveBlendState.m_dstColorBlendFactor = BlendFactor::ONE;
			additiveBlendState.m_colorBlendOp = BlendOp::ADD;
			additiveBlendState.m_srcAlphaBlendFactor = BlendFactor::ONE;
			additiveBlendState.m_dstAlphaBlendFactor = BlendFactor::ONE;
			additiveBlendState.m_alphaBlendOp = BlendOp::ADD;
			additiveBlendState.m_colorWriteMask = ColorComponentFlagBits::ALL_BITS;

			PipelineColorBlendAttachmentState colorBlendAttachments[]{ additiveBlendState,additiveBlendState };
			Format colorAttachmentFormats[]
			{
				registry.getImageView(data.m_fomImageViewHandle0)->getImage()->getDescription().m_format,
				registry.getImageView(data.m_fomImageViewHandle1)->getImage()->getDescription().m_format,
			};

			GraphicsPipelineCreateInfo pipelineCreateInfo;
			GraphicsPipelineBuilder builder(pipelineCreateInfo);
			builder.setVertexShader("Resources/Shaders/hlsl/fomVolume_vs.spv");
			builder.setFragmentShader("Resources/Shaders/hlsl/fomVolume_ps.spv");
			builder.setVertexBindingDescription({ 0, sizeof(float) * 3, VertexInputRate::VERTEX });
			builder.setVertexAttributeDescription({ 0, 0, Format::R32G32B32_SFLOAT, 0 });
			builder.setPolygonModeCullMode(PolygonMode::FILL, CullModeFlagBits::FRONT_BIT, FrontFace::COUNTER_CLOCKWISE);
			builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
			builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);
			builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			// begin renderpass
			ColorAttachmentDescription colorAttachDescs[]
			{
				{registry.getImageView(data.m_fomImageViewHandle0), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, {} },
				{registry.getImageView(data.m_fomImageViewHandle1), AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, {} },
			};
			cmdList->beginRenderPass(sizeof(colorAttachDescs) / sizeof(colorAttachDescs[0]), colorAttachDescs, nullptr, { {}, {128, 128} });

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageBuffer(&lightBufferInfo, LIGHT_INFO_BINDING),
					Initializers::storageBuffer(&volumeBufferInfo, VOLUME_INFO_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			Viewport viewport{ 0.0f, 0.0f, 128.0f, 128.0f, 0.0f, 1.0f };
			Rect scissor{ { 0, 0 }, { 128, 128 } };

			cmdList->setViewport(0, 1, &viewport);
			cmdList->setScissor(0, 1, &scissor);

			auto &renderResource = *data.m_passRecordContext->m_renderResources;

			cmdList->bindIndexBuffer(renderResource.m_lightProxyIndexBuffer, 0, IndexType::UINT16);

			uint64_t vertexOffset = 0;
			cmdList->bindVertexBuffers(0, 1, &renderResource.m_lightProxyVertexBuffer, &vertexOffset);

			const uint32_t boxProxyMeshIndexCount = renderResource.m_boxProxyMeshIndexCount;
			const uint32_t boxProxyMeshFirstIndex = renderResource.m_boxProxyMeshFirstIndex;
			const uint32_t boxProxyMeshVertexOffset = renderResource.m_boxProxyMeshVertexOffset;

			// participating media
			//for (size_t i = 0; i < data.m_localMediaCount; ++i)
			{
				PushConsts pushConsts;
				pushConsts.lightIndex = 0;
				pushConsts.volumeIndex = 0;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT | ShaderStageFlagBits::FRAGMENT_BIT, 0, sizeof(PushConsts), &pushConsts);
				cmdList->drawIndexed(boxProxyMeshIndexCount, 1, boxProxyMeshFirstIndex, boxProxyMeshVertexOffset, 0);
			}


			cmdList->endRenderPass();
		}, true);
}
