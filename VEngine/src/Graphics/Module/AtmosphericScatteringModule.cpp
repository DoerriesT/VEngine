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
	params.solar_irradiance = float3(1.474f, 1.8504f, 1.91198f); // TODO
	params.sun_angular_radius = 0.004675f;
	params.bottom_radius = 6360000.0f;
	params.top_radius = 6420000.0f;
	params.rayleigh_density = { {{0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, -1.25E-4f, 0.0f, 0.0f}} };
	params.rayleigh_scattering = float3(5.802339381712381E-6f, 1.355776244792022E-5f, 3.3100005976367735E-5f);
	params.mie_density = { {{0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, -8.333333333333334E-4f, 0.0f, 0.0f}} };
	params.mie_scattering = float3(3.996E-6f, 3.996E-6f, 3.996E-6f) * 1.0f;
	params.mie_phase_function_g = 0.875f;
	params.absorption_density = { {{25000.0f, 0.0f, 0.0f, 6.666666666666667E-5f, -0.6666666666666666f}, {0.0f, 0.0f, 0.0f, -6.666666666666667E-5f, 2.6666666666666665f}} };
	params.absorption_extinction = float3(6.497166E-7f, 1.8809E-6f, 8.501667999999999E-8f);
	params.ground_albedo = float3(0.1f, 0.1f, 0.1f);
	params.mu_s_min = -0.5f;

	// copy to constant buffer
	AtmosphereParameters *data;
	m_constantBuffer->map((void **)&data);
	*data = params;
	m_constantBuffer->unmap();
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
