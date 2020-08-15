#pragma once
#include "Handles.h"

namespace VEngine
{
	struct EditorBillboardComponent
	{
		Texture2DHandle m_texture;
		float m_scale;
		float m_opacity;
	};

	struct BillboardComponent
	{
		Texture2DHandle m_texture;
		float m_scale;
		float m_opacity;
	};
}