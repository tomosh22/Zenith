#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_RenderGraph.h"
#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "imgui.h"

namespace Zenith_EditorPanelRenderGraph
{
	static bool s_bVisible = true;
	static bool s_bHideDisabled = true;

	static const char* AccessToString(ResourceAccess eAccess)
	{
		switch (eAccess)
		{
		case RESOURCE_ACCESS_READ_SRV:          return "SRV";
		case RESOURCE_ACCESS_WRITE_RTV:          return "RTV";
		case RESOURCE_ACCESS_WRITE_UAV:          return "UAV";
		case RESOURCE_ACCESS_READ_DEPTH:         return "Depth SRV";
		case RESOURCE_ACCESS_WRITE_DSV:          return "Depth WTV";
		case RESOURCE_ACCESS_READWRITE_UAV:     return "UAV ReadWrite";
		case RESOURCE_ACCESS_UNDEFINED:          return "Undefined";
		default:                                return "Unknown";
		}
	}

	static const char* ResourceKindToString(Flux_GraphResourceKind eKind)
	{
		switch (eKind)
		{
		case Flux_GraphResourceKind::Image:     return "Image";
		case Flux_GraphResourceKind::ImageCube: return "ImageCube";
		case Flux_GraphResourceKind::Buffer:    return "Buffer";
		}
		return "?";
	}

	static const Flux_RenderGraph_Resource* FindResource(const Flux_RenderGraph& xGraph, void* pResource)
	{
		const auto& xResources = xGraph.GetResources();
		auto it = xResources.find(pResource);
		Zenith_Assert(it != xResources.end(), "Flux_RenderGraph: resource not found in resources map");
		return &it->second;
	}

	static void RenderResourceUsage(const Flux_RenderGraph_ResourceUsage& xUsage, const Flux_RenderGraph& xGraph)
	{
		void* pRes = xUsage.m_xResource.GetVoidPtr();
		const Flux_RenderGraph_Resource* pxRes = FindResource(xGraph, pRes);

		const std::string& strName = pxRes->m_xResource.GetName();
		const char* szResourceName = strName.empty() ? "<unnamed>" : strName.c_str();
		const char* szKind = ResourceKindToString(xUsage.m_xResource.GetKind());
		const char* szAccess = AccessToString(xUsage.m_eAccess);

		const bool bHasMipRange = (xUsage.m_uMipCount > 1 || xUsage.m_uMipLevel > 0);
		const bool bHasLayerRange = (xUsage.m_uLayerCount > 1 || xUsage.m_uLayer > 0);

		if (bHasMipRange && bHasLayerRange)
		{
			ImGui::Text("  %s (%s) [Mip%u/%u, Layer%u/%u] -> %s",
				szResourceName, szKind,
				xUsage.m_uMipLevel, xUsage.m_uMipCount,
				xUsage.m_uLayer, xUsage.m_uLayerCount,
				szAccess);
		}
		else if (bHasMipRange)
		{
			ImGui::Text("  %s (%s) [Mip%u/%u] -> %s",
				szResourceName, szKind, xUsage.m_uMipLevel, xUsage.m_uMipCount, szAccess);
		}
		else if (bHasLayerRange)
		{
			ImGui::Text("  %s (%s) [Layer%u/%u] -> %s",
				szResourceName, szKind, xUsage.m_uLayer, xUsage.m_uLayerCount, szAccess);
		}
		else
		{
			ImGui::Text("  %s (%s) -> %s",
				szResourceName, szKind, szAccess);
		}
	}

	static void RenderPass(const Flux_RenderGraph_Pass& xPass, const Flux_RenderGraph& xGraph)
	{
		const char* szPassName = xPass.m_strName.empty() ? "<unnamed>" : xPass.m_strName.c_str();
		const char* szType = xPass.m_bIsCompute ? "Compute" : "Graphics";

		char acHeader[256];
		int iOffset = snprintf(acHeader, sizeof(acHeader), "%s @ Topo#%u [%s]", szPassName, xPass.m_uTopologicalOrder, szType);

		if (!xPass.m_bEnabled)
		{
			iOffset += snprintf(acHeader + iOffset, sizeof(acHeader) - iOffset, " [Disabled]");
		}
		if (xPass.m_bClearTargets)
		{
			iOffset += snprintf(acHeader + iOffset, sizeof(acHeader) - iOffset, " [Clear]");
		}

		ImGui::BulletText("%s", acHeader);

		ImGui::Indent();

		if (xPass.m_xReads.GetSize() > 0)
		{
			ImGui::Text("Reads (%u):", xPass.m_xReads.GetSize());
			for (u_int i = 0; i < xPass.m_xReads.GetSize(); i++)
			{
				RenderResourceUsage(xPass.m_xReads.Get(i), xGraph);
			}
		}

		if (xPass.m_xWrites.GetSize() > 0)
		{
			ImGui::Text("Writes (%u):", xPass.m_xWrites.GetSize());
			for (u_int i = 0; i < xPass.m_xWrites.GetSize(); i++)
			{
				RenderResourceUsage(xPass.m_xWrites.Get(i), xGraph);
			}
		}

		if (xPass.m_xExplicitDependencies.GetSize() > 0)
		{
			ImGui::Text("Explicit Dependencies:");
			ImGui::Indent();
			for (u_int i = 0; i < xPass.m_xExplicitDependencies.GetSize(); i++)
			{
				u_int uDep = xPass.m_xExplicitDependencies.Get(i);
				const char* szDepName;
				if (uDep < xGraph.GetPasses().GetSize())
				{
					const Flux_RenderGraph_Pass* pxDepPass = xGraph.GetPasses().Get(uDep);
					szDepName = pxDepPass->m_strName.empty() ? "<unnamed>" : pxDepPass->m_strName.c_str();
				}
				else
				{
					szDepName = "<unknown>";
				}
				ImGui::Text("  Pass %u: %s", uDep, szDepName);
			}
			ImGui::Unindent();
		}

		ImGui::Unindent();
	}

	void Render()
	{
		if (!s_bVisible)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Render Graph", &s_bVisible))
		{
			Flux_RenderGraph& xGraph = Flux::GetRenderGraph();
			const bool bCompiled = !xGraph.IsDirty();

			ImGui::Text("Status: ");
			ImGui::SameLine();
			if (bCompiled)
			{
				ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "Compiled");
				ImGui::SameLine();
				ImGui::Text(" ✓");
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Dirty");
				ImGui::SameLine();
				ImGui::Text(" ✗");
			}

			ImGui::Separator();

			ImGui::Checkbox("Hide disabled passes", &s_bHideDisabled);

			const auto& axPasses = xGraph.GetPasses();
			const auto& axExecutionOrder = xGraph.GetExecutionOrder();

			if (axPasses.GetSize() == 0)
			{
				ImGui::TextDisabled("No passes in render graph");
			}
			else if (axExecutionOrder.GetSize() == 0)
			{
				ImGui::TextDisabled("Render graph not compiled (no execution order)");
			}
			else
			{
				for (u_int uExecIdx = 0; uExecIdx < axExecutionOrder.GetSize(); uExecIdx++)
				{
					u_int uPassIdx = axExecutionOrder.Get(uExecIdx);
					Zenith_Assert(uPassIdx < axPasses.GetSize(), "Pass index out of bounds");
					const Flux_RenderGraph_Pass* pxPass = axPasses.Get(uPassIdx);

					if (s_bHideDisabled && !pxPass->m_bEnabled)
					{
						continue;
					}

					ImGui::PushID(uPassIdx);
					RenderPass(*pxPass, xGraph);
					ImGui::PopID();
				}
			}
		}
		ImGui::End();
	}

	void SetVisible(bool bVisible)
	{
		s_bVisible = bVisible;
	}

	bool IsVisible()
	{
		return s_bVisible;
	}
}

#endif // ZENITH_TOOLS