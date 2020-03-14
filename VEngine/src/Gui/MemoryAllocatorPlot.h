#pragma once
#include "Graphics/imgui/imgui.h"

namespace VEngine
{
	void plotMemoryAllocator(const char* label, float(*values_getter)(void* data, int idx, int *type), void* data, int values_count, const char* overlay_text, ImVec2 frame_size);
}