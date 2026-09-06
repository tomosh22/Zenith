#include "Zenith.h"
#include "AssetHandling/Zenith_AnimatorControllerAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

bool Zenith_AnimatorControllerAsset::Export(const std::string& strPath) const
{
	if (strPath.empty())
	{
		Zenith_Assert(false, "Zenith_AnimatorControllerAsset::Export: empty path");
		return false;
	}
	return m_xDef.Export(Zenith_AssetRegistry::ResolvePath(strPath));
}

Zenith_Status Zenith_AnimatorControllerAsset::LoadFromFile(const std::string& strPath)
{
	if (strPath.empty())
	{
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());
	if (!xStream.IsValid())
	{
		// A missing file is a reportable condition, not a programming error — no
		// assert, matching Zenith_AnimationAsset's .zanim path.
		Zenith_Log(LOG_CATEGORY_ANIMATION, "Failed to read animator controller file: %s", strPath.c_str());
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}

	return m_xDef.ParseStream(xStream);
}

#ifdef ZENITH_TOOLS
void Zenith_AnimatorControllerAsset::RenderPropertiesPanel()
{
	ImGui::Text("Animator Controller");
	ImGui::Text("Clips: %u   Layers: %u   Top-level SM: %s",
		m_xDef.GetClipPaths().GetSize(),
		m_xDef.GetLayerCount(),
		m_xDef.HasStateMachineDef() ? "yes" : "no");

	for (u_int u = 0; u < m_xDef.GetLayerCount(); ++u)
	{
		const Flux_AnimatorControllerLayerDef* pxLayer = m_xDef.GetLayer(u);
		if (pxLayer == nullptr)
		{
			continue;
		}
		ImGui::Text("  [%u] %s  weight %.2f  %s%s",
			pxLayer->GetLayerId(),
			pxLayer->GetName().c_str(),
			pxLayer->GetWeight(),
			pxLayer->GetBlendMode() == LAYER_BLEND_ADDITIVE ? "additive" : "override",
			pxLayer->GetBoneMaskAssetPath().empty() ? "" : "  (masked)");
	}
}
#endif

#ifdef ZENITH_TESTING
#include "AssetHandling/Zenith_AnimatorControllerAsset.Tests.inl"
#endif
