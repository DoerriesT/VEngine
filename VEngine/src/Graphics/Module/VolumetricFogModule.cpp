#include "VolumetricFogModule.h"
#include "Graphics/Pass/VolumetricFogVBufferPass.h"
#include "Graphics/Pass/VolumetricFogScatterPass.h"
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

		rg::ImageHandle inscatteringImageHandle = graph.importImage(m_inScatteringHistoryImages[commonData.m_curResIdx], "Volumetric Fog In-Scattering Image", false, {}, &m_inScatteringHistoryImageState[commonData.m_curResIdx]);
		inscatteringImageViewHandle = graph.createImageView({ "Volumetric Fog In-Scattering Image", inscatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });

		rg::ImageHandle prevInscatteringImageHandle = graph.importImage(m_inScatteringHistoryImages[commonData.m_prevResIdx], "Volumetric Fog Prev In-Scattering Image", false, {}, &m_inScatteringHistoryImageState[commonData.m_prevResIdx]);
		prevInscatteringImageViewHandle = graph.createImageView({ "Volumetric Fog Prev In-Scattering Image", prevInscatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });

		desc.m_name = "Volumetric Fog Scattering Image";
		m_volumetricScatteringImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });
	}

	const size_t haltonIdx = data.m_passRecordContext->m_commonRenderData->m_frame % s_haltonSampleCount;
	const float jitterX = m_haltonJitter[haltonIdx * 3 + 0];
	const float jitterY = m_haltonJitter[haltonIdx * 3 + 1];
	const float jitterZ = m_haltonJitter[haltonIdx * 3 + 2];

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
	volumetricFogVBufferPassData.m_jitter[0] = jitterX;
	volumetricFogVBufferPassData.m_jitter[1] = jitterY;
	volumetricFogVBufferPassData.m_jitter[2] = jitterZ;
	volumetricFogVBufferPassData.m_scatteringExtinctionImageViewHandle = vBufferScatteringExtinctionImageViewHandle;
	volumetricFogVBufferPassData.m_emissivePhaseImageViewHandle = vbufferEmissivePhasImageViewHandle;

	VolumetricFogVBufferPass::addToGraph(graph, volumetricFogVBufferPassData);


	// volumetric fog scatter
	VolumetricFogScatterPass::Data volumetricFogScatterPassData;
	volumetricFogScatterPassData.m_passRecordContext = data.m_passRecordContext;
	for (size_t i = 0; i < 4; ++i) memcpy(volumetricFogScatterPassData.m_frustumCorners[i], &frustumCorners[i], sizeof(float) * 3);
	volumetricFogScatterPassData.m_jitter[0] = jitterX;
	volumetricFogScatterPassData.m_jitter[1] = jitterY;
	volumetricFogScatterPassData.m_jitter[2] = jitterZ;
	memcpy(volumetricFogScatterPassData.m_reprojectedTexCoordScaleBias, &reprojectedTexCoordScaleBias, sizeof(volumetricFogScatterPassData.m_reprojectedTexCoordScaleBias));
	volumetricFogScatterPassData.m_lightData = data.m_lightData;
	volumetricFogScatterPassData.m_pointLightDataBufferInfo = data.m_pointLightDataBufferInfo;
	volumetricFogScatterPassData.m_pointLightZBinsBufferInfo = data.m_pointLightZBinsBufferInfo;
	volumetricFogScatterPassData.m_spotLightDataBufferInfo = data.m_spotLightDataBufferInfo;
	volumetricFogScatterPassData.m_spotLightZBinsBufferInfo = data.m_spotLightZBinsBufferInfo;
	volumetricFogScatterPassData.m_pointLightBitMaskBufferHandle = data.m_pointLightBitMaskBufferHandle;
	volumetricFogScatterPassData.m_spotLightBitMaskBufferHandle = data.m_spotLightBitMaskBufferHandle;
	volumetricFogScatterPassData.m_resultImageViewHandle = inscatteringImageViewHandle;
	volumetricFogScatterPassData.m_prevResultImageViewHandle = prevInscatteringImageViewHandle;
	volumetricFogScatterPassData.m_scatteringExtinctionImageViewHandle = vBufferScatteringExtinctionImageViewHandle;
	volumetricFogScatterPassData.m_emissivePhaseImageViewHandle = vbufferEmissivePhasImageViewHandle;
	volumetricFogScatterPassData.m_shadowImageViewHandle = data.m_shadowImageViewHandle;
	volumetricFogScatterPassData.m_shadowMatricesBufferInfo = data.m_shadowMatricesBufferInfo;

	VolumetricFogScatterPass::addToGraph(graph, volumetricFogScatterPassData);


	// volumetric fog integrate
	VolumetricFogIntegratePass::Data volumetricFogIntegratePassData;
	volumetricFogIntegratePassData.m_passRecordContext = data.m_passRecordContext;
	volumetricFogIntegratePassData.m_inputImageViewHandle = inscatteringImageViewHandle;
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
	}
}

VEngine::rg::ImageViewHandle VEngine::VolumetricFogModule::getVolumetricScatteringImageViewHandle()
{
	return m_volumetricScatteringImageViewHandle;
}
