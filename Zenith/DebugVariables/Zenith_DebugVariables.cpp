#include "Zenith.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux.h"
// Pulled in for g_xEngine.FluxGraphics().m_xRepeatSampler (used by the texture preview
// widget); CreateImGuiTextureID itself is on Flux_PlatformAPI and reachable
// via the platform-graphics include already in Flux.h.
#include "Flux/Flux_GraphicsImpl.h"

template<>
void Zenith_DebugVariableTree::LeafNode<bool>::ImGuiDisplay()
{
	ImGui::Checkbox(m_xName.back().c_str(), m_pData);
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector2, float>::ImGuiDisplay()
{
	ImGui::SliderFloat2(m_xName.back().c_str(), &m_pData->x, m_xMin, m_xMax);
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector3, float>::ImGuiDisplay()
{
	ImGui::SliderFloat3(m_xName.back().c_str(), &m_pData->x, m_xMin, m_xMax);
}
template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>::ImGuiDisplay()
{
	ImGui::SliderFloat4(m_xName.back().c_str(), &m_pData->x, m_xMin, m_xMax);
}
template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::UVector4, float>::ImGuiDisplay()
{
	int iTempX = static_cast<int>(m_pData->x);
	int iTempY = static_cast<int>(m_pData->y);
	int iTempZ = static_cast<int>(m_pData->z);
	int iTempW = static_cast<int>(m_pData->w);
	int aiTemp[4] = { iTempX, iTempY, iTempZ, iTempW };
	ImGui::InputInt4(m_xName.back().c_str(), aiTemp);
	m_pData->x = aiTemp[0];
	m_pData->y = aiTemp[1];
	m_pData->z = aiTemp[2];
	m_pData->w = aiTemp[3];
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<float, float>::ImGuiDisplay()
{
	ImGui::SliderFloat(m_xName.back().c_str(), m_pData, m_xMin, m_xMax, "%.7f");
}

template<>
void Zenith_DebugVariableTree::LeafNode<const Flux_ShaderResourceView>::ImGuiDisplay()
{
	// Engine-typed wrapper — backend allocates the per-frame descriptor set
	// and returns an opaque uint64 that ImGui treats as a texture ID. Avoids
	// dragging Vulkan descriptor / image-view types into the debug-variable
	// system; portable to backends with different ImGui texture-ID conventions
	// once each backend implements CreateImGuiTextureID.
	const uint64_t ulTextureID = g_xEngine.Vulkan().CreateImGuiTextureID(*m_pData, g_xEngine.FluxGraphics().m_xRepeatSampler);
	ImGui::Image(static_cast<ImTextureID>(ulTextureID), ImVec2(1024, 1024), ImVec2(0, 1), ImVec2(1, 0));
}

void Zenith_DebugVariableTree::TextureCallbackLeafNode::ImGuiDisplay()
{
	// Resolve the current SRV each draw so the preview follows render-graph
	// rebuilds. Callback may return nullptr during early startup before the
	// graph's transients are allocated — in that case render nothing.
	const Flux_ShaderResourceView* pxSRV = m_pfnGetSRV ? m_pfnGetSRV() : nullptr;
	if (pxSRV == nullptr) return;
	const uint64_t ulTextureID = g_xEngine.Vulkan().CreateImGuiTextureID(*pxSRV, g_xEngine.FluxGraphics().m_xRepeatSampler);
	ImGui::Image(static_cast<ImTextureID>(ulTextureID), ImVec2(1024, 1024), ImVec2(0, 1), ImVec2(1, 0));
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>::ImGuiDisplay()
{
	ImGui::SliderInt(m_xName.back().c_str(), (int*)(m_pData), m_xMin, m_xMax);
}

template<>
void Zenith_DebugVariableTree::LeafNodeReadOnly<uint32_t>::ImGuiDisplay()
{
	ImGui::Text("%s: %u", m_xName.back().c_str(), *m_pData);
}

template<>
void Zenith_DebugVariableTree::LeafNodeReadOnly<uint64_t>::ImGuiDisplay()
{
	ImGui::Text("%s: %llu", m_xName.back().c_str(), *m_pData);
}

template<>
void Zenith_DebugVariableTree::LeafNodeReadOnly<float>::ImGuiDisplay()
{
	ImGui::Text("%s: %.3f", m_xName.back().c_str(), *m_pData);
}

void Zenith_DebugVariableTree::PfnLeafNode::ImGuiDisplay()
{
	if (ImGui::Button(m_xName.back().c_str()))
	{
		(*m_pData)();
	}
}

void Zenith_DebugVariableTree::TextNode::ImGuiDisplay()
{
	ImGui::Text(m_strText.c_str());
}

void Zenith_DebugVariableTree::TryAddNode(Node* pxNodeToAdd, Node* pxNode, std::vector<std::string>& xSplits, uint32_t uCurrentDepth, uint32_t uMaxDepth, bool& bSuccess, Node*& pxResult)
{
	Zenith_Assert(uCurrentDepth < xSplits.size() - 1, "Gone too deep");
	if (uCurrentDepth == uMaxDepth)
	{
		for (Node* pxChildNode : pxNode->m_xChildren)
		{
			if (pxChildNode->m_xName[uMaxDepth + 1] == xSplits[uMaxDepth])
			{
				delete pxNodeToAdd;
				pxResult = pxChildNode;
				bSuccess = true;
				return;
			}
		}
		pxNode->m_xChildren.push_back(pxNodeToAdd);
		pxResult = pxNodeToAdd;
		bSuccess = true;
		return;
	}
	for (Node* pxChildNode : pxNode->m_xChildren)
	{
		if (pxChildNode->m_xName[uCurrentDepth + 1] == xSplits[uCurrentDepth])
		{
			TryAddNode(pxNodeToAdd, pxChildNode, xSplits, uCurrentDepth + 1, uMaxDepth, bSuccess, pxResult);
		}
	}
}

void Zenith_DebugVariableTree::AddLeafNode(LeafNodeBase* pxLeafNode, std::vector<std::string>& xSplits)
{
	Node* pxParent = m_pxRoot;
	// Guard against underflow when xSplits.size() is 0 or 1
	if (xSplits.size() < 2)
	{
		pxParent->m_xLeaves.push_back(pxLeafNode);
		return;
	}
	for (uint32_t u = 0; u < xSplits.size() - 1; u++)
	{
		Node* pxNodeToAdd = new Node;
		pxNodeToAdd->m_xName.push_back(ROOT_NAME);
		for (uint32_t uSub = 0; uSub < u + 1; uSub++)
		{
			pxNodeToAdd->m_xName.push_back(xSplits[uSub]);
		}
		bool bSuccess = false;
		TryAddNode(pxNodeToAdd, m_pxRoot, xSplits, 0, u, bSuccess, pxParent);
	}
	pxParent->m_xLeaves.push_back(pxLeafNode);
}

#endif