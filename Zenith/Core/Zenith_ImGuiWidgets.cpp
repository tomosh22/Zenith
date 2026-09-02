#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Core/Zenith_ImGuiWidgets.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace
{
	// The X/Y/Z tag colours. Kept in sRGB here and converted to the linear values the
	// sRGB swapchain expects at draw time, mirroring the editor theme's conversion
	// (see Zenith_EditorUI::ApplyTheme).
	ImU32 LinearColour(float fR, float fG, float fB)
	{
		const ImVec4 xLinear(powf(fR, 2.2f), powf(fG, 2.2f), powf(fB, 2.2f), 1.0f);
		return ImGui::ColorConvertFloat4ToU32(xLinear);
	}

	struct AxisStyle
	{
		const char* m_szTag;
		float m_fR, m_fG, m_fB;
	};

	constexpr AxisStyle s_axAXES[3] =
	{
		{ "X", 0.86f, 0.30f, 0.32f },
		{ "Y", 0.46f, 0.76f, 0.30f },
		{ "Z", 0.30f, 0.53f, 0.93f },
	};

	// One axis tag: a small filled rounded rect carrying the axis letter, acting as
	// a button. Returns true when clicked (caller resets that component).
	bool AxisTag(const AxisStyle& xAxis, float fHeight)
	{
		const ImVec2 xSize(fHeight * 0.95f, fHeight);
		const ImVec2 xPos = ImGui::GetCursorScreenPos();
		const bool bClicked = ImGui::InvisibleButton(xAxis.m_szTag, xSize);
		const bool bHovered = ImGui::IsItemHovered();

		ImDrawList* pxDraw = ImGui::GetWindowDrawList();
		const ImU32 uFill = LinearColour(
			xAxis.m_fR * (bHovered ? 1.15f : 1.0f),
			xAxis.m_fG * (bHovered ? 1.15f : 1.0f),
			xAxis.m_fB * (bHovered ? 1.15f : 1.0f));
		const float fRounding = ImGui::GetStyle().FrameRounding;
		pxDraw->AddRectFilled(xPos, ImVec2(xPos.x + xSize.x, xPos.y + xSize.y), uFill, fRounding, ImDrawFlags_RoundCornersLeft);

		const ImVec2 xTextSize = ImGui::CalcTextSize(xAxis.m_szTag);
		const ImVec2 xTextPos(xPos.x + (xSize.x - xTextSize.x) * 0.5f, xPos.y + (xSize.y - xTextSize.y) * 0.5f);
		pxDraw->AddText(xTextPos, LinearColour(0.08f, 0.08f, 0.08f), xAxis.m_szTag);

		if (bHovered)
		{
			ImGui::SetTooltip("Reset %s", xAxis.m_szTag);
		}
		return bClicked;
	}
}

namespace Zenith_ImGuiWidgets
{

float GetLabelColumnWidth()
{
	// ~9 average glyphs of the current font. Scales with DPI via the font size.
	return ImGui::GetFontSize() * 6.0f;
}

void PropertyLabel(const char* szLabel)
{
	const float fLabelWidth = GetLabelColumnWidth();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(szLabel);
	ImGui::SameLine(fLabelWidth);
	ImGui::SetNextItemWidth(-FLT_MIN);
}

void Note(const char* szText)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::TextWrapped("%s", szText);
	ImGui::PopStyleColor();
}

bool Vec3Field(const char* szLabel, float* pfValues, float fSpeed, float fResetValue,
	const char* szFormat, float fMin, float fMax)
{
	bool bChanged = false;

	ImGui::PushID(szLabel);

	const ImGuiStyle& xStyle = ImGui::GetStyle();
	const float fFrameHeight = ImGui::GetFrameHeight();
	const float fGap = xStyle.ItemInnerSpacing.x;
	const float fTagWidth = fFrameHeight * 0.95f;
	const float fLabelWidth = GetLabelColumnWidth();

	// Label beside the fields when there is room for three legible fields;
	// otherwise on its own line so a narrow inspector never clips Y and Z.
	const float fMinFieldWidth = ImGui::GetFontSize() * 3.2f;
	const float fRowNeeded = fLabelWidth + (fTagWidth + fMinFieldWidth) * 3.0f + fGap * 2.0f;
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(szLabel);
	// The label item left the cursor at the start of the next line, so the
	// available width here is the full row width.
	if (ImGui::GetContentRegionAvail().x >= fRowNeeded)
	{
		ImGui::SameLine(fLabelWidth);
	}

	const float fAvail = ImGui::GetContentRegionAvail().x;
	// Three (tag + field) groups, separated by the inner spacing.
	const float fGroupWidth = (fAvail - fGap * 2.0f) / 3.0f;
	const float fFieldWidth = ImMax(fGroupWidth - fTagWidth, fFrameHeight);

	// Tag and field are glued: no spacing between them, the tag rounds its left
	// corners and the field its right ones so the pair reads as one control.
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, xStyle.ItemSpacing.y));

	for (int i = 0; i < 3; ++i)
	{
		ImGui::PushID(i);
		if (i > 0)
		{
			ImGui::SameLine(0.0f, fGap);
		}
		if (AxisTag(s_axAXES[i], fFrameHeight))
		{
			pfValues[i] = fResetValue;
			bChanged = true;
		}
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::SetNextItemWidth(fFieldWidth);
		if (ImGui::DragFloat("##v", &pfValues[i], fSpeed, fMin, fMax, szFormat))
		{
			bChanged = true;
		}
		ImGui::PopID();
	}

	ImGui::PopStyleVar();
	ImGui::PopID();
	return bChanged;
}

} // namespace Zenith_ImGuiWidgets

#endif // ZENITH_TOOLS
