#include "MemoryAllocatorPlot.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "Graphics/imgui/imgui_internal.h"

void VEngine::plotMemoryAllocator(const char* label, float(*values_getter)(void* data, int idx, int *type), void* data, int values_count, const char* overlay_text, ImVec2 frame_size)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);

	const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
	if (frame_size.x == 0.0f)
		frame_size.x = ImGui::CalcItemWidth();
	if (frame_size.y == 0.0f)
		frame_size.y = label_size.y + (style.FramePadding.y * 2);

	const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
	const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
	const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, 0, &frame_bb))
		return;
	const bool hovered = ImGui::ItemHoverable(frame_bb, id);

	ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

	const ImU32 colors[] =
	{
		ImGui::GetColorU32({ 0.0f, 1.0f, 0.0f, 1.0f }),
		ImGui::GetColorU32({ 0.0f, 0.5f, 0.0f, 1.0f }),
		ImGui::GetColorU32({ 0.0f, 0.0f, 1.0f, 1.0f }),
		ImGui::GetColorU32({ 0.0f, 0.0f, 0.5f, 1.0f }),
		ImGui::GetColorU32({ 1.0f, 0.0f, 0.0f, 1.0f }),
		ImGui::GetColorU32({ 0.5f, 0.0f, 0.0f, 1.0f }),
	};

	const ImU32 hoveredColor = ImGui::GetColorU32({ 1.0f, 1.0f, 0.0f, 1.0f });

	float lastPos = inner_bb.Min.x;
	for (int i = 0; i < values_count; ++i)
	{
		int type = 0;
		float pos = ImLerp(inner_bb.Min.x, inner_bb.Max.x, values_getter(data, i, &type));
		ImRect rect({ lastPos, inner_bb.Min.y }, { pos, inner_bb.Max.y });
		if (hovered && rect.Contains(g.IO.MousePos))
		{
			window->DrawList->AddRectFilled(rect.Min, rect.Max, hoveredColor);
			ImGui::SetTooltip("TODO");
		}
		else
		{
			window->DrawList->AddRectFilled(rect.Min, rect.Max, colors[type * 2 + (i & 1)]);
		}
		
		lastPos = pos;
	}

	// Text overlay
	if (overlay_text)
		ImGui::RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL, ImVec2(0.5f, 0.0f));

	if (label_size.x > 0.0f)
		ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);
}
