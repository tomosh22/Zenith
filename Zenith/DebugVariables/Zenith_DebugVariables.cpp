#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux.h"
// For g_xEngine.FluxGraphics().m_xRepeatSampler.
#include "Flux/Flux_GraphicsImpl.h"

Zenith_DebugVariableTree::~Zenith_DebugVariableTree()
{
	DeleteNode(m_pxRoot);
}

void Zenith_DebugVariableTree::DeleteNode(Node* pxNode)
{
	for (u_int u = 0; u < pxNode->m_xChildren.GetSize(); u++)
	{
		DeleteNode(pxNode->m_xChildren.Get(u));
	}
	for (u_int u = 0; u < pxNode->m_xLeaves.GetSize(); u++)
	{
		delete pxNode->m_xLeaves.Get(u);
	}
	delete pxNode;
}

template<>
void Zenith_DebugVariableTree::LeafNode<bool>::ImGuiDisplay()
{
	ImGui::Checkbox(m_xName.GetBack().c_str(), m_pData);
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector2, float>::ImGuiDisplay()
{
	ImGui::SliderFloat2(m_xName.GetBack().c_str(), &m_pData->x, m_xMin, m_xMax);
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector3, float>::ImGuiDisplay()
{
	ImGui::SliderFloat3(m_xName.GetBack().c_str(), &m_pData->x, m_xMin, m_xMax);
}
template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>::ImGuiDisplay()
{
	ImGui::SliderFloat4(m_xName.GetBack().c_str(), &m_pData->x, m_xMin, m_xMax);
}
template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::UVector4, float>::ImGuiDisplay()
{
	int iTempX = static_cast<int>(m_pData->x);
	int iTempY = static_cast<int>(m_pData->y);
	int iTempZ = static_cast<int>(m_pData->z);
	int iTempW = static_cast<int>(m_pData->w);
	int aiTemp[4] = { iTempX, iTempY, iTempZ, iTempW };
	ImGui::InputInt4(m_xName.GetBack().c_str(), aiTemp);
	m_pData->x = aiTemp[0];
	m_pData->y = aiTemp[1];
	m_pData->z = aiTemp[2];
	m_pData->w = aiTemp[3];
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<float, float>::ImGuiDisplay()
{
	ImGui::SliderFloat(m_xName.GetBack().c_str(), m_pData, m_xMin, m_xMax, "%.7f");
}

template<>
void Zenith_DebugVariableTree::LeafNode<const Flux_ShaderResourceView>::ImGuiDisplay()
{
	// The backend returns an opaque ID, keeping backend descriptor types out of this system.
	const uint64_t ulTextureID = g_xEngine.FluxBackend().CreateImGuiTextureID(*m_pData, g_xEngine.FluxGraphics().m_xRepeatSampler);
	ImGui::Image(static_cast<ImTextureID>(ulTextureID), ImVec2(1024, 1024), ImVec2(0, 1), ImVec2(1, 0));
}

void Zenith_DebugVariableTree::TextureCallbackLeafNode::ImGuiDisplay()
{
	// nullptr during early startup, before the graph's transients are allocated.
	const Flux_ShaderResourceView* pxSRV = m_pfnGetSRV ? m_pfnGetSRV() : nullptr;
	if (pxSRV == nullptr) return;
	const uint64_t ulTextureID = g_xEngine.FluxBackend().CreateImGuiTextureID(*pxSRV, g_xEngine.FluxGraphics().m_xRepeatSampler);
	ImGui::Image(static_cast<ImTextureID>(ulTextureID), ImVec2(1024, 1024), ImVec2(0, 1), ImVec2(1, 0));
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>::ImGuiDisplay()
{
	ImGui::SliderInt(m_xName.GetBack().c_str(), (int*)(m_pData), m_xMin, m_xMax);
}

template<>
void Zenith_DebugVariableTree::LeafNodeReadOnly<uint32_t>::ImGuiDisplay()
{
	ImGui::Text("%s: %u", m_xName.GetBack().c_str(), *m_pData);
}

template<>
void Zenith_DebugVariableTree::LeafNodeReadOnly<uint64_t>::ImGuiDisplay()
{
	ImGui::Text("%s: %llu", m_xName.GetBack().c_str(), *m_pData);
}

template<>
void Zenith_DebugVariableTree::LeafNodeReadOnly<float>::ImGuiDisplay()
{
	ImGui::Text("%s: %.3f", m_xName.GetBack().c_str(), *m_pData);
}

void Zenith_DebugVariableTree::PfnLeafNode::ImGuiDisplay()
{
	if (ImGui::Button(m_xName.GetBack().c_str()))
	{
		(*m_pData)();
	}
}

void Zenith_DebugVariableTree::TextNode::ImGuiDisplay()
{
	ImGui::Text(m_strText.c_str());
}

bool Zenith_DebugVariableTree::TryAddNode(Node* pxNodeToAdd, Node* pxNode, std::vector<std::string>& xSplits, uint32_t uCurrentDepth, uint32_t uMaxDepth, Node*& pxResult)
{
	Zenith_Assert(uCurrentDepth < xSplits.size() - 1, "Gone too deep");
	if (uCurrentDepth == uMaxDepth)
	{
		for (Node* pxChildNode : pxNode->m_xChildren)
		{
			if (pxChildNode->m_xName.Get(uMaxDepth + 1) == xSplits[uMaxDepth])
			{
				delete pxNodeToAdd;
				pxResult = pxChildNode;
				return true;
			}
		}
		pxNode->m_xChildren.PushBack(pxNodeToAdd);
		pxResult = pxNodeToAdd;
		return true;
	}
	for (Node* pxChildNode : pxNode->m_xChildren)
	{
		if (pxChildNode->m_xName.Get(uCurrentDepth + 1) == xSplits[uCurrentDepth])
		{
			if (TryAddNode(pxNodeToAdd, pxChildNode, xSplits, uCurrentDepth + 1, uMaxDepth, pxResult))
			{
				return true;
			}
		}
	}
	return false;
}

void Zenith_DebugVariableTree::AddLeafNode(LeafNodeBase* pxLeafNode, std::vector<std::string>& xSplits)
{
	Node* pxParent = m_pxRoot;
	// xSplits.size() - 1 below underflows for sizes 0 and 1
	if (xSplits.size() < 2)
	{
		pxParent->m_xLeaves.PushBack(pxLeafNode);
		return;
	}
	for (uint32_t u = 0; u < xSplits.size() - 1; u++)
	{
		Node* pxNodeToAdd = new Node;
		pxNodeToAdd->m_xName.PushBack(ROOT_NAME);
		for (uint32_t uSub = 0; uSub < u + 1; uSub++)
		{
			pxNodeToAdd->m_xName.PushBack(xSplits[uSub]);
		}
		TryAddNode(pxNodeToAdd, m_pxRoot, xSplits, 0, u, pxParent);
	}
	pxParent->m_xLeaves.PushBack(pxLeafNode);
}

#endif