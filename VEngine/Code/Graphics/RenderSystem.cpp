#include "RenderSystem.h"
#include "Components/MeshComponent.h"
#include "Components/TransformationComponent.h"
#include "Components/RenderableComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/CameraComponent.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "GlobalVar.h"
#include <glm/ext.hpp>
#include <glm/vec3.hpp>
#include <algorithm>
#include "Utility/Utility.h"
#include "Mesh.h"

VEngine::RenderSystem::RenderSystem(entt::registry &entityRegistry, void *windowHandle)
	:m_entityRegistry(entityRegistry),
	m_cameraEntity(entt::null),
	m_materialBatchAssignment(std::make_unique<uint8_t[]>(RendererConsts::MAX_MATERIALS)),
	m_aabbs(std::make_unique<AxisAlignedBoundingBox[]>(RendererConsts::MAX_SUB_MESHES))
{
	memset(m_materialBatchAssignment.get(), 0, RendererConsts::MAX_MATERIALS * sizeof(m_materialBatchAssignment[0]));
	memset(m_aabbs.get(), 0, RendererConsts::MAX_SUB_MESHES * sizeof(m_aabbs[0]));

	for (size_t i = 0; i < RendererConsts::MAX_TAA_HALTON_SAMPLES; ++i)
	{
		m_haltonX[i] = Utility::halton(i + 1, 2) * 2.0f - 1.0f;
		m_haltonY[i] = Utility::halton(i + 1, 3) * 2.0f - 1.0f;
	}

	m_renderer = std::make_unique<VKRenderer>(g_windowWidth, g_windowHeight, windowHandle);
}

void VEngine::RenderSystem::update(float timeDelta)
{
	m_transformData.clear();
	m_opaqueSubMeshInstanceData.clear();
	m_maskedSubMeshInstanceData.clear();

	m_lightData.m_shadowData.clear();
	m_lightData.m_directionalLightData.clear();
	m_lightData.m_pointLightData.clear();
	m_lightData.m_spotLightData.clear();
	m_lightData.m_shadowJobs.clear();

	if (m_cameraEntity != entt::null)
	{
		// update render params
		{
			CameraComponent &cameraComponent = m_entityRegistry.get<CameraComponent>(m_cameraEntity);
			TransformationComponent &transformationComponent = m_entityRegistry.get<TransformationComponent>(m_cameraEntity);

			glm::mat4 viewMatrix = cameraComponent.m_viewMatrix;
			glm::mat4 projectionMatrix = cameraComponent.m_projectionMatrix;
			glm::mat4 jitterMatrix = g_TAAEnabled ?
				glm::translate(glm::vec3(m_haltonX[m_commonRenderData.m_frame % RendererConsts::MAX_TAA_HALTON_SAMPLES] / g_windowWidth,
					m_haltonY[m_commonRenderData.m_frame % RendererConsts::MAX_TAA_HALTON_SAMPLES] / g_windowHeight,
					0.0f))
				: glm::mat4();

			m_commonRenderData.m_time = 0.0f;
			m_commonRenderData.m_fovy = cameraComponent.m_fovy;
			m_commonRenderData.m_nearPlane = cameraComponent.m_near;
			m_commonRenderData.m_farPlane = cameraComponent.m_far;
			m_commonRenderData.m_prevViewMatrix = m_commonRenderData.m_viewMatrix;
			m_commonRenderData.m_prevProjectionMatrix = m_commonRenderData.m_projectionMatrix;
			m_commonRenderData.m_prevViewProjectionMatrix = m_commonRenderData.m_viewProjectionMatrix;
			m_commonRenderData.m_prevInvViewMatrix = m_commonRenderData.m_invViewMatrix;
			m_commonRenderData.m_prevInvProjectionMatrix = m_commonRenderData.m_invProjectionMatrix;
			m_commonRenderData.m_prevInvViewProjectionMatrix = m_commonRenderData.m_invViewProjectionMatrix;
			m_commonRenderData.m_prevJitteredProjectionMatrix = m_commonRenderData.m_jitteredProjectionMatrix;
			m_commonRenderData.m_prevJitteredViewProjectionMatrix = m_commonRenderData.m_jitteredViewProjectionMatrix;
			m_commonRenderData.m_prevInvJitteredProjectionMatrix = m_commonRenderData.m_invJitteredProjectionMatrix;
			m_commonRenderData.m_prevInvJitteredViewProjectionMatrix = m_commonRenderData.m_invJitteredViewProjectionMatrix;
			m_commonRenderData.m_viewMatrix = viewMatrix;
			m_commonRenderData.m_projectionMatrix = projectionMatrix;
			m_commonRenderData.m_viewProjectionMatrix = projectionMatrix * viewMatrix;
			m_commonRenderData.m_invViewMatrix = glm::inverse(viewMatrix);
			m_commonRenderData.m_invProjectionMatrix = glm::inverse(projectionMatrix);
			m_commonRenderData.m_invViewProjectionMatrix = glm::inverse(m_commonRenderData.m_viewProjectionMatrix);
			m_commonRenderData.m_jitteredProjectionMatrix = jitterMatrix * m_commonRenderData.m_projectionMatrix;
			m_commonRenderData.m_jitteredViewProjectionMatrix = jitterMatrix * m_commonRenderData.m_viewProjectionMatrix;
			m_commonRenderData.m_invJitteredProjectionMatrix = glm::inverse(m_commonRenderData.m_jitteredProjectionMatrix);
			m_commonRenderData.m_invJitteredViewProjectionMatrix = glm::inverse(m_commonRenderData.m_jitteredViewProjectionMatrix);
			m_commonRenderData.m_cameraPosition = glm::vec4(transformationComponent.m_position, 1.0f);
			m_commonRenderData.m_cameraDirection = -glm::vec4(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2], 1.0f);
			m_commonRenderData.m_width = g_windowWidth;
			m_commonRenderData.m_height = g_windowHeight;
			m_commonRenderData.m_currentResourceIndex = m_commonRenderData.m_frame % RendererConsts::FRAMES_IN_FLIGHT;
			m_commonRenderData.m_previousResourceIndex = (m_commonRenderData.m_frame + RendererConsts::FRAMES_IN_FLIGHT - 1) % RendererConsts::FRAMES_IN_FLIGHT;
			m_commonRenderData.m_timeDelta = static_cast<float>(timeDelta);
		}

		glm::mat4 proj = glm::transpose(m_commonRenderData.m_viewProjectionMatrix);
		glm::vec4 frustumPlaneEquations[] =
		{
			glm::normalize(proj[3] + proj[0]),	// left
			glm::normalize(proj[3] - proj[0]),	// right
			glm::normalize(proj[3] + proj[1]),	// bottom
			glm::normalize(proj[3] - proj[1]),	// top
			glm::normalize(proj[2]),			// near
			glm::normalize(proj[3] - proj[2])	// far
		};

		//glm::mat4 shadow = glm::normalize(glm::vec3(0.1f, 3.0f, -1.0f))

		// update all transformations and generate draw lists
		{
			auto view = m_entityRegistry.view<MeshComponent, TransformationComponent, RenderableComponent>();

			view.each([&frustumPlaneEquations, this](MeshComponent &meshComponent, TransformationComponent &transformationComponent, RenderableComponent&)
			{
				transformationComponent.m_previousTransformation = transformationComponent.m_transformation;
				glm::mat4 rotationMatrix = glm::mat4_cast(transformationComponent.m_orientation);
				transformationComponent.m_transformation = glm::translate(transformationComponent.m_position)
					* rotationMatrix
					* glm::scale(glm::vec3(transformationComponent.m_scale));

				uint32_t transformIndex = static_cast<uint32_t>(m_transformData.size());

				m_transformData.push_back(transformationComponent.m_transformation);

				glm::mat3 rotationMatrix3x3 = glm::mat3(m_commonRenderData.m_viewMatrix) * glm::mat3(rotationMatrix);

				for (const auto &p : meshComponent.m_subMeshMaterialPairs)
				{
					SubMeshInstanceData instanceData;
					instanceData.m_subMeshIndex = p.first;
					instanceData.m_transformIndex = transformIndex;
					instanceData.m_materialIndex = p.second;

					auto batchAssigment = m_materialBatchAssignment[p.second];

					// frustum cull
					bool viewFrustumCulled = false;
					bool shadowFrustumCulled = false;
					{
						auto aabb = m_aabbs[instanceData.m_subMeshIndex];
						glm::vec3 center = (aabb.m_max + aabb.m_min) * 0.5f;
						glm::vec3 half = (aabb.m_max - aabb.m_min) * 0.5f;

						auto cullAgainstPlanes = [](const glm::vec3 center, const glm::vec3 half, size_t count, const glm::vec4 *planes)
						{
							for (size_t i = 0; i < count; ++i)
							{
								glm::vec3 normal = planes[i];
								float extent = glm::dot(half, glm::abs(normal));
								//float extent = half.x * 2.0f * glm::abs(glm::dot(normal, rotationMatrix3x3[0]))
								//	+ half.y * 2.0f * glm::abs(glm::dot(normal, rotationMatrix3x3[1]))
								//	+ half.z * 2.0f * glm::abs(glm::dot(normal, rotationMatrix3x3[2]));
								float s = glm::dot(center, normal) + planes[i].w;
								if (!((s - extent > 0) || !(s + extent < 0)))
								{
									return false;
								}
							}
							return true;
						};

						viewFrustumCulled = !cullAgainstPlanes(center, half, 6, frustumPlaneEquations);
					}

					if (!viewFrustumCulled)
					{
						// opaque batch
						if (batchAssigment & 1)
						{
							m_opaqueSubMeshInstanceData.push_back(instanceData);
						}
						// masked batch
						else if (batchAssigment & (1 << 1))
						{
							m_maskedSubMeshInstanceData.push_back(instanceData);
						}
					}
				}
			});
		}

		//printf("%d\n", (int)m_opaqueSubMeshInstanceData.size());

		// generate light data
		{
			auto view = m_entityRegistry.view<TransformationComponent, PointLightComponent, RenderableComponent>();

			view.each([this, &frustumPlaneEquations](TransformationComponent &transformationComponent, PointLightComponent &pointLightComponent, RenderableComponent&)
			{
				PointLightData pl = {};
				glm::vec3 pos = glm::vec3(m_commonRenderData.m_viewMatrix * glm::vec4(transformationComponent.m_position, 1.0f));
				pl.m_positionRadius = glm::vec4(pos, pointLightComponent.m_radius);
				float intensity = pointLightComponent.m_luminousPower * (1.0f / (4.0f * glm::pi<float>()));
				pl.m_colorInvSqrAttRadius = glm::vec4(pointLightComponent.m_color * intensity, 1.0f / (pointLightComponent.m_radius * pointLightComponent.m_radius));

				// frustum cull
				for (const auto &plane : frustumPlaneEquations)
				{
					if (glm::dot(glm::vec4(transformationComponent.m_position, 1.0f), plane) <= -pointLightComponent.m_radius)
					{
						return;
					}
				}

				m_lightData.m_pointLightData.push_back(pl);
			});

			std::sort(m_lightData.m_pointLightData.begin(), m_lightData.m_pointLightData.end(),
				[](const PointLightData &lhs, const PointLightData &rhs)
			{
				return (-lhs.m_positionRadius.z - lhs.m_positionRadius.w) < (-rhs.m_positionRadius.z - rhs.m_positionRadius.w);
			});

			const unsigned int emptyBin = ((~0 & 0xFFFF) << 16);

			// clear bins
			for (size_t i = 0; i < m_lightData.m_zBins.size(); ++i)
			{
				m_lightData.m_zBins[i] = emptyBin;
			}

			// assign lights
			for (size_t i = 0; i < m_lightData.m_pointLightData.size(); ++i)
			{
				glm::vec4 &posRadius = m_lightData.m_pointLightData[i].m_positionRadius;
				float nearestPoint = -posRadius.z - posRadius.w;
				float furthestPoint = -posRadius.z + posRadius.w;

				size_t minBin = glm::clamp(static_cast<size_t>(glm::max(nearestPoint / RendererConsts::Z_BIN_DEPTH, 0.0f)), size_t(0), size_t(RendererConsts::Z_BINS - 1));
				size_t maxBin = glm::clamp(static_cast<size_t>(glm::max(furthestPoint / RendererConsts::Z_BIN_DEPTH, 0.0f)), size_t(0), size_t(RendererConsts::Z_BINS - 1));

				for (size_t j = minBin; j <= maxBin; ++j)
				{
					uint32_t &val = m_lightData.m_zBins[j];
					uint32_t minIndex = (val & 0xFFFF0000) >> 16;
					uint32_t maxIndex = val & 0xFFFF;
					minIndex = std::min(minIndex, static_cast<uint32_t>(i));
					maxIndex = std::max(maxIndex, static_cast<uint32_t>(i));
					val = ((minIndex & 0xFFFF) << 16) | (maxIndex & 0xFFFF);
				}
			}

			DirectionalLightData directionalLightData{ glm::vec4(glm::vec3(100.0f), 1.0f), glm::vec4(glm::normalize(glm::vec3(0.1f, 3.0f, -1.0f)), 0.0f), 0, 3 };

			glm::mat4 matrices[3];
			calculateCascadeViewProjectionMatrices(m_commonRenderData, glm::vec3(directionalLightData.m_direction), 0.1f, 30.0f, 0.7f, 2048, 3, matrices);

			// transform light dir to view space
			directionalLightData.m_direction = m_commonRenderData.m_viewMatrix * directionalLightData.m_direction;

			m_lightData.m_directionalLightData.push_back(directionalLightData);

			m_lightData.m_shadowData.push_back({ matrices[0], { 0.25f, 0.25f, 0.0f, 0.0f } });
			m_lightData.m_shadowData.push_back({ matrices[1], { 0.25f, 0.25f,  0.25f, 0.0f } });
			m_lightData.m_shadowData.push_back({ matrices[2], { 0.25f, 0.25f, 0.0f,  0.25f } });
			//m_lightData.m_shadowData.push_back({ matrices[3], { 0.25f, 0.25f, 1.0f, 1.0f } });

			m_lightData.m_shadowJobs.push_back({ matrices[0], 0, 0, 2048 });
			m_lightData.m_shadowJobs.push_back({ matrices[1], 2048, 0, 2048 });
			m_lightData.m_shadowJobs.push_back({ matrices[2], 0, 2048, 2048 });
			//m_lightData.m_shadowJobs.push_back({ matrices[3], 2048, 2048, 2048 });
		}

		m_commonRenderData.m_directionalLightCount = static_cast<uint32_t>(m_lightData.m_directionalLightData.size());
		m_commonRenderData.m_pointLightCount = static_cast<uint32_t>(m_lightData.m_pointLightData.size());

		RenderData renderData;
		renderData.m_transformDataCount = static_cast<uint32_t>(m_transformData.size());
		renderData.m_transformData = m_transformData.data();
		renderData.m_opaqueSubMeshInstanceDataCount = static_cast<uint32_t>(m_opaqueSubMeshInstanceData.size());
		renderData.m_opaqueSubMeshInstanceData = m_opaqueSubMeshInstanceData.data();
		renderData.m_maskedSubMeshInstanceDataCount = static_cast<uint32_t>(m_maskedSubMeshInstanceData.size());
		renderData.m_maskedSubMeshInstanceData = m_maskedSubMeshInstanceData.data();

		m_renderer->render(m_commonRenderData, renderData, m_lightData);
		++m_commonRenderData.m_frame;
	}
}

VEngine::TextureHandle VEngine::RenderSystem::createTexture(const char *filepath)
{
	return m_renderer->loadTexture(filepath);
}

void VEngine::RenderSystem::destroyTexture(TextureHandle handle)
{
	m_renderer->freeTexture(handle);
}

void VEngine::RenderSystem::updateTextureData()
{
	m_renderer->updateTextureData();
}

void VEngine::RenderSystem::createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	m_renderer->createMaterials(count, materials, handles);
	updateMaterialBatchAssigments(count, materials, handles);
}

void VEngine::RenderSystem::updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	m_renderer->updateMaterials(count, materials, handles);
	updateMaterialBatchAssigments(count, materials, handles);
}

void VEngine::RenderSystem::destroyMaterials(uint32_t count, MaterialHandle *handles)
{
	m_renderer->destroyMaterials(count, handles);
}

void VEngine::RenderSystem::createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles)
{
	m_renderer->createSubMeshes(count, subMeshes, handles);
	for (uint32_t i = 0; i < count; ++i)
	{
		m_aabbs[handles[i]] = { subMeshes[i].m_minCorner, subMeshes[i].m_maxCorner };
	}
}

void VEngine::RenderSystem::destroySubMeshes(uint32_t count, SubMeshHandle *handles)
{
	m_renderer->destroySubMeshes(count, handles);
}

void VEngine::RenderSystem::setCameraEntity(entt::entity cameraEntity)
{
	m_cameraEntity = cameraEntity;
}

entt::entity VEngine::RenderSystem::getCameraEntity() const
{
	return m_cameraEntity;
}

void VEngine::RenderSystem::calculateCascadeViewProjectionMatrices(
	const CommonRenderData &renderParams,
	const glm::vec3 &lightDir,
	float nearPlane,
	float farPlane,
	float splitLambda,
	float shadowTextureSize,
	size_t cascadeCount,
	glm::mat4 *viewProjectionMatrices)
{
	const size_t maxShadowCascades = 4;
	float splits[maxShadowCascades];

	assert(cascadeCount <= maxShadowCascades);

	float range = farPlane - nearPlane;
	float ratio = farPlane / nearPlane;

	for (size_t i = 0; i < cascadeCount; ++i)
	{
		float p = (i + 1) / static_cast<float>(cascadeCount);
		float log = nearPlane * std::pow(ratio, p);
		float uniform = nearPlane + range * p;
		float d = splitLambda * (log - uniform) + uniform;
		splits[i] = (d - nearPlane) / range;
	}
	glm::mat4 vulkanCorrection =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.5f, 0.0f },
		{ 0.0f, 0.0f, 0.5f, 1.0f }
	};


	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < cascadeCount; i++)
	{
		glm::mat4 projectionMatrix = glm::perspective(renderParams.m_fovy, g_windowWidth / float(g_windowHeight), nearPlane + lastSplitDist * (farPlane - nearPlane), nearPlane + splits[i] * (farPlane - nearPlane));
		glm::mat4 invProjection = glm::inverse(projectionMatrix);

		glm::vec3 frustumCorners[8];
		frustumCorners[0] = glm::vec3(-1.0f, -1.0f, -1.0f); // xyz
		frustumCorners[1] = glm::vec3(1.0f, -1.0f, -1.0f); // Xyz
		frustumCorners[2] = glm::vec3(-1.0f, 1.0f, -1.0f); // xYz
		frustumCorners[3] = glm::vec3(1.0f, 1.0f, -1.0f); // XYz
		frustumCorners[4] = glm::vec3(-1.0f, -1.0f, 1.0f); // xyZ
		frustumCorners[5] = glm::vec3(1.0f, -1.0f, 1.0f); // XyZ
		frustumCorners[6] = glm::vec3(-1.0f, 1.0f, 1.0f); // xYZ
		frustumCorners[7] = glm::vec3(1.0f, 1.0f, 1.0f); // XYZ

		glm::vec3 minCorner = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 maxCorner = glm::vec3(std::numeric_limits<float>::lowest());

		for (size_t i = 0; i < 8; ++i)
		{
			glm::vec4 corner4 = invProjection * glm::vec4(frustumCorners[i], 1.0f);
			glm::vec3 corner = corner4 /= corner4.w;
			minCorner = glm::min(minCorner, corner);
			maxCorner = glm::max(maxCorner, corner);
		}

		float radius = glm::distance(minCorner, maxCorner) * 0.5f;
		glm::vec3 sphereCenter = (minCorner + maxCorner) * 0.5f;
		glm::vec3 target = renderParams.m_invViewMatrix * glm::vec4(sphereCenter, 1.0f);

		glm::vec3 upDir(0.0f, 1.0f, 0.0f);

		// choose different up vector if light direction would be linearly dependent otherwise
		if (abs(lightDir.x) < 0.001 && abs(lightDir.z) < 0.001)
		{
			upDir = glm::vec3(1.0f, 1.0f, 0.0f);
		}

		glm::mat4 lightView = glm::lookAt(target + lightDir * 150.0f, target - lightDir * 150.0f, upDir);

		lightView[3].x -= fmodf(lightView[3].x, (radius / static_cast<float>(shadowTextureSize)) * 2.0f);
		lightView[3].y -= fmodf(lightView[3].y, (radius / static_cast<float>(shadowTextureSize)) * 2.0f);

		viewProjectionMatrices[i] = vulkanCorrection * glm::ortho(-radius, radius, -radius, radius, 0.0f, 300.0f) * lightView;

		lastSplitDist = splits[i];
	}
}

void VEngine::RenderSystem::updateMaterialBatchAssigments(size_t count, const Material *materials, MaterialHandle *handles)
{
	for (size_t i = 0; i < count; ++i)
	{
		auto &batchAssignment = m_materialBatchAssignment[handles[i]];

		switch (materials[i].m_alpha)
		{
		case Material::Alpha::OPAQUE:
			batchAssignment = 1;
			break;
		case Material::Alpha::MASKED:
			batchAssignment = 1 << 1;
			break;
		default:
			break;
		}
	}
}
