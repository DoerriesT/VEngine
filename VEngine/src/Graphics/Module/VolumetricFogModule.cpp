#include "VolumetricFogModule.h"
#include "Graphics/Pass/VolumetricFogMergedPass.h"
#include "Graphics/Pass/VolumetricFogFilterPass2.h"
#include "Graphics/Pass/VolumetricFogIntegratePass.h"
#include "Graphics/Pass/VolumetricRaymarchPass.h"
#include "Graphics/RenderData.h"
#include "Utility/Utility.h"
#include "Graphics/PassRecordContext.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

VEngine::VolumetricFogModule::VolumetricFogModule(gal::GraphicsDevice *graphicsDevice, uint32_t width, uint32_t height)
	:m_graphicsDevice(graphicsDevice),
	m_width(width),
	m_height(height),
	m_haltonJitter(new float[s_haltonSampleCount * 3]),
	m_volumetricScatteringImageViewHandle()
{
	for (size_t i = 0; i < s_haltonSampleCount; ++i)
	{
		m_haltonJitter[i * 3 + 0] = Utility::halton(i + 1, 2);
		m_haltonJitter[i * 3 + 1] = Utility::halton(i + 1, 3);
		m_haltonJitter[i * 3 + 2] = Utility::halton(i + 1, 5);
	}

	resize(width, height);
}

VEngine::VolumetricFogModule::~VolumetricFogModule()
{
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_graphicsDevice->destroyImage(m_scatteringImages[i]);
		m_graphicsDevice->destroyImage(m_raymarchedScatteringImages[i]);
		m_graphicsDevice->destroyImage(m_downsampledDepthImages[i]);
	}
	delete[] m_haltonJitter;
}

void VEngine::VolumetricFogModule::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	auto &commonData = *data.m_commonData;

	const uint32_t imageWidth = 160;// (m_width + 7) / 8;
	const uint32_t imageHeight = 90;// (m_height + 7) / 8;
	const uint32_t imageDepth = 64;

	rg::ImageViewHandle scatteringImageViewHandle;
	rg::ImageViewHandle prevScatteringImageViewHandle;
	rg::ImageViewHandle checkerboardImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = imageWidth;
		desc.m_height = imageHeight;
		desc.m_depth = imageDepth;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_imageType = ImageType::_3D;
		desc.m_format = Format::R16G16B16A16_SFLOAT;

		desc.m_name = "Volumetric Fog Integrated Scattering Image";
		m_volumetricScatteringImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });

		desc.m_name = "Volumetric Fog Checkerboard Scattering Image";
		desc.m_depth /= 2;
		checkerboardImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });

		rg::ImageHandle scatteringImageHandle = graph.importImage(m_scatteringImages[commonData.m_curResIdx], "Volumetric Fog Scattering Image", false, {}, &m_scatteringImageState[commonData.m_curResIdx]);
		scatteringImageViewHandle = graph.createImageView({ "Volumetric Fog Scattering Image", scatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });

		rg::ImageHandle prevScatteringImageHandle = graph.importImage(m_scatteringImages[commonData.m_prevResIdx], "Volumetric Fog Prev Scattering Image", false, {}, &m_scatteringImageState[commonData.m_prevResIdx]);
		prevScatteringImageViewHandle = graph.createImageView({ "Volumetric Fog Prev Scattering Image", prevScatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });
	}

	// raymarched
	{
		rg::ImageHandle imageHandle = graph.importImage(m_raymarchedScatteringImages[commonData.m_curResIdx], "Volumetric Fog Raymarched Scattering Image", false, {}, &m_raymarchedScatteringImageState[commonData.m_curResIdx]);
		m_raymarchedScatteringImageViewHandle = graph.createImageView({ "Volumetric Fog Raymarched Scattering Image", imageHandle, { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle downsampledDepthImageViewHandle;
	{
		rg::ImageHandle imageHandle = graph.importImage(m_downsampledDepthImages[commonData.m_curResIdx], "CB-Min/Max Downsampled Depth Image", false, {}, &m_downsampledDepthImageState[commonData.m_curResIdx]);
		m_downsampledDepthImageViewHandle = downsampledDepthImageViewHandle = graph.createImageView({ "CB-Min/Max Downsampled Depth Image", imageHandle, { 0, 1, 0, 1 } });
	}

	const size_t haltonIdx0 = (data.m_passRecordContext->m_commonRenderData->m_frame * 2) % s_haltonSampleCount;
	const size_t haltonIdx1 = (data.m_passRecordContext->m_commonRenderData->m_frame * 2 + 1) % s_haltonSampleCount;
	const float jitterX0 = m_haltonJitter[haltonIdx0 * 3 + 0];
	const float jitterY0 = m_haltonJitter[haltonIdx0 * 3 + 1];
	const float jitterZ0 = m_haltonJitter[haltonIdx0 * 3 + 2];
	const float jitterX1 = m_haltonJitter[haltonIdx1 * 3 + 0];
	const float jitterY1 = m_haltonJitter[haltonIdx1 * 3 + 1];
	const float jitterZ1 = m_haltonJitter[haltonIdx1 * 3 + 2];


	auto proj = glm::perspective(commonData.m_fovy, m_width / float(m_height), commonData.m_nearPlane, 64.0f);
	auto invViewProj = glm::inverse(proj * commonData.m_viewMatrix);
	glm::vec4 frustumCorners[4];
	frustumCorners[0] = invViewProj * glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f); // top left
	frustumCorners[1] = invViewProj * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // top right
	frustumCorners[2] = invViewProj * glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f); // bottom left
	frustumCorners[3] = invViewProj * glm::vec4(1.0f, -1.0f, 1.0f, 1.0f); // bottom right

	for (auto &c : frustumCorners)
	{
		c /= c.w;
		c -= commonData.m_cameraPosition;
	}

	// volumetric fog merged pass
	VolumetricFogMergedPass::Data volumetricFogMergedPassData;
	volumetricFogMergedPassData.m_passRecordContext = data.m_passRecordContext;
	for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogMergedPassData.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	volumetricFogMergedPassData.m_jitter[0] = jitterX0;
	volumetricFogMergedPassData.m_jitter[1] = jitterY0;
	volumetricFogMergedPassData.m_jitter[2] = jitterZ0;
	volumetricFogMergedPassData.m_jitter[3] = jitterX1;
	volumetricFogMergedPassData.m_jitter[4] = jitterY1;
	volumetricFogMergedPassData.m_jitter[5] = jitterZ1;
	volumetricFogMergedPassData.m_directionalLightsBufferInfo = data.m_directionalLightsBufferInfo;
	volumetricFogMergedPassData.m_directionalLightsShadowedBufferInfo = data.m_directionalLightsShadowedBufferInfo;
	volumetricFogMergedPassData.m_punctualLightsBufferInfo = data.m_punctualLightsBufferInfo;
	volumetricFogMergedPassData.m_punctualLightsZBinsBufferInfo = data.m_punctualLightsZBinsBufferInfo;
	volumetricFogMergedPassData.m_punctualLightsShadowedBufferInfo = data.m_punctualLightsShadowedBufferInfo;
	volumetricFogMergedPassData.m_punctualLightsShadowedZBinsBufferInfo = data.m_punctualLightsShadowedZBinsBufferInfo;
	volumetricFogMergedPassData.m_punctualLightsBitMaskImageViewHandle = data.m_punctualLightsBitMaskImageViewHandle;
	volumetricFogMergedPassData.m_punctualLightsShadowedBitMaskImageViewHandle = data.m_punctualLightsShadowedBitMaskImageViewHandle;
	volumetricFogMergedPassData.m_exposureDataBufferHandle = data.m_exposureDataBufferHandle;
	volumetricFogMergedPassData.m_resultImageViewHandle = checkerboardImageViewHandle;
	volumetricFogMergedPassData.m_shadowImageViewHandle = data.m_shadowImageViewHandle;
	volumetricFogMergedPassData.m_shadowAtlasImageViewHandle = data.m_shadowAtlasImageViewHandle;
	volumetricFogMergedPassData.m_shadowMatricesBufferInfo = data.m_shadowMatricesBufferInfo;
	volumetricFogMergedPassData.m_fomImageViewHandle = data.m_fomImageViewHandle;
	volumetricFogMergedPassData.m_directionalLightFOMImageViewHandle = data.m_directionalLightFOMImageViewHandle;
	volumetricFogMergedPassData.m_directionalLightFOMDepthRangeImageViewHandle = data.m_directionalLightFOMDepthRangeImageViewHandle;
	volumetricFogMergedPassData.m_depthImageViewHandle = data.m_depthImageViewHandle;
	volumetricFogMergedPassData.m_localMediaBufferInfo = data.m_localMediaBufferInfo;
	volumetricFogMergedPassData.m_localMediaZBinsBufferInfo = data.m_localMediaZBinsBufferInfo;
	volumetricFogMergedPassData.m_globalMediaBufferInfo = data.m_globalMediaBufferInfo;
	volumetricFogMergedPassData.m_localMediaBitMaskImageViewHandle = data.m_localMediaBitMaskImageViewHandle;

	VolumetricFogMergedPass::addToGraph(graph, volumetricFogMergedPassData);


	// volumetric fog temporal filter
	VolumetricFogFilterPass2::Data volumetricFogFilterPass2Data;
	volumetricFogFilterPass2Data.m_passRecordContext = data.m_passRecordContext;
	volumetricFogFilterPass2Data.m_ignoreHistory = data.m_ignoreHistory;
	for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogFilterPass2Data.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	volumetricFogFilterPass2Data.m_exposureDataBufferHandle = data.m_exposureDataBufferHandle;
	volumetricFogFilterPass2Data.m_resultImageViewHandle = scatteringImageViewHandle;
	volumetricFogFilterPass2Data.m_inputImageViewHandle = checkerboardImageViewHandle;
	volumetricFogFilterPass2Data.m_historyImageViewHandle = prevScatteringImageViewHandle;

	VolumetricFogFilterPass2::addToGraph(graph, volumetricFogFilterPass2Data);


	// volumetric fog integrate
	VolumetricFogIntegratePass::Data volumetricFogIntegratePassData;
	volumetricFogIntegratePassData.m_passRecordContext = data.m_passRecordContext;
	for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogIntegratePassData.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	volumetricFogIntegratePassData.m_inputImageViewHandle = scatteringImageViewHandle;
	volumetricFogIntegratePassData.m_resultImageViewHandle = m_volumetricScatteringImageViewHandle;

	VolumetricFogIntegratePass::addToGraph(graph, volumetricFogIntegratePassData);


	// downsample depth
	{
		rg::ResourceUsageDescription passUsages[]
		{
			{rg::ResourceViewHandle(downsampledDepthImageViewHandle), { gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
			{rg::ResourceViewHandle(data.m_depthImageViewHandle), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
		};

		graph.addPass("CB-Min/Max Downsample Depth", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
				const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

				// create pipeline description
				ComputePipelineCreateInfo pipelineCreateInfo;
				ComputePipelineBuilder builder(pipelineCreateInfo);
				builder.setComputeShader("Resources/Shaders/hlsl/depthDownsampleCBMinMax_cs");

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *resultImageView = registry.getImageView(downsampledDepthImageViewHandle);
					ImageView *depthImageView = registry.getImageView(data.m_depthImageViewHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::rwTexture(&resultImageView, 0),
						Initializers::texture(&depthImageView, 1),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);

					DescriptorSet *sets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };

					cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
				}

				struct PushConsts
				{
					glm::vec2 srcTexelSize;
					uint32_t width;
					uint32_t height;
				};

				PushConsts pushConsts;
				pushConsts.width = width / 2;
				pushConsts.height = height / 2;
				pushConsts.srcTexelSize = 1.0f / glm::vec2(width, height);

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				cmdList->dispatch((width / 2 + 7) / 8, (height / 2 + 7) / 8, 1);;
			}, true);
	}


	// volumetric raymarch
	VolumetricRaymarchPass::Data volumetricRaymarchPassData;
	volumetricRaymarchPassData.m_passRecordContext = data.m_passRecordContext;
	volumetricRaymarchPassData.m_blueNoiseImageView = data.m_blueNoiseImageView;
	volumetricRaymarchPassData.m_directionalLightsBufferInfo = data.m_directionalLightsBufferInfo;
	volumetricRaymarchPassData.m_directionalLightsShadowedBufferInfo = data.m_directionalLightsShadowedBufferInfo;
	volumetricRaymarchPassData.m_globalMediaBufferInfo = data.m_globalMediaBufferInfo;
	volumetricRaymarchPassData.m_shadowMatricesBufferInfo = data.m_shadowMatricesBufferInfo;
	volumetricRaymarchPassData.m_exposureDataBufferHandle = data.m_exposureDataBufferHandle;
	volumetricRaymarchPassData.m_resultImageViewHandle = m_raymarchedScatteringImageViewHandle;
	volumetricRaymarchPassData.m_depthImageViewHandle = downsampledDepthImageViewHandle;
	volumetricRaymarchPassData.m_shadowImageViewHandle = data.m_shadowImageViewHandle;

	VolumetricRaymarchPass::addToGraph(graph, volumetricRaymarchPassData);
}

void VEngine::VolumetricFogModule::resize(uint32_t width, uint32_t height)
{
	m_width = width;
	m_height = height;

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		if (m_scatteringImages[i])
		{
			m_graphicsDevice->destroyImage(m_scatteringImages[i]);
		}
		if (m_raymarchedScatteringImages[i])
		{
			m_graphicsDevice->destroyImage(m_raymarchedScatteringImages[i]);
		}
		if (m_downsampledDepthImages[i])
		{
			m_graphicsDevice->destroyImage(m_downsampledDepthImages[i]);
		}
	}

	const uint32_t imageWidth = 160;// (m_width + 7) / 8;
	const uint32_t imageHeight = 90;// (m_height + 7) / 8;
	const uint32_t imageDepth = 64;

	ImageCreateInfo imageCreateInfo{};
	imageCreateInfo.m_width = imageWidth;
	imageCreateInfo.m_height = imageHeight;
	imageCreateInfo.m_depth = imageDepth;
	imageCreateInfo.m_levels = 1;
	imageCreateInfo.m_layers = 1;
	imageCreateInfo.m_samples = SampleCount::_1;
	imageCreateInfo.m_imageType = ImageType::_3D;
	imageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;
	imageCreateInfo.m_createFlags = 0;
	imageCreateInfo.m_usageFlags = ImageUsageFlagBits::RW_TEXTURE_BIT | ImageUsageFlagBits::TEXTURE_BIT;


	ImageCreateInfo raymarchedImageCreateInfo{};
	raymarchedImageCreateInfo.m_width = m_width / 2;
	raymarchedImageCreateInfo.m_height = m_height / 2;
	raymarchedImageCreateInfo.m_depth = 1;
	raymarchedImageCreateInfo.m_levels = 1;
	raymarchedImageCreateInfo.m_layers = 1;
	raymarchedImageCreateInfo.m_samples = SampleCount::_1;
	raymarchedImageCreateInfo.m_imageType = ImageType::_2D;
	raymarchedImageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;
	raymarchedImageCreateInfo.m_createFlags = 0;
	raymarchedImageCreateInfo.m_usageFlags = ImageUsageFlagBits::RW_TEXTURE_BIT | ImageUsageFlagBits::TEXTURE_BIT;


	ImageCreateInfo downsampledDepthImageCreateInfo{};
	downsampledDepthImageCreateInfo.m_width = m_width / 2;
	downsampledDepthImageCreateInfo.m_height = m_height / 2;
	downsampledDepthImageCreateInfo.m_depth = 1;
	downsampledDepthImageCreateInfo.m_levels = 1;
	downsampledDepthImageCreateInfo.m_layers = 1;
	downsampledDepthImageCreateInfo.m_samples = SampleCount::_1;
	downsampledDepthImageCreateInfo.m_imageType = ImageType::_2D;
	downsampledDepthImageCreateInfo.m_format = Format::R32_SFLOAT;
	downsampledDepthImageCreateInfo.m_createFlags = 0;
	downsampledDepthImageCreateInfo.m_usageFlags = ImageUsageFlagBits::RW_TEXTURE_BIT | ImageUsageFlagBits::TEXTURE_BIT;


	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_scatteringImages[i]);
		m_graphicsDevice->createImage(raymarchedImageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_raymarchedScatteringImages[i]);
		m_graphicsDevice->createImage(downsampledDepthImageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_downsampledDepthImages[i]);

		m_scatteringImageState[i] = {};
		m_raymarchedScatteringImageState[i] = {};
		m_downsampledDepthImageState[i] = {};
	}
}

VEngine::rg::ImageViewHandle VEngine::VolumetricFogModule::getVolumetricScatteringImageViewHandle()
{
	return m_volumetricScatteringImageViewHandle;
}

VEngine::rg::ImageViewHandle VEngine::VolumetricFogModule::getRaymarchedScatteringImageViewHandle()
{
	return m_raymarchedScatteringImageViewHandle;
}

VEngine::rg::ImageViewHandle VEngine::VolumetricFogModule::getDownsampledDepthImageViewHandle()
{
	return m_downsampledDepthImageViewHandle;
}
