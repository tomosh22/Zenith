#include "Zenith.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"

Zenith_DebugVariableTree Zenith_DebugVariables::s_xTree;

template<>
void Zenith_DebugVariableTree::LeafNode<bool>::ImGuiDisplay()
{
    ImGui::Checkbox(
        m_xName.back().c_str(), m_pData);
}
#endif

void Zenith_DebugVariableTree::TryAddNode(Node* pxNodeToAdd, Node* pxNode, std::vector<std::string>& xSplits, uint32_t uCurrentDepth, uint32_t uMaxDepth, bool& bSuccess, Node*& pxResult)
{
    Zenith_Assert(uCurrentDepth < xSplits.size(), "Gone too deep");
    if (uCurrentDepth == uMaxDepth - 1)
    {
        for (Node* pxChildNode : pxNode->m_xChildren)
        {
            if (pxChildNode->m_xName[uCurrentDepth] == xSplits[uCurrentDepth])
            {
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
        if (pxChildNode->m_xName[uCurrentDepth] == xSplits[uCurrentDepth])
        {
            TryAddNode(pxNodeToAdd, pxChildNode, xSplits, uCurrentDepth + 1, uMaxDepth, bSuccess, pxResult);
        }
    }
}

void Zenith_DebugVariableTree::AddLeafNode(LeafNodeBase* pxLeafNode, std::vector<std::string>& xSplits)
{
    Node* pxParent = nullptr;
    for (uint32_t u = 0; u < xSplits.size() -1; u++)
    {
        Node* pxNodeToAdd = new Node;
        for (uint32_t uSub = 0; uSub < u+1; uSub++)
        {
            pxNodeToAdd->m_xName.push_back(xSplits[uSub]);
        }
        bool bSuccess = false;
        TryAddNode(pxNodeToAdd, m_pxRoot, xSplits, 0, u+1, bSuccess, pxParent);
    }
    pxParent->m_xLeaves.push_back(pxLeafNode);
}
