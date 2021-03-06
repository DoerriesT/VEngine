#include "AtmosphericScatteringModule.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include <glm/vec3.hpp>
#include "GlobalVar.h"
#include "Graphics/gal/Initializers.h"

#define TRANSMITTANCE_TEXTURE_WIDTH 256
#define TRANSMITTANCE_TEXTURE_HEIGHT 64

#define SCATTERING_TEXTURE_R_SIZE 32
#define SCATTERING_TEXTURE_MU_SIZE 128
#define SCATTERING_TEXTURE_MU_S_SIZE 32
#define SCATTERING_TEXTURE_NU_SIZE 8

#define SCATTERING_TEXTURE_WIDTH (SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE)
#define SCATTERING_TEXTURE_HEIGHT SCATTERING_TEXTURE_MU_SIZE
#define SCATTERING_TEXTURE_DEPTH SCATTERING_TEXTURE_R_SIZE

#define IRRADIANCE_TEXTURE_WIDTH 64
#define IRRADIANCE_TEXTURE_HEIGHT 16

// The conversion factor between watts and lumens.
#define MAX_LUMINOUS_EFFICACY 683.0

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/commonAtmosphericScatteringTypes.hlsli"
}

namespace
{
	constexpr float kLambdaR = 680.0f;
	constexpr float kLambdaG = 550.0f;
	constexpr float kLambdaB = 440.0f;
	constexpr int kLambdaMin = 360;
	constexpr int kLambdaMax = 830;

	// Values from "CIE (1931) 2-deg color matching functions", see
// "http://web.archive.org/web/20081228084047/
//    http://www.cvrl.org/database/data/cmfs/ciexyz31.txt".
	constexpr double CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[380] = {
	  360, 0.000129900000, 0.000003917000, 0.000606100000,
	  365, 0.000232100000, 0.000006965000, 0.001086000000,
	  370, 0.000414900000, 0.000012390000, 0.001946000000,
	  375, 0.000741600000, 0.000022020000, 0.003486000000,
	  380, 0.001368000000, 0.000039000000, 0.006450001000,
	  385, 0.002236000000, 0.000064000000, 0.010549990000,
	  390, 0.004243000000, 0.000120000000, 0.020050010000,
	  395, 0.007650000000, 0.000217000000, 0.036210000000,
	  400, 0.014310000000, 0.000396000000, 0.067850010000,
	  405, 0.023190000000, 0.000640000000, 0.110200000000,
	  410, 0.043510000000, 0.001210000000, 0.207400000000,
	  415, 0.077630000000, 0.002180000000, 0.371300000000,
	  420, 0.134380000000, 0.004000000000, 0.645600000000,
	  425, 0.214770000000, 0.007300000000, 1.039050100000,
	  430, 0.283900000000, 0.011600000000, 1.385600000000,
	  435, 0.328500000000, 0.016840000000, 1.622960000000,
	  440, 0.348280000000, 0.023000000000, 1.747060000000,
	  445, 0.348060000000, 0.029800000000, 1.782600000000,
	  450, 0.336200000000, 0.038000000000, 1.772110000000,
	  455, 0.318700000000, 0.048000000000, 1.744100000000,
	  460, 0.290800000000, 0.060000000000, 1.669200000000,
	  465, 0.251100000000, 0.073900000000, 1.528100000000,
	  470, 0.195360000000, 0.090980000000, 1.287640000000,
	  475, 0.142100000000, 0.112600000000, 1.041900000000,
	  480, 0.095640000000, 0.139020000000, 0.812950100000,
	  485, 0.057950010000, 0.169300000000, 0.616200000000,
	  490, 0.032010000000, 0.208020000000, 0.465180000000,
	  495, 0.014700000000, 0.258600000000, 0.353300000000,
	  500, 0.004900000000, 0.323000000000, 0.272000000000,
	  505, 0.002400000000, 0.407300000000, 0.212300000000,
	  510, 0.009300000000, 0.503000000000, 0.158200000000,
	  515, 0.029100000000, 0.608200000000, 0.111700000000,
	  520, 0.063270000000, 0.710000000000, 0.078249990000,
	  525, 0.109600000000, 0.793200000000, 0.057250010000,
	  530, 0.165500000000, 0.862000000000, 0.042160000000,
	  535, 0.225749900000, 0.914850100000, 0.029840000000,
	  540, 0.290400000000, 0.954000000000, 0.020300000000,
	  545, 0.359700000000, 0.980300000000, 0.013400000000,
	  550, 0.433449900000, 0.994950100000, 0.008749999000,
	  555, 0.512050100000, 1.000000000000, 0.005749999000,
	  560, 0.594500000000, 0.995000000000, 0.003900000000,
	  565, 0.678400000000, 0.978600000000, 0.002749999000,
	  570, 0.762100000000, 0.952000000000, 0.002100000000,
	  575, 0.842500000000, 0.915400000000, 0.001800000000,
	  580, 0.916300000000, 0.870000000000, 0.001650001000,
	  585, 0.978600000000, 0.816300000000, 0.001400000000,
	  590, 1.026300000000, 0.757000000000, 0.001100000000,
	  595, 1.056700000000, 0.694900000000, 0.001000000000,
	  600, 1.062200000000, 0.631000000000, 0.000800000000,
	  605, 1.045600000000, 0.566800000000, 0.000600000000,
	  610, 1.002600000000, 0.503000000000, 0.000340000000,
	  615, 0.938400000000, 0.441200000000, 0.000240000000,
	  620, 0.854449900000, 0.381000000000, 0.000190000000,
	  625, 0.751400000000, 0.321000000000, 0.000100000000,
	  630, 0.642400000000, 0.265000000000, 0.000049999990,
	  635, 0.541900000000, 0.217000000000, 0.000030000000,
	  640, 0.447900000000, 0.175000000000, 0.000020000000,
	  645, 0.360800000000, 0.138200000000, 0.000010000000,
	  650, 0.283500000000, 0.107000000000, 0.000000000000,
	  655, 0.218700000000, 0.081600000000, 0.000000000000,
	  660, 0.164900000000, 0.061000000000, 0.000000000000,
	  665, 0.121200000000, 0.044580000000, 0.000000000000,
	  670, 0.087400000000, 0.032000000000, 0.000000000000,
	  675, 0.063600000000, 0.023200000000, 0.000000000000,
	  680, 0.046770000000, 0.017000000000, 0.000000000000,
	  685, 0.032900000000, 0.011920000000, 0.000000000000,
	  690, 0.022700000000, 0.008210000000, 0.000000000000,
	  695, 0.015840000000, 0.005723000000, 0.000000000000,
	  700, 0.011359160000, 0.004102000000, 0.000000000000,
	  705, 0.008110916000, 0.002929000000, 0.000000000000,
	  710, 0.005790346000, 0.002091000000, 0.000000000000,
	  715, 0.004109457000, 0.001484000000, 0.000000000000,
	  720, 0.002899327000, 0.001047000000, 0.000000000000,
	  725, 0.002049190000, 0.000740000000, 0.000000000000,
	  730, 0.001439971000, 0.000520000000, 0.000000000000,
	  735, 0.000999949300, 0.000361100000, 0.000000000000,
	  740, 0.000690078600, 0.000249200000, 0.000000000000,
	  745, 0.000476021300, 0.000171900000, 0.000000000000,
	  750, 0.000332301100, 0.000120000000, 0.000000000000,
	  755, 0.000234826100, 0.000084800000, 0.000000000000,
	  760, 0.000166150500, 0.000060000000, 0.000000000000,
	  765, 0.000117413000, 0.000042400000, 0.000000000000,
	  770, 0.000083075270, 0.000030000000, 0.000000000000,
	  775, 0.000058706520, 0.000021200000, 0.000000000000,
	  780, 0.000041509940, 0.000014990000, 0.000000000000,
	  785, 0.000029353260, 0.000010600000, 0.000000000000,
	  790, 0.000020673830, 0.000007465700, 0.000000000000,
	  795, 0.000014559770, 0.000005257800, 0.000000000000,
	  800, 0.000010253980, 0.000003702900, 0.000000000000,
	  805, 0.000007221456, 0.000002607800, 0.000000000000,
	  810, 0.000005085868, 0.000001836600, 0.000000000000,
	  815, 0.000003581652, 0.000001293400, 0.000000000000,
	  820, 0.000002522525, 0.000000910930, 0.000000000000,
	  825, 0.000001776509, 0.000000641530, 0.000000000000,
	  830, 0.000001251141, 0.000000451810, 0.000000000000,
	};

	// The conversion matrix from XYZ to linear sRGB color spaces.
	// Values from https://en.wikipedia.org/wiki/SRGB.
	constexpr double XYZ_TO_SRGB[9] = {
	  +3.2406, -1.5372, -0.4986,
	  -0.9689, +1.8758, +0.0415,
	  +0.0557, -0.2040, +1.0570
	};

	double CieColorMatchingFunctionTableValue(double wavelength, int column) {
		if (wavelength <= kLambdaMin || wavelength >= kLambdaMax) {
			return 0.0;
		}
		double u = (wavelength - kLambdaMin) / 5.0;
		int row = static_cast<int>(std::floor(u));
		assert(row >= 0 && row + 1 < 95);
		assert(CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * row] <= wavelength &&
			CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * (row + 1)] >= wavelength);
		u -= row;
		return CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * row + column] * (1.0 - u) +
			CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * (row + 1) + column] * u;
	}

	double Interpolate(
		const std::vector<double> &wavelengths,
		const std::vector<double> &wavelength_function,
		double wavelength) {
		assert(wavelength_function.size() == wavelengths.size());
		if (wavelength < wavelengths[0]) {
			return wavelength_function[0];
		}
		for (unsigned int i = 0; i < wavelengths.size() - 1; ++i) {
			if (wavelength < wavelengths[i + 1]) {
				double u =
					(wavelength - wavelengths[i]) / (wavelengths[i + 1] - wavelengths[i]);
				return
					wavelength_function[i] * (1.0 - u) + wavelength_function[i + 1] * u;
			}
		}
		return wavelength_function[wavelength_function.size() - 1];
	}

	// The returned constants are in lumen.nm / watt.
	void ComputeSpectralRadianceToLuminanceFactors(
		const std::vector<double> &wavelengths,
		const std::vector<double> &solar_irradiance,
		double lambda_power, double *k_r, double *k_g, double *k_b) {
		*k_r = 0.0;
		*k_g = 0.0;
		*k_b = 0.0;
		double solar_r = Interpolate(wavelengths, solar_irradiance, kLambdaR);
		double solar_g = Interpolate(wavelengths, solar_irradiance, kLambdaG);
		double solar_b = Interpolate(wavelengths, solar_irradiance, kLambdaB);
		int dlambda = 1;
		for (int lambda = kLambdaMin; lambda < kLambdaMax; lambda += dlambda) {
			double x_bar = CieColorMatchingFunctionTableValue(lambda, 1);
			double y_bar = CieColorMatchingFunctionTableValue(lambda, 2);
			double z_bar = CieColorMatchingFunctionTableValue(lambda, 3);
			const double *xyz2srgb = XYZ_TO_SRGB;
			double r_bar =
				xyz2srgb[0] * x_bar + xyz2srgb[1] * y_bar + xyz2srgb[2] * z_bar;
			double g_bar =
				xyz2srgb[3] * x_bar + xyz2srgb[4] * y_bar + xyz2srgb[5] * z_bar;
			double b_bar =
				xyz2srgb[6] * x_bar + xyz2srgb[7] * y_bar + xyz2srgb[8] * z_bar;
			double irradiance = Interpolate(wavelengths, solar_irradiance, lambda);
			*k_r += r_bar * irradiance / solar_r *
				pow(lambda / kLambdaR, lambda_power);
			*k_g += g_bar * irradiance / solar_g *
				pow(lambda / kLambdaG, lambda_power);
			*k_b += b_bar * irradiance / solar_b *
				pow(lambda / kLambdaB, lambda_power);
		}
		*k_r *= MAX_LUMINOUS_EFFICACY * dlambda;
		*k_g *= MAX_LUMINOUS_EFFICACY * dlambda;
		*k_b *= MAX_LUMINOUS_EFFICACY * dlambda;
	}
}

VEngine::AtmosphericScatteringModule::AtmosphericScatteringModule(gal::GraphicsDevice *graphicsDevice)
	:m_graphicsDevice(graphicsDevice)
{
	// transmittance
	{
		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = TRANSMITTANCE_TEXTURE_WIDTH;
		imageCreateInfo.m_height = TRANSMITTANCE_TEXTURE_HEIGHT;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = Format::R32G32B32A32_SFLOAT;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::RW_TEXTURE_BIT | ImageUsageFlagBits::TEXTURE_BIT;
		imageCreateInfo.m_optimizedClearValue = {};

		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, true, &m_transmittanceImage);
	}

	// scattering
	{
		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = SCATTERING_TEXTURE_WIDTH;
		imageCreateInfo.m_height = SCATTERING_TEXTURE_HEIGHT;
		imageCreateInfo.m_depth = SCATTERING_TEXTURE_DEPTH;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_3D;
		imageCreateInfo.m_format = Format::R32G32B32A32_SFLOAT;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::RW_TEXTURE_BIT | ImageUsageFlagBits::TEXTURE_BIT;
		imageCreateInfo.m_optimizedClearValue = {};

		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, true, &m_scatteringImage);
	}

	// irradiance
	{
		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = IRRADIANCE_TEXTURE_WIDTH;
		imageCreateInfo.m_height = IRRADIANCE_TEXTURE_HEIGHT;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = Format::R32G32B32A32_SFLOAT;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::RW_TEXTURE_BIT | ImageUsageFlagBits::TEXTURE_BIT;
		imageCreateInfo.m_optimizedClearValue = {};

		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, true, &m_irradianceImage);
	}

	// constant buffer
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = sizeof(AtmosphereParameters);
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::CONSTANT_BUFFER_BIT;

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::HOST_COHERENT_BIT | MemoryPropertyFlagBits::HOST_VISIBLE_BIT, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, false, &m_constantBuffer);
	}

	AtmosphereParameters params{};
	params.solar_irradiance = float3(1.474, 1.8504, 1.91198); // TODO
	params.sun_angular_radius = 0.004675;
	params.bottom_radius = 6360000.0;
	params.top_radius = 6420000.0;
	params.rayleigh_density = { {{0.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 1.0, -1.25E-4, 0.0, 0.0}} };
	params.rayleigh_scattering = float3(5.802339381712381E-6, 1.355776244792022E-5, 3.3100005976367735E-5);
	params.mie_density = { {{0.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 1.0, -8.333333333333334E-4, 0.0, 0.0}} };
	params.mie_scattering = float3(3.996E-6, 3.996E-6, 3.996E-6) * 1.0f;
	params.mie_phase_function_g = 0.875;
	params.absorption_density = { {{25000.0, 0.0, 0.0, 6.666666666666667E-5, -0.6666666666666666}, {0.0, 0.0, 0.0, -6.666666666666667E-5, 2.6666666666666665}} };
	params.absorption_extinction = float3(6.497166E-7, 1.8809E-6, 8.501667999999999E-8);
	params.ground_albedo = float3(0.1, 0.1, 0.1);;
	params.mu_s_min = -0.5;

	// copy to constant buffer
	AtmosphereParameters *data;
	m_constantBuffer->map((void **)&data);
	*data = params;
	m_constantBuffer->unmap();


	constexpr double kSolarIrradiance[48] = {
	1.11776, 1.14259, 1.01249, 1.14716, 1.72765, 1.73054, 1.6887, 1.61253,
	1.91198, 2.03474, 2.02042, 2.02212, 1.93377, 1.95809, 1.91686, 1.8298,
	1.8685, 1.8931, 1.85149, 1.8504, 1.8341, 1.8345, 1.8147, 1.78158, 1.7533,
	1.6965, 1.68194, 1.64654, 1.6048, 1.52143, 1.55622, 1.5113, 1.474, 1.4482,
	1.41018, 1.36775, 1.34188, 1.31429, 1.28303, 1.26758, 1.2367, 1.2082,
	1.18737, 1.14683, 1.12362, 1.1058, 1.07124, 1.04992
	};

	std::vector<double> solarIrradiance = { 1.474, 1.8504, 1.91198 };

	std::vector<double> wavelengths;
	std::vector<double> solar_irradiance;
	for (int l = kLambdaMin; l <= kLambdaMax; l += 10) {
		wavelengths.push_back(l);
		solar_irradiance.push_back(kSolarIrradiance[(l - kLambdaMin) / 10]);
	}

	double sky_k_r, sky_k_g, sky_k_b;
	ComputeSpectralRadianceToLuminanceFactors(wavelengths, solar_irradiance,
		-3 /* lambda_power */, &sky_k_r, &sky_k_g, &sky_k_b);

	double sun_k_r, sun_k_g, sun_k_b;
	ComputeSpectralRadianceToLuminanceFactors(wavelengths, solar_irradiance,
		0 /* lambda_power */, &sun_k_r, &sun_k_g, &sun_k_b);
}

VEngine::AtmosphericScatteringModule::~AtmosphericScatteringModule()
{
	m_graphicsDevice->destroyImage(m_transmittanceImage);
	m_graphicsDevice->destroyImage(m_scatteringImage);
	m_graphicsDevice->destroyImage(m_irradianceImage);
	m_graphicsDevice->destroyBuffer(m_constantBuffer);
}

void VEngine::AtmosphericScatteringModule::addPrecomputationToGraph(rg::RenderGraph &graph, const Data &data)
{
	// import resources
	rg::ImageViewHandle transmittanceImageViewHandle = m_transmittanceImageViewHandle;
	rg::ImageViewHandle scatteringImageViewHandle = m_scatteringImageViewHandle;
	rg::ImageViewHandle irradianceImageViewHandle;
	{
		//rg::ImageHandle transmittanceImageHandle = graph.importImage(m_transmittanceImage, "AS Transmittance Image", false, {}, &m_transmittanceImageState);
		//transmittanceImageViewHandle = graph.createImageView({ "AS Transmittance Image", transmittanceImageHandle, { 0, 1, 0, 1 }, ImageViewType::_2D });
		//
		//rg::ImageHandle scatteringImageHandle = graph.importImage(m_scatteringImage, "AS Scattering Image", false, {}, &m_scatteringImageState);
		//scatteringImageViewHandle = graph.createImageView({ "AS Scattering Image", scatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });

		rg::ImageHandle irradianceImageHandle = graph.importImage(m_irradianceImage, "AS Irradiance Image", false, {}, &m_irradianceImageState);
		irradianceImageViewHandle = graph.createImageView({ "AS Irradiance Image", irradianceImageHandle, { 0, 1, 0, 1 }, ImageViewType::_2D });
	}

	// create temporary graph managed resources
	rg::ImageViewHandle deltaIrradianceImageViewHandle;
	rg::ImageViewHandle deltaRayleighScatteringImageViewHandle;
	rg::ImageViewHandle deltaMieScatteringImageViewHandle;
	rg::ImageViewHandle deltaScatteringDensityImageViewHandle;
	rg::ImageViewHandle deltaMultipleScatteringImageViewHandle;
	{
		// delta irradiance image
		{
			rg::ImageDescription desc = {};
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = IRRADIANCE_TEXTURE_WIDTH;
			desc.m_height = IRRADIANCE_TEXTURE_HEIGHT;
			desc.m_depth = 1;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = SampleCount::_1;
			desc.m_imageType = ImageType::_2D;
			desc.m_format = Format::R32G32B32A32_SFLOAT;
			desc.m_name = "AS Delta Irradiance Image";

			deltaIrradianceImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_2D });
		}
		
		// delta scattering images
		{
			rg::ImageDescription desc = {};
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = SCATTERING_TEXTURE_WIDTH;
			desc.m_height = SCATTERING_TEXTURE_HEIGHT;
			desc.m_depth = SCATTERING_TEXTURE_DEPTH;
			desc.m_layers = 1;
			desc.m_levels = 1;
			desc.m_samples = SampleCount::_1;
			desc.m_imageType = ImageType::_3D;
			desc.m_format = Format::R32G32B32A32_SFLOAT;
			
			desc.m_name = "AS Delta Rayleigh Scattering Image";
			deltaMultipleScatteringImageViewHandle = deltaRayleighScatteringImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });
		
			desc.m_name = "AS Delta Mie Scattering Image";
			deltaMieScatteringImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });

			desc.m_name = "AS Delta Scattering Density Image";
			deltaScatteringDensityImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 }, ImageViewType::_3D });
		}
	}

	DescriptorBufferInfo constantBufferInfo = { m_constantBuffer, 0, sizeof(AtmosphereParameters) };

	// compute transmittance
	{
		rg::ResourceUsageDescription passUsages[]
		{
			{rg::ResourceViewHandle(transmittanceImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		};

		graph.addPass("AS Transmittance", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				// create pipeline description
				ComputePipelineCreateInfo pipelineCreateInfo;
				ComputePipelineBuilder builder(pipelineCreateInfo);
				builder.setComputeShader("Resources/Shaders/hlsl/atmosphereComputeTransmittance_cs");

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *transmittanceImageView = registry.getImageView(transmittanceImageViewHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::rwTexture(&transmittanceImageView, 0),
						Initializers::constantBuffer(&constantBufferInfo, 1),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);

					cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
				}

				cmdList->dispatch((TRANSMITTANCE_TEXTURE_WIDTH + 7) / 8, (TRANSMITTANCE_TEXTURE_HEIGHT + 7) / 8, 1);
			}, true);
	}

	// compute direct irradiance
	{
		rg::ResourceUsageDescription passUsages[]
		{
			{rg::ResourceViewHandle(deltaIrradianceImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
			{rg::ResourceViewHandle(irradianceImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
			{rg::ResourceViewHandle(transmittanceImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		};

		graph.addPass("AS Direct Irradiance", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				// create pipeline description
				ComputePipelineCreateInfo pipelineCreateInfo;
				ComputePipelineBuilder builder(pipelineCreateInfo);
				builder.setComputeShader("Resources/Shaders/hlsl/atmosphereComputeDirectIrradiance_cs");

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *deltaIrrImageView = registry.getImageView(deltaIrradianceImageViewHandle);
					ImageView *irrImageView = registry.getImageView(irradianceImageViewHandle);
					ImageView *transmittanceImageView = registry.getImageView(transmittanceImageViewHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::rwTexture(&deltaIrrImageView, 0),
						Initializers::rwTexture(&irrImageView, 1),
						Initializers::texture(&transmittanceImageView, 2),
						Initializers::constantBuffer(&constantBufferInfo, 4),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);

					DescriptorSet *sets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
				}

				cmdList->dispatch((IRRADIANCE_TEXTURE_WIDTH + 7) / 8, (IRRADIANCE_TEXTURE_HEIGHT + 7) / 8, 1);
			}, true);
	}

	// compute rayleigh and mie single scattering
	{
		rg::ResourceUsageDescription passUsages[]
		{
			{rg::ResourceViewHandle(deltaRayleighScatteringImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
			{rg::ResourceViewHandle(deltaMieScatteringImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
			{rg::ResourceViewHandle(scatteringImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
			{rg::ResourceViewHandle(transmittanceImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		};

		graph.addPass("AS Single Scattering", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
			{
				// create pipeline description
				ComputePipelineCreateInfo pipelineCreateInfo;
				ComputePipelineBuilder builder(pipelineCreateInfo);
				builder.setComputeShader("Resources/Shaders/hlsl/atmosphereComputeSingleScattering_cs");

				auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

				cmdList->bindPipeline(pipeline);

				// update descriptor sets
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *deltaRayleighImageView = registry.getImageView(deltaRayleighScatteringImageViewHandle);
					ImageView *deltaMieImageView = registry.getImageView(deltaMieScatteringImageViewHandle);
					ImageView *scatteringImageView = registry.getImageView(scatteringImageViewHandle);
					ImageView *transmittanceImageView = registry.getImageView(transmittanceImageViewHandle);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::rwTexture(&deltaRayleighImageView, 0),
						Initializers::rwTexture(&deltaMieImageView, 1),
						Initializers::rwTexture(&scatteringImageView, 2),
						Initializers::texture(&transmittanceImageView, 4),
						Initializers::constantBuffer(&constantBufferInfo, 6),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);

					DescriptorSet *sets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
				}

				cmdList->dispatch((SCATTERING_TEXTURE_WIDTH + 7) / 8, (SCATTERING_TEXTURE_HEIGHT + 7) / 8, SCATTERING_TEXTURE_DEPTH);
			}, true);
	}

	// compute multiple scattering orders
	for (uint32_t scatteringOrder = 2; scatteringOrder <= 4; ++scatteringOrder)
	{
		// compute scattering density
		{
			rg::ResourceUsageDescription passUsages[]
			{
				{rg::ResourceViewHandle(deltaScatteringDensityImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(transmittanceImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(deltaRayleighScatteringImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(deltaMieScatteringImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(deltaMultipleScatteringImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(deltaIrradianceImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
			};

			graph.addPass("AS Scattering Density", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
				{
					// create pipeline description
					ComputePipelineCreateInfo pipelineCreateInfo;
					ComputePipelineBuilder builder(pipelineCreateInfo);
					builder.setComputeShader("Resources/Shaders/hlsl/atmosphereComputeScatteringDensity_cs");

					auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

					cmdList->bindPipeline(pipeline);

					// update descriptor sets
					{
						DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

						ImageView *deltaScattDensityImageView = registry.getImageView(deltaScatteringDensityImageViewHandle);
						ImageView *transmittanceImageView = registry.getImageView(transmittanceImageViewHandle);
						ImageView *deltaRayleighImageView = registry.getImageView(deltaRayleighScatteringImageViewHandle);
						ImageView *deltaMieImageView = registry.getImageView(deltaMieScatteringImageViewHandle);
						ImageView *deltaMultipleImageView = registry.getImageView(deltaMultipleScatteringImageViewHandle);
						ImageView *deltaIrradianceImageView = registry.getImageView(deltaIrradianceImageViewHandle);
						

						DescriptorSetUpdate2 updates[] =
						{
							Initializers::rwTexture(&deltaScattDensityImageView, 0),
							Initializers::texture(&transmittanceImageView, 1),
							Initializers::texture(&deltaRayleighImageView, 2),
							Initializers::texture(&deltaMieImageView, 3),
							Initializers::texture(&deltaMultipleImageView, 4),
							Initializers::texture(&deltaIrradianceImageView, 5),
							Initializers::constantBuffer(&constantBufferInfo, 7),
						};

						descriptorSet->update((uint32_t)std::size(updates), updates);

						DescriptorSet *sets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
						cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
					}

					cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, 4, &scatteringOrder);
					cmdList->dispatch((SCATTERING_TEXTURE_WIDTH + 7) / 8, (SCATTERING_TEXTURE_HEIGHT + 7) / 8, SCATTERING_TEXTURE_DEPTH);
				}, true);
		}

		// compute indirect irradiance
		{
			rg::ResourceUsageDescription passUsages[]
			{
				{rg::ResourceViewHandle(deltaIrradianceImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(irradianceImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(deltaRayleighScatteringImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(deltaMieScatteringImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(deltaMultipleScatteringImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
			};

			graph.addPass("AS Indirect Irradiance", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
				{
					// create pipeline description
					ComputePipelineCreateInfo pipelineCreateInfo;
					ComputePipelineBuilder builder(pipelineCreateInfo);
					builder.setComputeShader("Resources/Shaders/hlsl/atmosphereComputeIndirectIrradiance_cs");

					auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

					cmdList->bindPipeline(pipeline);

					// update descriptor sets
					{
						DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

						ImageView *deltaIrrImageView = registry.getImageView(deltaIrradianceImageViewHandle);
						ImageView *irrImageView = registry.getImageView(irradianceImageViewHandle);
						ImageView *deltaRayleighImageView = registry.getImageView(deltaRayleighScatteringImageViewHandle);
						ImageView *deltaMieImageView = registry.getImageView(deltaMieScatteringImageViewHandle);
						ImageView *deltaMultipleImageView = registry.getImageView(deltaMultipleScatteringImageViewHandle);

						DescriptorSetUpdate2 updates[] =
						{
							Initializers::rwTexture(&deltaIrrImageView, 0),
							Initializers::rwTexture(&irrImageView, 1),
							Initializers::texture(&deltaRayleighImageView, 2),
							Initializers::texture(&deltaMieImageView, 3),
							Initializers::texture(&deltaMultipleImageView, 4),
							Initializers::constantBuffer(&constantBufferInfo, 6),
						};

						descriptorSet->update((uint32_t)std::size(updates), updates);

						DescriptorSet *sets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
						cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
					}

					const uint32_t pushConstScattOrder = scatteringOrder - 1;
					cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, 4, &pushConstScattOrder);
					cmdList->dispatch((IRRADIANCE_TEXTURE_WIDTH + 7) / 8, (IRRADIANCE_TEXTURE_HEIGHT + 7) / 8, 1);
				}, true);
		}

		// compute multiple scattering
		{
			rg::ResourceUsageDescription passUsages[]
			{
				{rg::ResourceViewHandle(deltaMultipleScatteringImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(scatteringImageViewHandle), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(transmittanceImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
				{rg::ResourceViewHandle(deltaScatteringDensityImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
			};

			graph.addPass("AS Multiple Scattering", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
				{
					// create pipeline description
					ComputePipelineCreateInfo pipelineCreateInfo;
					ComputePipelineBuilder builder(pipelineCreateInfo);
					builder.setComputeShader("Resources/Shaders/hlsl/atmosphereComputeMultipleScattering_cs");

					auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

					cmdList->bindPipeline(pipeline);

					// update descriptor sets
					{
						DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

						ImageView *deltaMultipleImageView = registry.getImageView(deltaMultipleScatteringImageViewHandle);
						ImageView *scatteringImageView = registry.getImageView(scatteringImageViewHandle);
						ImageView *transmittanceImageView = registry.getImageView(transmittanceImageViewHandle);
						ImageView *deltaScattDensityImageView = registry.getImageView(deltaScatteringDensityImageViewHandle);
						
						DescriptorSetUpdate2 updates[] =
						{
							Initializers::rwTexture(&deltaMultipleImageView, 0),
							Initializers::rwTexture(&scatteringImageView, 1),
							Initializers::texture(&transmittanceImageView, 2),
							Initializers::texture(&deltaScattDensityImageView, 3),
							Initializers::constantBuffer(&constantBufferInfo, 5),
						};

						descriptorSet->update((uint32_t)std::size(updates), updates);

						DescriptorSet *sets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
						cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
					}

					cmdList->dispatch((SCATTERING_TEXTURE_WIDTH + 7) / 8, (SCATTERING_TEXTURE_HEIGHT + 7) / 8, SCATTERING_TEXTURE_DEPTH);
				}, true);
		}
	}
}

void VEngine::AtmosphericScatteringModule::registerResources(rg::RenderGraph &graph)
{
	// import resources
	{
		rg::ImageHandle transmittanceImageHandle = graph.importImage(m_transmittanceImage, "AS Transmittance Image", false, {}, &m_transmittanceImageState);
		m_transmittanceImageViewHandle = graph.createImageView({ "AS Transmittance Image", transmittanceImageHandle, { 0, 1, 0, 1 }, ImageViewType::_2D });

		rg::ImageHandle scatteringImageHandle = graph.importImage(m_scatteringImage, "AS Scattering Image", false, {}, &m_scatteringImageState);
		m_scatteringImageViewHandle = graph.createImageView({ "AS Scattering Image", scatteringImageHandle, { 0, 1, 0, 1 }, ImageViewType::_3D });
	}
}

VEngine::rg::ImageViewHandle VEngine::AtmosphericScatteringModule::getTransmittanceImageViewHandle()
{
	return m_transmittanceImageViewHandle;
}

VEngine::rg::ImageViewHandle VEngine::AtmosphericScatteringModule::getScatteringImageViewHandle()
{
	return m_scatteringImageViewHandle;
}

VEngine::gal::DescriptorBufferInfo VEngine::AtmosphericScatteringModule::getConstantBufferInfo()
{
	return {m_constantBuffer, 0, sizeof(AtmosphereParameters)};
}
