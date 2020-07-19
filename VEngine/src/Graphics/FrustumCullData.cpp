#include "FrustumCullData.h"

VEngine::FrustumCullData::FrustumCullData(const glm::mat4 &matrix, uint32_t planeCount, uint32_t renderListIndex, uint32_t flags, const glm::vec4 viewMatrixDepthRow, float depthRange)
	:m_planeCount(planeCount),
	m_renderListIndex(renderListIndex),
	m_contentTypeFlags(flags),
	m_viewMatrixDepthRow(viewMatrixDepthRow),
	m_depthRange(depthRange)
{
	glm::mat4 proj = glm::transpose(matrix);
	m_planes[0] = (proj[3] + proj[0]);	// left
	m_planes[1] = (proj[3] - proj[0]);	// right
	m_planes[2] = (proj[3] + proj[1]);	// bottom
	m_planes[3] = (proj[3] - proj[1]);	// top
	m_planes[4] = (proj[3] - proj[2]);	// far
	m_planes[5] = (proj[2]);			// near

	for (size_t i = 0; i < m_planeCount; ++i)
	{
		float length = sqrtf(m_planes[i].x * m_planes[i].x + m_planes[i].y * m_planes[i].y + m_planes[i].z * m_planes[i].z);
		m_planes[i] /= length;
	}
}
