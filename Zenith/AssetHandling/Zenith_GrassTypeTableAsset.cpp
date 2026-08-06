#include "Zenith.h"
#include "AssetHandling/Zenith_GrassTypeTableAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

ZENITH_REGISTER_ASSET_TYPE(Zenith_GrassTypeTableAsset)

bool Zenith_GrassTypeTableAsset_ForceLink()
{
	return true;
}

void Zenith_GrassTypeTableAsset::SetTable(const Flux_GrassTypeTable& xTable)
{
	m_xTable = xTable;
	// Validated on ingest, not on save: a value clamped at the point it enters the
	// asset is the value that round-trips, so what is written matches what is read.
	m_xTable.Validate();
	m_bLoadedOk = true;
}

#ifdef ZENITH_TOOLS
void Zenith_GrassTypeTableAsset::RenderPropertiesPanel()
{
	ImGui::Text("Grass Type Table");
	ImGui::Text("Types: %u / %u", m_xTable.GetCount(), uFLUX_GRASS_MAX_TYPES);
	for (u_int u = 0; u < m_xTable.GetCount(); u++)
	{
		const Flux_GrassTypeParams& xType = m_xTable.Get(u);
		ImGui::Text("  %u  %-12s  h %.2f-%.2f m  density %.2f",
			u, m_xTable.GetName(u).c_str(), xType.m_fHeightMin, xType.m_fHeightMax, xType.m_fDensity);
	}
	if (!m_bLoadedOk)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "FAILED TO LOAD - showing the built-in defaults");
	}
}
#endif
