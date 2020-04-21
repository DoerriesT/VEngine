#include "VolumetricFogModule.h"
#include "Graphics/Pass/VolumetricFogVBufferPass.h"
#include "Graphics/Pass/VolumetricFogScatterPass.h"
#include "Graphics/Pass/VolumetricFogFilterPass.h"
#include "Graphics/Pass/VolumetricFogIntegratePass.h"
#include "Graphics/RenderData.h"
#include "Utility/Utility.h"
#include "Graphics/PassRecordContext.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

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
		m_graphicsDevice->destroyImage(m_inScatteringHistoryImages[i]);
		m_graphicsDevice->destroyImage(m_inScatteringImages[i]);
	}
	delete[] m_haltonJitter;
}

void VEngine::VolumetricFogModule::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	auto &commonData = *data.m_commonData;

	const uint32_t imageWidth = (m_width + 7) / 8;
	const uint32_t imageHeight = (m_height + 7) / 8;
	const uint32_t imageDepth = 64;

	rg::ImageViewHandle vBufferScatteringExtinctionImageViewHandle;
	rg::ImageViewHandle vbufferEmissivePhasImageViewHandle;
	rg::ImageViewHandle inscatteringImageViewHandle;
	rg::ImageViewHandle prevFilteredInscatteringImageViewHandle;
	rg::ImageViewHandle filteredInscatteringImageViewHandle;
	rg::ImageViewHandle prevInscatteringImageViewHandle;
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

		desc.m_name = "Volumetric Fog V-Buffer Scattering/Extinction Image";
		vBufferScatteringExtinctionImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });

		desc.m_name = "Volumetric Fog V-Buffer Emissive/Phase Image";
		vbufferEmissivePhasImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });

		desc.m_name = "Volumetric Fog In-Scattering Image";
		inscatteringImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });

		rg::ImageHandle inscatteringImageHandle = graph.importImage(m_inScatteringImages[commonData.m_curResIdx], "Volumetric Fog In-Scattering Image", false, {}, &m_inScatteringImageState[commonData.m_curResIdx]);
		inscatteringImageViewHandle = graph.createImageView({ "Volumetric Fog In-Scattering Image", inscatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });

		rg::ImageHandle prevInscatteringImageHandle = graph.importImage(m_inScatteringImages[commonData.m_prevResIdx], "Volumetric Fog Prev In-Scattering Image", false, {}, &m_inScatteringImageState[commonData.m_prevResIdx]);
		prevInscatteringImageViewHandle = graph.createImageView({ "Volumetric Fog Prev In-Scattering Image", prevInscatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });

		rg::ImageHandle filteredInscatteringImageHandle = graph.importImage(m_inScatteringHistoryImages[commonData.m_curResIdx], "Volumetric Fog Filtered In-Scattering Image", false, {}, &m_inScatteringHistoryImageState[commonData.m_curResIdx]);
		filteredInscatteringImageViewHandle = graph.createImageView({ "Volumetric Fog Filtered In-Scattering Image", filteredInscatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });

		rg::ImageHandle prevFilteredInscatteringImageHandle = graph.importImage(m_inScatteringHistoryImages[commonData.m_prevResIdx], "Volumetric Fog Prev Filtered In-Scattering Image", false, {}, &m_inScatteringHistoryImageState[commonData.m_prevResIdx]);
		prevFilteredInscatteringImageViewHandle = graph.createImageView({ "Volumetric Fog Prev Filtered In-Scattering Image", prevFilteredInscatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });

		desc.m_name = "Volumetric Fog Scattering Image";
		m_volumetricScatteringImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });
	}

	const size_t haltonIdx0 = (data.m_passRecordContext->m_commonRenderData->m_frame * 2) % s_haltonSampleCount;
	const size_t haltonIdx1 = (data.m_passRecordContext->m_commonRenderData->m_frame * 2 + 1) % s_haltonSampleCount;
	const float jitterX0 = (m_haltonJitter[haltonIdx0 * 3 + 0]);// - 0.5f) * 0.5f + 0.25f;
	const float jitterY0 = (m_haltonJitter[haltonIdx0 * 3 + 1]);// - 0.5f) * 0.5f + 0.25f;
	const float jitterZ0 = (m_haltonJitter[haltonIdx0 * 3 + 2]);// - 0.5f) * 0.5f + 0.25f;
	const float jitterX1 = (m_haltonJitter[haltonIdx1 * 3 + 0]);// - 0.5f) * 0.5f + 0.25f;
	const float jitterY1 = (m_haltonJitter[haltonIdx1 * 3 + 1]);// - 0.5f) * 0.5f + 0.25f;
	const float jitterZ1 = (m_haltonJitter[haltonIdx1 * 3 + 2]);// - 0.5f) * 0.5f + 0.25f;

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


	// volumetric fog v-buffer
	VolumetricFogVBufferPass::Data volumetricFogVBufferPassData;
	volumetricFogVBufferPassData.m_passRecordContext = data.m_passRecordContext;
	for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogVBufferPassData.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	volumetricFogVBufferPassData.m_jitter[0] = jitterX0;
	volumetricFogVBufferPassData.m_jitter[1] = jitterY0;
	volumetricFogVBufferPassData.m_jitter[2] = jitterZ0;
	volumetricFogVBufferPassData.m_jitter[3] = jitterX1;
	volumetricFogVBufferPassData.m_jitter[4] = jitterY1;
	volumetricFogVBufferPassData.m_jitter[5] = jitterZ1;
	volumetricFogVBufferPassData.m_localMediaBufferInfo = data.m_localMediaBufferInfo;
	volumetricFogVBufferPassData.m_localMediaZBinsBufferInfo = data.m_localMediaZBinsBufferInfo;
	volumetricFogVBufferPassData.m_globalMediaBufferInfo = data.m_globalMediaBufferInfo;
	volumetricFogVBufferPassData.m_localMediaBitMaskBufferHandle = data.m_localMediaBitMaskBufferHandle;
	volumetricFogVBufferPassData.m_scatteringExtinctionImageViewHandle = vBufferScatteringExtinctionImageViewHandle;
	volumetricFogVBufferPassData.m_emissivePhaseImageViewHandle = vbufferEmissivePhasImageViewHandle;

	VolumetricFogVBufferPass::addToGraph(graph, volumetricFogVBufferPassData);


	// volumetric fog scatter
	VolumetricFogScatterPass::Data volumetricFogScatterPassData;
	volumetricFogScatterPassData.m_passRecordContext = data.m_passRecordContext;
	for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogScatterPassData.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	volumetricFogVBufferPassData.m_jitter[0] = jitterX0;
	volumetricFogVBufferPassData.m_jitter[1] = jitterY0;
	volumetricFogVBufferPassData.m_jitter[2] = jitterZ0;
	volumetricFogVBufferPassData.m_jitter[3] = jitterX1;
	volumetricFogVBufferPassData.m_jitter[4] = jitterY1;
	volumetricFogVBufferPassData.m_jitter[5] = jitterZ1;
	volumetricFogScatterPassData.m_directionalLightsBufferInfo = data.m_directionalLightsBufferInfo;
	volumetricFogScatterPassData.m_directionalLightsShadowedBufferInfo = data.m_directionalLightsShadowedBufferInfo;
	volumetricFogScatterPassData.m_punctualLightsBufferInfo = data.m_punctualLightsBufferInfo;
	volumetricFogScatterPassData.m_punctualLightsZBinsBufferInfo = data.m_punctualLightsZBinsBufferInfo;
	volumetricFogScatterPassData.m_punctualLightsShadowedBufferInfo = data.m_punctualLightsShadowedBufferInfo;
	volumetricFogScatterPassData.m_punctualLightsShadowedZBinsBufferInfo = data.m_punctualLightsShadowedZBinsBufferInfo;
	volumetricFogScatterPassData.m_punctualLightsBitMaskBufferHandle = data.m_punctualLightsBitMaskBufferHandle;
	volumetricFogScatterPassData.m_punctualLightsShadowedBitMaskBufferHandle = data.m_punctualLightsShadowedBitMaskBufferHandle;
	volumetricFogScatterPassData.m_exposureDataBufferHandle = data.m_exposureDataBufferHandle;
	volumetricFogScatterPassData.m_resultImageViewHandle = inscatteringImageViewHandle;
	volumetricFogScatterPassData.m_scatteringExtinctionImageViewHandle = vBufferScatteringExtinctionImageViewHandle;
	volumetricFogScatterPassData.m_emissivePhaseImageViewHandle = vbufferEmissivePhasImageViewHandle;
	volumetricFogScatterPassData.m_shadowImageViewHandle = data.m_shadowImageViewHandle;
	volumetricFogScatterPassData.m_shadowAtlasImageViewHandle = data.m_shadowAtlasImageViewHandle;
	volumetricFogScatterPassData.m_shadowMatricesBufferInfo = data.m_shadowMatricesBufferInfo;

	VolumetricFogScatterPass::addToGraph(graph, volumetricFogScatterPassData);


	// volumetric fog temporal filter
	VolumetricFogFilterPass::Data volumetricFogFilterPassData;
	volumetricFogFilterPassData.m_passRecordContext = data.m_passRecordContext;
	volumetricFogFilterPassData.m_ignoreHistory = data.m_ignoreHistory;
	for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogFilterPassData.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	memcpy(volumetricFogFilterPassData.m_reprojectedTexCoordScaleBias, &reprojectedTexCoordScaleBias, sizeof(volumetricFogFilterPassData.m_reprojectedTexCoordScaleBias));
	volumetricFogFilterPassData.m_exposureDataBufferHandle = data.m_exposureDataBufferHandle;
	volumetricFogFilterPassData.m_resultImageViewHandle = filteredInscatteringImageViewHandle;
	volumetricFogFilterPassData.m_inputImageViewHandle = inscatteringImageViewHandle;
	volumetricFogFilterPassData.m_historyImageViewHandle = prevFilteredInscatteringImageViewHandle;
	volumetricFogFilterPassData.m_prevImageViewHandle = prevInscatteringImageViewHandle;
	
	VolumetricFogFilterPass::addToGraph(graph, volumetricFogFilterPassData);


	// volumetric fog integrate
	VolumetricFogIntegratePass::Data volumetricFogIntegratePassData;
	volumetricFogIntegratePassData.m_passRecordContext = data.m_passRecordContext;
	volumetricFogIntegratePassData.m_inputImageViewHandle = filteredInscatteringImageViewHandle;
	volumetricFogIntegratePassData.m_resultImageViewHandle = m_volumetricScatteringImageViewHandle;

	VolumetricFogIntegratePass::addToGraph(graph, volumetricFogIntegratePassData);
}

void VEngine::VolumetricFogModule::resize(uint32_t width, uint32_t height)
{
	m_width = width;
	m_height = height;

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		if (m_inScatteringHistoryImages[i])
		{
			m_graphicsDevice->destroyImage(m_inScatteringHistoryImages[i]);
		}
		if (m_inScatteringImages[i])
		{
			m_graphicsDevice->destroyImage(m_inScatteringImages[i]);
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
		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_inScatteringHistoryImages[i]);

		m_inScatteringHistoryImageState[i] = {};

		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_inScatteringImages[i]);

		m_inScatteringImageState[i] = {};
	}
}

VEngine::rg::ImageViewHandle VEngine::VolumetricFogModule::getVolumetricScatteringImageViewHandle()
{
	return m_volumetricScatteringImageViewHandle;
}
