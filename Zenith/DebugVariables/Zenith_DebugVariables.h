#ifdef ZENITH_TOOLS
#include "imgui.h"
class Zenith_DebugVariableTree
{
public:
	static constexpr char* s_szRootName = "Debug Variables";
	static constexpr uint32_t s_uMaxNameLength = 64;

	Zenith_DebugVariableTree()
	{
		m_pxRoot = new Node;
		m_pxRoot->m_xName = { s_szRootName };
	}

	struct LeafNodeBase
	{
		virtual ~LeafNodeBase() = default;
		virtual void ImGuiDisplay() = 0;

		std::vector<std::string> m_xName = { s_szRootName };
	};

	template<typename T>
	struct LeafNode : public LeafNodeBase
	{
		T* m_pData = nullptr;

		LeafNode(std::vector<std::string>& xName, T* data)
		{
			for (const std::string& strSection : xName)
			{
				m_xName.push_back(strSection);
			}
			m_pData = data;
		}

		void ImGuiDisplay() override;
	};

	struct PfnLeafNode : public LeafNodeBase
	{
		void(*m_pData)() = nullptr;

		PfnLeafNode(std::vector<std::string>& xName, void(*pfnData)())
		{
			for (const std::string& strSection : xName)
			{
				m_xName.push_back(strSection);
			}
			m_pData = pfnData;
		}

		void ImGuiDisplay() override;
	};

	template<typename ValueT, typename RangeT>
	struct LeafNodeWithRange : public LeafNodeBase
	{
		ValueT* m_pData;
		RangeT m_xMin, m_xMax;

		LeafNodeWithRange(std::vector<std::string>& xName, ValueT* data, RangeT xMin, RangeT xMax)
			: m_xMin(xMin)
			, m_xMax(xMax)
		{
			for (const std::string& strSection : xName)
			{
				m_xName.push_back(strSection);
			}
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

	void TryAddNode(Node* pxNodeToAdd, Node* pxNode, std::vector<std::string>& xSplits, uint32_t uCurrentDepth, uint32_t uMaxDepth, bool& bSuccess, Node*& pxResult);
	void AddLeafNode(LeafNodeBase* pxLeafNode, std::vector<std::string>& xSplits);

	Node* m_pxRoot;
};

class Zenith_DebugVariables
{
public:
	static void AddBoolean(std::vector<std::string> xName, bool& bVar)
	{
		Zenith_DebugVariableTree::LeafNode<bool>* pxLeaf = new Zenith_DebugVariableTree::LeafNode<bool>(xName, &bVar);
		s_xTree.AddLeafNode(pxLeaf, xName);
	}
	static void AddVector2(std::vector<std::string> xName, Zenith_Maths::Vector2& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector2, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector2, float>(xName, &xVar, fMin, fMax);
		s_xTree.AddLeafNode(pxLeaf, xName);
	}
	static void AddVector3(std::vector<std::string> xName, Zenith_Maths::Vector3& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector3, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector3, float>(xName, &xVar, fMin, fMax);
		s_xTree.AddLeafNode(pxLeaf, xName);
	}
	static void AddVector3(std::vector<std::string> xName, Zenith_Maths::Vector4& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>(xName, &xVar, fMin, fMax);
		s_xTree.AddLeafNode(pxLeaf, xName);
	}
	static void AddFloat(std::vector<std::string> xName, float& fVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<float, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<float, float>(xName, &fVar, fMin, fMax);
		s_xTree.AddLeafNode(pxLeaf, xName);
	}
	static void AddUInt32(std::vector<std::string> xName, uint32_t& xVar, uint32_t uMin, uint32_t uMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>(xName, &xVar, uMin, uMax);
		s_xTree.AddLeafNode(pxLeaf, xName);
	}
	static void AddButton(std::vector<std::string> xName, void(*pfnCallback)())
	{
		Zenith_DebugVariableTree::PfnLeafNode* pxLeaf = new Zenith_DebugVariableTree::PfnLeafNode(xName, pfnCallback);
		s_xTree.AddLeafNode(pxLeaf, xName);
	}

	static Zenith_DebugVariableTree s_xTree;
};

#endif