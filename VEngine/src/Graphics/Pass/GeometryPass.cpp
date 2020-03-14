#include "GeometryPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/Mesh.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	using namespace glm;
#include "../../../../Application/Resources/Shaders/geometry_bindings.h"
}

void VEngine::GeometryPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		//{ResourceViewHandle(data.m_indicesBufferHandle), ResourceState::READ_INDEX_BUFFER},
		//{ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::READ_INDIRECT_BUFFER},
		{rg::ResourceViewHandle(data.m_depthImageHandle), {gal::ResourceState::READ_WRITE_DEPTH_STENCIL, PipelineStageFlagBits::EARLY_FRAGMENT_TESTS_BIT | PipelineStageFlagBits::LATE_FRAGMENT_TESTS_BIT}},
		{rg::ResourceViewHandle(data.m_uvImageHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_ddxyLengthImageHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_ddxyRotMaterialIdImageHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(data.m_tangentSpaceImageHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
	};

	graph.addPass(data.m_alphaMasked ? "GBuffer Fill Alpha" : "GBuffer Fill", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		GraphicsPipelineCreateInfo pipelineCreateInfo;
		GraphicsPipelineBuilder builder(pipelineCreateInfo);

		gal::DynamicState dynamicState[] = { DynamicState::VIEWPORT,  DynamicState::SCISSOR };
		PipelineColorBlendAttachmentState colorBlendAttachments[]
		{
			GraphicsPipelineBuilder::s_defaultBlendAttachment,
			GraphicsPipelineBuilder::s_defaultBlendAttachment,
			GraphicsPipelineBuilder::s_defaultBlendAttachment,
			GraphicsPipelineBuilder::s_defaultBlendAttachment,
		};
		Format colorAttachmentFormats[]
		{
			registry.getImageView(data.m_uvImageHandle)->getImage()->getDescription().m_format,
			registry.getImageView(data.m_ddxyLengthImageHandle)->getImage()->getDescription().m_format,
			registry.getImageView(data.m_ddxyRotMaterialIdImageHandle)->getImage()->getDescription().m_format,
			registry.getImageView(data.m_tangentSpaceImageHandle)->getImage()->getDescription().m_format,
		};

		builder.setVertexShader("Resources/Shaders/geometry_vert.spv");
		builder.setFragmentShader(data.m_alphaMasked ? "Resources/Shaders/geometry_alpha_mask_frag.spv" : "Resources/Shaders/geometry_frag.spv");
		builder.setPolygonModeCullMode(PolygonMode::FILL, data.m_alphaMasked ? CullModeFlagBits::NONE : CullModeFlagBits::BACK_BIT, FrontFace::COUNTER_CLOCKWISE);
		builder.setDepthTest(true, true, CompareOp::GREATER_OR_EQUAL);
		builder.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
		builder.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
		builder.setDepthStencilAttachmentFormat(registry.getImageView(data.m_depthImageHandle)->getImage()->getDescription().m_format);
		builder.setColorAttachmentFormats(sizeof(colorAttachmentFormats) / sizeof(colorAttachmentFormats[0]), colorAttachmentFormats);

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		// begin renderpass
		DepthStencilAttachmentDescription depthAttachDesc =
		{ registry.getImageView(data.m_depthImageHandle), data.m_alphaMasked ? AttachmentLoadOp::LOAD : AttachmentLoadOp::CLEAR, AttachmentStoreOp::STORE, AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::DONT_CARE, { 0.0f, 0 } };
		ColorAttachmentDescription colorAttachDescs[]
		{
			{registry.getImageView(data.m_uvImageHandle), data.m_alphaMasked ? AttachmentLoadOp::LOAD : AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} },
			{registry.getImageView(data.m_ddxyLengthImageHandle), data.m_alphaMasked ? AttachmentLoadOp::LOAD : AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} },
			{registry.getImageView(data.m_ddxyRotMaterialIdImageHandle), data.m_alphaMasked ? AttachmentLoadOp::LOAD : AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} },
			{registry.getImageView(data.m_tangentSpaceImageHandle), data.m_alphaMasked ? AttachmentLoadOp::LOAD : AttachmentLoadOp::DONT_CARE, AttachmentStoreOp::STORE, {} },
		};
		cmdList->beginRenderPass(sizeof(colorAttachDescs) / sizeof(colorAttachDescs[0]), colorAttachDescs, &depthAttachDesc, { {}, {width, height} });

		cmdList->bindPipeline(pipeline);

		DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

		// update descriptor sets
		{
			Buffer *vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer;
			DescriptorBufferInfo positionsBufferInfo{ vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition) };
			DescriptorBufferInfo normalsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * sizeof(VertexNormal) };
			DescriptorBufferInfo texCoordsBufferInfo{ vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord) };

			DescriptorSetUpdate updates[] =
			{
				Initializers::storageBuffer(&positionsBufferInfo, VERTEX_POSITIONS_BINDING),
				Initializers::storageBuffer(&normalsBufferInfo, VERTEX_NORMALS_BINDING),
				Initializers::storageBuffer(&texCoordsBufferInfo, VERTEX_TEXCOORDS_BINDING),
				Initializers::storageBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),

				// if (data.m_alphaMasked)
				Initializers::storageBuffer(&data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING),
			};

			descriptorSet->update(data.m_alphaMasked ? 5 : 4, updates);
		}

		DescriptorSet *descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
		cmdList->bindDescriptorSets(pipeline, 0, data.m_alphaMasked ? 2 : 1, descriptorSets);

		Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		Rect scissor{ { 0, 0 }, { width, height } };

		cmdList->setViewport(0, 1, &viewport);
		cmdList->setScissor(0, 1, &scissor);

		cmdList->bindIndexBuffer(data.m_passRecordContext->m_renderResources->m_indexBuffer, 0, IndexType::UINT16);

		const glm::mat4 rowMajorViewMatrix = glm::transpose(data.m_passRecordContext->m_commonRenderData->m_viewMatrix);

		for (uint32_t i = 0; i < data.m_instanceDataCount; ++i)
		{
			const auto &instanceData = data.m_instanceData[i + data.m_instanceDataOffset];

			PushConsts pushConsts;
			pushConsts.jitteredViewProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredViewProjectionMatrix;
			pushConsts.viewMatrixRow0 = rowMajorViewMatrix[0];
			pushConsts.viewMatrixRow1 = rowMajorViewMatrix[1];
			pushConsts.viewMatrixRow2 = rowMajorViewMatrix[2];
			pushConsts.transformIndex = instanceData.m_transformIndex;
			pushConsts.materialIndex = instanceData.m_materialIndex;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

			const auto &subMeshInfo = data.m_subMeshInfo[instanceData.m_subMeshIndex];

			cmdList->drawIndexed(subMeshInfo.m_indexCount, 1, subMeshInfo.m_firstIndex, subMeshInfo.m_vertexOffset, 0);
		}

		//vkCmdDrawIndexedIndirect(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), 0, 1, sizeof(VkDrawIndexedIndirectCommand));

		cmdList->endRenderPass();
	});
}
