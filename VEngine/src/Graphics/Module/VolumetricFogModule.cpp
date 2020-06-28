#include "VolumetricFogModule.h"
#include "Graphics/Pass/VolumetricFogVBufferPass.h"
#include "Graphics/Pass/VolumetricFogScatterPass.h"
#include "Graphics/Pass/VolumetricFogMergedPass.h"
#include "Graphics/Pass/VolumetricFogFilterPass.h"
#include "Graphics/Pass/VolumetricFogIntegratePass.h"
#include "Graphics/Pass/VolumetricFogExtinctionVolumePass.h"
#include "Graphics/RenderData.h"
#include "Utility/Utility.h"
#include "Graphics/PassRecordContext.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

extern bool g_fogLookupDithering;
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
	}
	delete[] m_haltonJitter;
}

void VEngine::VolumetricFogModule::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	auto &commonData = *data.m_commonData;

	const uint32_t imageWidth = (m_width + 7) / 8;
	const uint32_t imageHeight = (m_height + 7) / 8;
	const uint32_t imageDepth = 64;

	rg::ImageViewHandle scatteringImageViewHandle;
	rg::ImageViewHandle prevScatteringImageViewHandle;
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

		rg::ImageHandle scatteringImageHandle = graph.importImage(m_scatteringImages[commonData.m_curResIdx], "Volumetric Fog Scattering Image", false, {}, &m_scatteringImageState[commonData.m_curResIdx]);
		scatteringImageViewHandle = graph.createImageView({ "Volumetric Fog Scattering Image", scatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });

		rg::ImageHandle prevScatteringImageHandle = graph.importImage(m_scatteringImages[commonData.m_prevResIdx], "Volumetric Fog Prev Scattering Image", false, {}, &m_scatteringImageState[commonData.m_prevResIdx]);
		prevScatteringImageViewHandle = graph.createImageView({ "Volumetric Fog Prev Scattering Image", prevScatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });
	}

	// extinction volume
	{
		rg::ImageViewHandle extinctionVolumeImageViewHandle;

		rg::ImageDescription desc = {};
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = 64;
		desc.m_height = 64;
		desc.m_depth = 64;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_imageType = ImageType::_3D;
		desc.m_format = Format::R16_SFLOAT;

		desc.m_name = "Volumetric Fog V-Buffer Extinction Volume Image";
		m_extinctionVolumeImageViewHandle = extinctionVolumeImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });


		VolumetricFogExtinctionVolumePass::Data extinctionVolumePassData;
		extinctionVolumePassData.m_passRecordContext = data.m_passRecordContext;
		extinctionVolumePassData.m_localMediaBufferInfo = data.m_localMediaBufferInfo;
		extinctionVolumePassData.m_globalMediaBufferInfo = data.m_globalMediaBufferInfo;
		extinctionVolumePassData.m_extinctionVolumeImageViewHandle = extinctionVolumeImageViewHandle;

		//VolumetricFogExtinctionVolumePass::addToGraph(graph, extinctionVolumePassData);
	}

	const size_t haltonIdx0 = (data.m_passRecordContext->m_commonRenderData->m_frame * 2) % s_haltonSampleCount;
	const size_t haltonIdx1 = (data.m_passRecordContext->m_commonRenderData->m_frame * 2 + 1) % s_haltonSampleCount;
	const float jitterX0 = g_fogLookupDithering ? (m_haltonJitter[haltonIdx0 * 3 + 0] - 0.5f) * 0.5f + 0.25f : m_haltonJitter[haltonIdx0 * 3 + 0];
	const float jitterY0 = g_fogLookupDithering ? (m_haltonJitter[haltonIdx0 * 3 + 1] - 0.5f) * 0.5f + 0.25f : m_haltonJitter[haltonIdx0 * 3 + 1];
	const float jitterZ0 = g_fogLookupDithering ? (m_haltonJitter[haltonIdx0 * 3 + 2] - 0.5f) * 0.5f + 0.25f : m_haltonJitter[haltonIdx0 * 3 + 2];
	const float jitterX1 = g_fogLookupDithering ? (m_haltonJitter[haltonIdx1 * 3 + 0] - 0.5f) * 0.5f + 0.25f : m_haltonJitter[haltonIdx1 * 3 + 0];
	const float jitterY1 = g_fogLookupDithering ? (m_haltonJitter[haltonIdx1 * 3 + 1] - 0.5f) * 0.5f + 0.25f : m_haltonJitter[haltonIdx1 * 3 + 1];
	const float jitterZ1 = g_fogLookupDithering ? (m_haltonJitter[haltonIdx1 * 3 + 2] - 0.5f) * 0.5f + 0.25f : m_haltonJitter[haltonIdx1 * 3 + 2];

	// we may need to expand the frustum a little to the right and a little downwards, so that each froxel corresponds
	// exactly to a single tile from the tiled lighting setup
	const float overSizeX = imageWidth * 8.0f / m_width - 1.0f;
	const float overSizeY = imageHeight * 8.0f / m_height - 1.0f;

	auto proj = glm::perspective(commonData.m_fovy, m_width / float(m_height), commonData.m_nearPlane, 64.0f);
	auto invViewProj = glm::inverse(proj * commonData.m_viewMatrix);
	glm::vec4 frustumCorners[4];
	frustumCorners[0] = invViewProj * glm::vec4(-1.0f					, 1.0f						, 1.0f, 1.0f); // top left
	frustumCorners[1] = invViewProj * glm::vec4(1.0f + 2.0f * overSizeX	, 1.0f						, 1.0f, 1.0f); // top right
	frustumCorners[2] = invViewProj * glm::vec4(-1.0f					, -1.0f - 2.0f * overSizeY	, 1.0f, 1.0f); // bottom left
	frustumCorners[3] = invViewProj * glm::vec4(1.0f + 2.0f * overSizeX	, -1.0f - 2.0f * overSizeY	, 1.0f, 1.0f); // bottom right

	for (auto &c : frustumCorners)
	{
		c /= c.w;
		c -= commonData.m_cameraPosition;
	}

	glm::vec4 reprojectedTexCoordScaleBias = glm::vec4(m_width / (imageWidth * 8.0f), m_height / (imageHeight * 8.0f), 0.0f, 0.0f);


	//// volumetric fog v-buffer
	//VolumetricFogVBufferPass::Data volumetricFogVBufferPassData;
	//volumetricFogVBufferPassData.m_passRecordContext = data.m_passRecordContext;
	//for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogVBufferPassData.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	//volumetricFogVBufferPassData.m_jitter[0] = jitterX0;
	//volumetricFogVBufferPassData.m_jitter[1] = jitterY0;
	//volumetricFogVBufferPassData.m_jitter[2] = jitterZ0;
	//volumetricFogVBufferPassData.m_jitter[3] = jitterX1;
	//volumetricFogVBufferPassData.m_jitter[4] = jitterY1;
	//volumetricFogVBufferPassData.m_jitter[5] = jitterZ1;
	//volumetricFogVBufferPassData.m_localMediaBufferInfo = data.m_localMediaBufferInfo;
	//volumetricFogVBufferPassData.m_localMediaZBinsBufferInfo = data.m_localMediaZBinsBufferInfo;
	//volumetricFogVBufferPassData.m_globalMediaBufferInfo = data.m_globalMediaBufferInfo;
	//volumetricFogVBufferPassData.m_localMediaBitMaskBufferHandle = data.m_localMediaBitMaskBufferHandle;
	//volumetricFogVBufferPassData.m_scatteringExtinctionImageViewHandle = vBufferScatteringExtinctionImageViewHandle;
	//volumetricFogVBufferPassData.m_emissivePhaseImageViewHandle = vbufferEmissivePhasImageViewHandle;
	//
	//VolumetricFogVBufferPass::addToGraph(graph, volumetricFogVBufferPassData);
	//
	//
	//// volumetric fog scatter
	//VolumetricFogScatterPass::Data volumetricFogScatterPassData;
	//volumetricFogScatterPassData.m_passRecordContext = data.m_passRecordContext;
	//for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogScatterPassData.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	//volumetricFogScatterPassData.m_jitter[0] = jitterX0;
	//volumetricFogScatterPassData.m_jitter[1] = jitterY0;
	//volumetricFogScatterPassData.m_jitter[2] = jitterZ0;
	//volumetricFogScatterPassData.m_jitter[3] = jitterX1;
	//volumetricFogScatterPassData.m_jitter[4] = jitterY1;
	//volumetricFogScatterPassData.m_jitter[5] = jitterZ1;
	//volumetricFogScatterPassData.m_directionalLightsBufferInfo = data.m_directionalLightsBufferInfo;
	//volumetricFogScatterPassData.m_directionalLightsShadowedBufferInfo = data.m_directionalLightsShadowedBufferInfo;
	//volumetricFogScatterPassData.m_punctualLightsBufferInfo = data.m_punctualLightsBufferInfo;
	//volumetricFogScatterPassData.m_punctualLightsZBinsBufferInfo = data.m_punctualLightsZBinsBufferInfo;
	//volumetricFogScatterPassData.m_punctualLightsShadowedBufferInfo = data.m_punctualLightsShadowedBufferInfo;
	//volumetricFogScatterPassData.m_punctualLightsShadowedZBinsBufferInfo = data.m_punctualLightsShadowedZBinsBufferInfo;
	//volumetricFogScatterPassData.m_punctualLightsBitMaskBufferHandle = data.m_punctualLightsBitMaskBufferHandle;
	//volumetricFogScatterPassData.m_punctualLightsShadowedBitMaskBufferHandle = data.m_punctualLightsShadowedBitMaskBufferHandle;
	//volumetricFogScatterPassData.m_exposureDataBufferHandle = data.m_exposureDataBufferHandle;
	//volumetricFogScatterPassData.m_resultImageViewHandle = inscatteringImageViewHandle;
	//volumetricFogScatterPassData.m_scatteringExtinctionImageViewHandle = vBufferScatteringExtinctionImageViewHandle;
	//volumetricFogScatterPassData.m_emissivePhaseImageViewHandle = vbufferEmissivePhasImageViewHandle;
	//volumetricFogScatterPassData.m_shadowImageViewHandle = data.m_shadowImageViewHandle;
	//volumetricFogScatterPassData.m_shadowAtlasImageViewHandle = data.m_shadowAtlasImageViewHandle;
	//volumetricFogScatterPassData.m_shadowMatricesBufferInfo = data.m_shadowMatricesBufferInfo;
	//volumetricFogScatterPassData.m_extinctionVolumeImageViewHandle = m_extinctionVolumeImageViewHandle;
	//volumetricFogScatterPassData.m_fomImageViewHandle = data.m_fomImageViewHandle;
	//
	//VolumetricFogScatterPass::addToGraph(graph, volumetricFogScatterPassData);


	// volumetric fog merged pass
	VolumetricFogMergedPass::Data volumetricFogMergedPassData;
	volumetricFogMergedPassData.m_passRecordContext = data.m_passRecordContext;
	volumetricFogMergedPassData.m_ignoreHistory = data.m_ignoreHistory;
	for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogMergedPassData.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	volumetricFogMergedPassData.m_jitter[0] = jitterX0;
	volumetricFogMergedPassData.m_jitter[1] = jitterY0;
	volumetricFogMergedPassData.m_jitter[2] = jitterZ0;
	volumetricFogMergedPassData.m_jitter[3] = jitterX1;
	volumetricFogMergedPassData.m_jitter[4] = jitterY1;
	volumetricFogMergedPassData.m_jitter[5] = jitterZ1;
	memcpy(volumetricFogMergedPassData.m_reprojectedTexCoordScaleBias, &reprojectedTexCoordScaleBias, sizeof(volumetricFogMergedPassData.m_reprojectedTexCoordScaleBias));
	volumetricFogMergedPassData.m_directionalLightsBufferInfo = data.m_directionalLightsBufferInfo;
	volumetricFogMergedPassData.m_directionalLightsShadowedBufferInfo = data.m_directionalLightsShadowedBufferInfo;
	volumetricFogMergedPassData.m_punctualLightsBufferInfo = data.m_punctualLightsBufferInfo;
	volumetricFogMergedPassData.m_punctualLightsZBinsBufferInfo = data.m_punctualLightsZBinsBufferInfo;
	volumetricFogMergedPassData.m_punctualLightsShadowedBufferInfo = data.m_punctualLightsShadowedBufferInfo;
	volumetricFogMergedPassData.m_punctualLightsShadowedZBinsBufferInfo = data.m_punctualLightsShadowedZBinsBufferInfo;
	volumetricFogMergedPassData.m_punctualLightsBitMaskBufferHandle = data.m_punctualLightsBitMaskBufferHandle;
	volumetricFogMergedPassData.m_punctualLightsShadowedBitMaskBufferHandle = data.m_punctualLightsShadowedBitMaskBufferHandle;
	volumetricFogMergedPassData.m_exposureDataBufferHandle = data.m_exposureDataBufferHandle;
	volumetricFogMergedPassData.m_resultImageViewHandle = scatteringImageViewHandle;
	volumetricFogMergedPassData.m_historyImageViewHandle = prevScatteringImageViewHandle;
	volumetricFogMergedPassData.m_shadowImageViewHandle = data.m_shadowImageViewHandle;
	volumetricFogMergedPassData.m_shadowAtlasImageViewHandle = data.m_shadowAtlasImageViewHandle;
	volumetricFogMergedPassData.m_shadowMatricesBufferInfo = data.m_shadowMatricesBufferInfo;
	volumetricFogMergedPassData.m_fomImageViewHandle = data.m_fomImageViewHandle;
	volumetricFogMergedPassData.m_localMediaBufferInfo = data.m_localMediaBufferInfo;
	volumetricFogMergedPassData.m_localMediaZBinsBufferInfo = data.m_localMediaZBinsBufferInfo;
	volumetricFogMergedPassData.m_globalMediaBufferInfo = data.m_globalMediaBufferInfo;
	volumetricFogMergedPassData.m_localMediaBitMaskBufferHandle = data.m_localMediaBitMaskBufferHandle;

	VolumetricFogMergedPass::addToGraph(graph, volumetricFogMergedPassData);


	//// volumetric fog temporal filter
	//VolumetricFogFilterPass::Data volumetricFogFilterPassData;
	//volumetricFogFilterPassData.m_passRecordContext = data.m_passRecordContext;
	//volumetricFogFilterPassData.m_ignoreHistory = data.m_ignoreHistory;
	//for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogFilterPassData.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	//memcpy(volumetricFogFilterPassData.m_reprojectedTexCoordScaleBias, &reprojectedTexCoordScaleBias, sizeof(volumetricFogFilterPassData.m_reprojectedTexCoordScaleBias));
	//volumetricFogFilterPassData.m_exposureDataBufferHandle = data.m_exposureDataBufferHandle;
	//volumetricFogFilterPassData.m_resultImageViewHandle = filteredInscatteringImageViewHandle;
	//volumetricFogFilterPassData.m_inputImageViewHandle = inscatteringImageViewHandle;
	//volumetricFogFilterPassData.m_historyImageViewHandle = prevFilteredInscatteringImageViewHandle;
	//volumetricFogFilterPassData.m_prevImageViewHandle = prevInscatteringImageViewHandle;
	//
	//VolumetricFogFilterPass::addToGraph(graph, volumetricFogFilterPassData);


	// volumetric fog integrate
	VolumetricFogIntegratePass::Data volumetricFogIntegratePassData;
	volumetricFogIntegratePassData.m_passRecordContext = data.m_passRecordContext;
	volumetricFogIntegratePassData.m_inputImageViewHandle = scatteringImageViewHandle;
	volumetricFogIntegratePassData.m_resultImageViewHandle = m_volumetricScatteringImageViewHandle;

	VolumetricFogIntegratePass::addToGraph(graph, volumetricFogIntegratePassData);
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
	}

	const uint32_t imageWidth = (m_width + 7) / 8;
	const uint32_t imageHeight = (m_height + 7) / 8;
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
	imageCreateInfo.m_usageFlags = ImageUsageFlagBits::STORAGE_BIT | ImageUsageFlagBits::SAMPLED_BIT;


	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_scatteringImages[i]);

		m_scatteringImageState[i] = {};
	}
}

VEngine::rg::ImageViewHandle VEngine::VolumetricFogModule::getVolumetricScatteringImageViewHandle()
{
	return m_volumetricScatteringImageViewHandle;
}

VEngine::rg::ImageViewHandle VEngine::VolumetricFogModule::getExtinctionVolumeImageViewHandle()
{
	return m_extinctionVolumeImageViewHandle;
}
