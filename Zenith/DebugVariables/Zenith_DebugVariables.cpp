#include "Zenith.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"

Zenith_DebugVariableTree Zenith_DebugVariables::s_xTree;

template<>
void Zenith_DebugVariableTree::LeafNode<bool>::ImGuiDisplay()
{
	ImGui::Checkbox(m_xName.back().c_str(), m_pData);
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector3, float>::ImGuiDisplay()
{
	ImGui::SliderFloat3(m_xName.back().c_str(), &m_pData->x, m_xMin, m_xMax);
}
template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>::ImGuiDisplay()
{
	ImGui::SliderFloat3(m_xName.back().c_str(), &m_pData->x, m_xMin, m_xMax);
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<float, float>::ImGuiDisplay()
{
	ImGui::SliderFloat(m_xName.back().c_str(), m_pData, m_xMin, m_xMax, "%.7f");
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>::ImGuiDisplay()
{
	ImGui::SliderInt(m_xName.back().c_str(), (int*)(m_pData), m_xMin, m_xMax);
}

template<>
void Zenith_DebugVariableTree::LeafNode<void(*)()>::ImGuiDisplay()
{
	if (ImGui::Button(m_xName.back().c_str()))
	{
		(*m_pData)();
	}
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
	for (uint32_t u = 0; u < xSplits.size() - 1; u++)
	{
		Node* pxNodeToAdd = new Node;
		pxNodeToAdd->m_xName.push_back(s_szRootName);
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