#include "Zenith.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

ZENITH_REGISTER_ASSET_TYPE(Zenith_BehaviourGraphAsset)

bool Zenith_BehaviourGraphAsset_ForceLink()
{
	return true;
}

#ifdef ZENITH_TOOLS
void Zenith_BehaviourGraphAsset::RenderPropertiesPanel()
{
	ImGui::Text("Behaviour Graph");
	ImGui::Text("Nodes: %u  Edges: %u  Variables: %u",
		m_xDefinition.GetNodeCount(), m_xDefinition.GetEdgeCount(), m_xDefinition.GetVariableCount());
	if (!m_bLoadedOk)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "FAILED TO LOAD - not a valid .bgraph");
	}
}
#endif
