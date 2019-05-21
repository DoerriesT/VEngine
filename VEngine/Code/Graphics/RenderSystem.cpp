#include "RenderSystem.h"
#include "Components/MeshComponent.h"
#include "Components/TransformationComponent.h"
#include "Components/RenderableComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
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
	m_opaqueBatch.clear();
	m_alphaTestedBatch.clear();
	m_transparentBatch.clear();
	m_opaqueShadowBatch.clear();
	m_alphaTestedShadowBatch.clear();

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

		// extract view frustum plane equations from matrix
		glm::vec4 viewFrustumPlaneEquations[6];
		{
			glm::mat4 proj = glm::transpose(m_commonRenderData.m_viewProjectionMatrix);
			viewFrustumPlaneEquations[0] = glm::normalize(proj[3] + proj[0]);	// left
			viewFrustumPlaneEquations[1] = glm::normalize(proj[3] - proj[0]);	// right
			viewFrustumPlaneEquations[2] = glm::normalize(proj[3] + proj[1]);	// bottom
			viewFrustumPlaneEquations[3] = glm::normalize(proj[3] - proj[1]);	// top
			viewFrustumPlaneEquations[4] = glm::normalize(proj[2]);				// near
			viewFrustumPlaneEquations[5] = glm::normalize(proj[3] - proj[2]);	// far
		};

		glm::mat4 unifiedDirectionalLightViewProjectionMatrix;
		float orthoNear = std::numeric_limits<float>::max();
		float orthoFar = std::numeric_limits<float>::lowest();
		glm::mat4 lightView;
		float projScaleXInv;
		float projScaleYInv;

		// generate light data
		{
			// point lights
			{
				auto view = m_entityRegistry.view<TransformationComponent, PointLightComponent, RenderableComponent>();

				view.each([this, &viewFrustumPlaneEquations](TransformationComponent &transformationComponent, PointLightComponent &pointLightComponent, RenderableComponent&)
				{
					const float intensity = pointLightComponent.m_luminousPower * (1.0f / (4.0f * glm::pi<float>()));

					PointLightData pointLightData
					{
						glm::vec4(glm::vec3(m_commonRenderData.m_viewMatrix * glm::vec4(transformationComponent.m_position, 1.0f)), pointLightComponent.m_radius),
						glm::vec4(pointLightComponent.m_color * intensity, 1.0f / (pointLightComponent.m_radius * pointLightComponent.m_radius))
					};

					// frustum cull
					for (const auto &plane : viewFrustumPlaneEquations)
					{
						if (glm::dot(glm::vec4(transformationComponent.m_position, 1.0f), plane) <= -pointLightComponent.m_radius)
						{
							return;
						}
					}

					m_lightData.m_pointLightData.push_back(pointLightData);
				});

				// sort by distance to camera
				std::sort(m_lightData.m_pointLightData.begin(), m_lightData.m_pointLightData.end(),
					[](const PointLightData &lhs, const PointLightData &rhs)
				{
					return (-lhs.m_positionRadius.z - lhs.m_positionRadius.w) < (-rhs.m_positionRadius.z - rhs.m_positionRadius.w);
				});

				// clear bins
				for (size_t i = 0; i < m_lightData.m_zBins.size(); ++i)
				{
					const uint32_t emptyBin = ((~0u & 0xFFFFu) << 16u);
					m_lightData.m_zBins[i] = emptyBin;
				}

				// assign lights to bins
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
			}

			// directional lights
			{
				auto view = m_entityRegistry.view<DirectionalLightComponent, RenderableComponent>();

				view.each([&](DirectionalLightComponent &directionalLightComponent, RenderableComponent&)
				{
					directionalLightComponent.m_direction = glm::normalize(directionalLightComponent.m_direction);
					DirectionalLightData directionalLightData
					{
						glm::vec4(directionalLightComponent.m_color, 1.0f),
						m_commonRenderData.m_viewMatrix * glm::vec4(directionalLightComponent.m_direction, 0.0f)
					};

					// first directional light is allowed to have shadows
					if (m_lightData.m_directionalLightData.empty() && directionalLightComponent.m_shadows)
					{
						directionalLightData.m_shadowDataOffset = 0;
						directionalLightData.m_shadowDataCount = 4;

						m_lightData.m_shadowData.push_back({ glm::mat4(), { 0.25f, 0.25f, 0.0f, 0.0f } });
						m_lightData.m_shadowData.push_back({ glm::mat4(), { 0.25f, 0.25f,  0.25f, 0.0f } });
						m_lightData.m_shadowData.push_back({ glm::mat4(), { 0.25f, 0.25f, 0.0f,  0.25f } });
						m_lightData.m_shadowData.push_back({ glm::mat4(), { 0.25f, 0.25f, 0.25f, 0.25f } });

						m_lightData.m_shadowJobs.push_back({ glm::mat4(), 0, 0, 2048 });
						m_lightData.m_shadowJobs.push_back({ glm::mat4(), 2048, 0, 2048 });
						m_lightData.m_shadowJobs.push_back({ glm::mat4(), 0, 2048, 2048 });
						m_lightData.m_shadowJobs.push_back({ glm::mat4(), 2048, 2048, 2048 });

						lightView = glm::lookAt(glm::vec3(), -glm::vec3(directionalLightComponent.m_direction), glm::vec3(glm::transpose(m_commonRenderData.m_viewMatrix)[0]));
						glm::mat4 cameraViewToLightView = lightView * m_commonRenderData.m_invViewMatrix;
						projScaleXInv = 1.0f / m_commonRenderData.m_jitteredProjectionMatrix[0][0];
						projScaleYInv = 1.0f / m_commonRenderData.m_jitteredProjectionMatrix[1][1];

						// extract frustum points
						const float nearZ = m_commonRenderData.m_nearPlane;
						const float farZ = m_commonRenderData.m_farPlane;

						glm::vec3 corners[8];
						// near corners (in view space)
						const float nearX = projScaleXInv * nearZ;
						const float nearY = projScaleYInv * nearZ;
						corners[0] = glm::vec3(-nearX, nearY, -nearZ);
						corners[1] = glm::vec3(nearX, nearY, -nearZ);
						corners[2] = glm::vec3(-nearX, -nearY, -nearZ);
						corners[3] = glm::vec3(nearX, -nearY, -nearZ);
						// far corners (in view space)
						const float farX = projScaleXInv * farZ;
						const float farY = projScaleYInv * farZ;
						corners[4] = glm::vec3(-farX, farY, -farZ);
						corners[5] = glm::vec3(farX, farY, -farZ);
						corners[6] = glm::vec3(-farX, -farY, -farZ);
						corners[7] = glm::vec3(farX, -farY, -farZ);

						glm::vec3 minCorner = glm::vec3(std::numeric_limits<float>::max());
						glm::vec3 maxCorner = glm::vec3(std::numeric_limits<float>::lowest());

						// find min and max corners in light space
						for (glm::uint i = 0; i < 8; ++i)
						{
							glm::vec3 corner = glm::vec3(cameraViewToLightView * glm::vec4(corners[i], 1.0f));
							minCorner = glm::min(minCorner, corner);
							maxCorner = glm::max(maxCorner, corner);
						}

						glm::vec3 center = 0.5f * (minCorner + maxCorner);

						lightView = glm::translate(glm::vec3(-center.x, -center.y, 0.0f)) * lightView;
						glm::vec3 dimensions = maxCorner - minCorner;

						unifiedDirectionalLightViewProjectionMatrix = glm::ortho(-dimensions.x * 0.5f, dimensions.x * 0.5f, -dimensions.y * 0.5f, dimensions.y * 0.5f, 0.0f, 1.0f) * lightView;
					}

					m_lightData.m_directionalLightData.push_back(directionalLightData);
				});
			}
		}

		// extract shadow frustum plane equations from matrices
		glm::vec4 shadowFrustumPlaneEquations[4];
		if (!m_lightData.m_directionalLightData.empty() && m_lightData.m_directionalLightData.front().m_shadowDataCount)
		{
			glm::mat4 proj = glm::transpose(unifiedDirectionalLightViewProjectionMatrix);
			shadowFrustumPlaneEquations[0] = glm::normalize(proj[3] + proj[0]);	// left
			shadowFrustumPlaneEquations[1] = glm::normalize(proj[3] - proj[0]);	// right
			shadowFrustumPlaneEquations[2] = glm::normalize(proj[3] + proj[1]);	// bottom
			shadowFrustumPlaneEquations[3] = glm::normalize(proj[3] - proj[1]);	// top
		};

		// update all transformations and generate draw lists
		{
			auto view = m_entityRegistry.view<MeshComponent, TransformationComponent, RenderableComponent>();

			view.each([&](MeshComponent &meshComponent, TransformationComponent &transformationComponent, RenderableComponent&)
			{
				const glm::mat4 rotationMatrix = glm::mat4_cast(transformationComponent.m_orientation);
				transformationComponent.m_previousTransformation = transformationComponent.m_transformation;
				transformationComponent.m_transformation = glm::translate(transformationComponent.m_position)
					* rotationMatrix
					* glm::scale(glm::vec3(transformationComponent.m_scale));

				const uint32_t transformIndex = static_cast<uint32_t>(m_transformData.size());

				m_transformData.push_back(transformationComponent.m_transformation);

				const glm::mat3 rotationMatrix3x3 = glm::mat3(m_commonRenderData.m_viewMatrix) * glm::mat3(rotationMatrix);

				for (const auto &p : meshComponent.m_subMeshMaterialPairs)
				{
					SubMeshInstanceData instanceData;
					instanceData.m_subMeshIndex = p.first;
					instanceData.m_transformIndex = transformIndex;
					instanceData.m_materialIndex = p.second;

					const auto batchAssigment = m_materialBatchAssignment[p.second];

					// frustum cull
					bool viewFrustumCulled = false;
					bool shadowFrustumCulled = false;
					auto aabb = m_aabbs[instanceData.m_subMeshIndex];
					{
						glm::vec3 center = (aabb.m_max + aabb.m_min) * 0.5f;
						glm::vec3 half = (aabb.m_max - aabb.m_min) * 0.5f;

						auto cullAgainstPlanes = [](const glm::vec3 center, const glm::vec3 half, size_t count, const glm::vec4 *planes)
						{
							for (size_t i = 0; i < count; ++i)
							{
								glm::vec3 normal = planes[i];
								float extent = glm::dot(half, glm::abs(normal));
								float s = glm::dot(center, normal) + planes[i].w;
								if (!((s - extent > 0) || !(s + extent < 0)))
								{
									return false;
								}
							}
							return true;
						};

						viewFrustumCulled = !cullAgainstPlanes(center, half, 6, viewFrustumPlaneEquations);
						shadowFrustumCulled = !cullAgainstPlanes(center, half, 4, shadowFrustumPlaneEquations);
					}

					if (!viewFrustumCulled)
					{
						// opaque batch
						if (batchAssigment & 1)
						{
							m_opaqueBatch.push_back(instanceData);
						}
						// masked batch
						else if (batchAssigment & (1 << 1))
						{
							m_alphaTestedBatch.push_back(instanceData);
						}
						// transparent batch
						else if (batchAssigment & (1 << 2))
						{
							m_transparentBatch.push_back(instanceData);
						}
					}

					if (!shadowFrustumCulled)
					{
						glm::vec4 corners[8] =
						{
							{ aabb.m_min.x, aabb.m_min.y, aabb.m_min.z, 1.0f},
							{ aabb.m_min.x, aabb.m_max.y, aabb.m_min.z, 1.0f},
							{ aabb.m_max.x, aabb.m_min.y, aabb.m_min.z, 1.0f},
							{ aabb.m_max.x, aabb.m_max.y, aabb.m_min.z, 1.0f},
							{ aabb.m_min.x, aabb.m_min.y, aabb.m_max.z, 1.0f},
							{ aabb.m_min.x, aabb.m_max.y, aabb.m_max.z, 1.0f},
							{ aabb.m_max.x, aabb.m_min.y, aabb.m_max.z, 1.0f},
							{ aabb.m_max.x, aabb.m_max.y, aabb.m_max.z, 1.0f},
						};

						float nearest = orthoNear;
						float farthest = orthoFar;

						for (size_t i = 0; i < 8; ++i)
						{
							auto c = corners[i] * unifiedDirectionalLightViewProjectionMatrix;
							nearest = std::min(c.z, nearest);
							farthest = std::max(c.z, farthest);
						}

						// opaque batch
						if (batchAssigment & 1)
						{
							orthoNear = std::min(orthoNear, nearest);
							orthoFar = std::max(orthoFar, farthest);
							m_opaqueShadowBatch.push_back(instanceData);
						}
						// masked batch
						else if (batchAssigment & (1 << 1))
						{
							orthoNear = std::min(orthoNear, nearest);
							orthoFar = std::max(orthoFar, farthest);
							m_alphaTestedShadowBatch.push_back(instanceData);
						}
					}
				}
			});

			std::sort(m_opaqueBatch.begin(), m_opaqueBatch.end(), [](const auto &lhs, const auto &rhs) {return lhs.m_materialIndex < rhs.m_materialIndex; });
			std::sort(m_alphaTestedBatch.begin(), m_alphaTestedBatch.end(), [](const auto &lhs, const auto &rhs) {return lhs.m_materialIndex < rhs.m_materialIndex; });
			std::sort(m_transparentBatch.begin(), m_transparentBatch.end(), [](const auto &lhs, const auto &rhs) {return lhs.m_materialIndex < rhs.m_materialIndex; });
			std::sort(m_alphaTestedShadowBatch.begin(), m_alphaTestedShadowBatch.end(), [](const auto &lhs, const auto &rhs) {return lhs.m_materialIndex < rhs.m_materialIndex; });
		}

		m_commonRenderData.m_directionalLightCount = static_cast<uint32_t>(m_lightData.m_directionalLightData.size());
		m_commonRenderData.m_pointLightCount = static_cast<uint32_t>(m_lightData.m_pointLightData.size());

		RenderData renderData;
		renderData.m_transformDataCount = static_cast<uint32_t>(m_transformData.size());
		renderData.m_transformData = m_transformData.data();
		renderData.m_opaqueBatchSize = static_cast<uint32_t>(m_opaqueBatch.size());
		renderData.m_opaqueBatch = m_opaqueBatch.data();
		renderData.m_alphaTestedBatchSize = static_cast<uint32_t>(m_alphaTestedBatch.size());
		renderData.m_alphaTestedBatch = m_alphaTestedBatch.data();
		renderData.m_transparentBatchSize = static_cast<uint32_t>(m_transparentBatch.size());
		renderData.m_transparentBatch = m_transparentBatch.data();
		renderData.m_opaqueShadowBatchSize = static_cast<uint32_t>(m_opaqueShadowBatch.size());
		renderData.m_opaqueShadowBatch = m_opaqueShadowBatch.data();
		renderData.m_alphaTestedShadowBatchSize = static_cast<uint32_t>(m_alphaTestedShadowBatch.size());
		renderData.m_alphaTestedShadowBatch = m_alphaTestedShadowBatch.data();
		renderData.m_orthoNearest = orthoNear;
		renderData.m_orthoFarthest = orthoFar;
		renderData.m_projScaleXInv = projScaleXInv;
		renderData.m_projScaleYInv = projScaleYInv;
		renderData.m_lightView = lightView;
		renderData.m_cameraToLightView = lightView * m_commonRenderData.m_invViewMatrix;

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
		case Material::Alpha::BLENDED:
			batchAssignment = 1 << 2;
			break;
		default:
			break;
		}
	}
}
