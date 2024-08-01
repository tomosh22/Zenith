#ifdef ZENITH_TOOLS
#include "imgui.h"
struct Zenith_DebugVariableTree
{
    static std::string s_strRootName;
    static constexpr uint32_t s_uMaxNameLength = 64;

    Zenith_DebugVariableTree()
    {
        m_pxRoot = new Node;
        m_pxRoot->m_xName = { "Root" };
    }

    struct LeafNodeBase
    {
        virtual ~LeafNodeBase() = default;
        virtual void ImGuiDisplay() = 0;

        std::vector<std::string> m_xName;
    };

    template<typename T>
    struct LeafNode : public LeafNodeBase
    {
        T* m_pData;

        LeafNode(std::vector<std::string> xName, T* data)
        {
            m_xName = xName;
            m_pData = data;
        }

        void ImGuiDisplay() override;
    };

    struct Node
    {
        std::vector<std::string> m_xName;
        std::vector<Node*> m_xChildren;
        std::vector<LeafNodeBase*> m_xLeaves;
    };

    void TryAddLeafNode(LeafNodeBase* pxLeafNode, Node* pxNode, std::vector<std::string>& xSplits, uint32_t uCurrentDepth);
    void TryAddNode(Node* pxNodeToAdd, Node* pxNode, std::vector<std::string>& xSplits, uint32_t uCurrentDepth, uint32_t uMaxDepth, bool& bSuccess, Node*& pxResult);
    void AddLeafNode(LeafNodeBase* pxLeafNode, std::vector<std::string>& xSplits);

    Node* m_pxRoot;
};

class Zenith_DebugVariables
{
public:
    static void DebugBoolean(std::vector<std::string> xName, bool& xVar)
    {
        Zenith_DebugVariableTree::LeafNode<bool>* pxLeaf = new Zenith_DebugVariableTree::LeafNode<bool>(xName, &xVar);
        s_xTree.AddLeafNode(pxLeaf, xName);
    }

    static Zenith_DebugVariableTree s_xTree;
};
#endif