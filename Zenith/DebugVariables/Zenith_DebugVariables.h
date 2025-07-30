#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Flux/Flux.h"

#define ROOT_NAME "Debug Variables"

class Zenith_DebugVariableTree
{
public:
	static constexpr uint32_t s_uMaxNameLength = 64;

	Zenith_DebugVariableTree()
	{
		m_pxRoot = new Node;
		m_pxRoot->m_xName = { ROOT_NAME };
	}

	struct LeafNodeBase
	{
		virtual ~LeafNodeBase() = default;
		virtual void ImGuiDisplay() = 0;

		std::vector<std::string> m_xName = { ROOT_NAME };
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

	struct TextNode : public LeafNodeBase
	{
		const std::string& m_strText;

		TextNode(std::vector<std::string>& xName, const std::string& strText) : m_strText(strText)
		{
			for (const std::string& strSection : xName)
			{
				m_xName.push_back(strSection);
			}
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
	static void AddVector4(std::vector<std::string> xName, Zenith_Maths::Vector4& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>(xName, &xVar, fMin, fMax);
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
	static void AddTexture(std::vector<std::string> xName, const Flux_Texture& xTexture)
	{
		Zenith_DebugVariableTree::LeafNode<const Flux_Texture>* pxLeaf = new Zenith_DebugVariableTree::LeafNode(xName, &xTexture);
		s_xTree.AddLeafNode(pxLeaf, xName);
	}
	static void AddText(std::vector<std::string> xName, std::string& strText)
	{
		Zenith_DebugVariableTree::TextNode* pxLeaf = new Zenith_DebugVariableTree::TextNode(xName, strText);
		s_xTree.AddLeafNode(pxLeaf, xName);
	}

	static Zenith_DebugVariableTree s_xTree;
};

#endif