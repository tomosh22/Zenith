#pragma once

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Collections/Zenith_Vector.h"

// Local forward-decl (not Flux_Fwd.h): this L0 header must stay off the Flux (L2)
// layer; the SRV is only touched by pointer here, dereferences live in the .cpp.
struct Flux_ShaderResourceView;

#define ROOT_NAME "Debug Variables"

class Zenith_DebugVariableTree
{
public:
	static constexpr uint32_t s_uMaxNameLength = 64;

	Zenith_DebugVariableTree()
	{
		m_pxRoot = new Node;
		m_pxRoot->m_xName.PushBack(ROOT_NAME);
	}
	~Zenith_DebugVariableTree();
	Zenith_DebugVariableTree(const Zenith_DebugVariableTree&) = delete;
	Zenith_DebugVariableTree& operator=(const Zenith_DebugVariableTree&) = delete;

	struct LeafNodeBase
	{
		LeafNodeBase()
		{
			m_xName.PushBack(ROOT_NAME);
		}
		virtual ~LeafNodeBase() = default;
		virtual void ImGuiDisplay() = 0;

		Zenith_Vector<std::string> m_xName;
	};

	template<typename T>
	struct LeafNode : public LeafNodeBase
	{
		T* m_pData = nullptr;

		LeafNode(std::vector<std::string>& xName, T* data)
		{
			for (const std::string& strSection : xName)
			{
				m_xName.PushBack(strSection);
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
				m_xName.PushBack(strSection);
			}
			m_pData = pfnData;
		}

		void ImGuiDisplay() override;
	};

	// Fetches the SRV via callback on every draw so the preview survives
	// render-graph rebuilds; a nullptr result renders nothing.
	struct TextureCallbackLeafNode : public LeafNodeBase
	{
		const Flux_ShaderResourceView*(*m_pfnGetSRV)() = nullptr;

		TextureCallbackLeafNode(std::vector<std::string>& xName, const Flux_ShaderResourceView*(*pfnGetSRV)())
		{
			for (const std::string& strSection : xName)
			{
				m_xName.PushBack(strSection);
			}
			m_pfnGetSRV = pfnGetSRV;
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
				m_xName.PushBack(strSection);
			}
			m_pData = data;
		}

		void ImGuiDisplay() override;
	};

	template<typename T>
	struct LeafNodeReadOnly : public LeafNodeBase
	{
		T* m_pData = nullptr;

		LeafNodeReadOnly(std::vector<std::string>& xName, T* data)
		{
			for (const std::string& strSection : xName)
			{
				m_xName.PushBack(strSection);
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
				m_xName.PushBack(strSection);
			}
		}

		void ImGuiDisplay() override;
	};

	struct Node
	{
		Zenith_Vector<std::string> m_xName;
		Zenith_Vector<Node*> m_xChildren;
		Zenith_Vector<LeafNodeBase*> m_xLeaves;
	};

	bool TryAddNode(Node* pxNodeToAdd, Node* pxNode, std::vector<std::string>& xSplits, uint32_t uCurrentDepth, uint32_t uMaxDepth, Node*& pxResult);
	void AddLeafNode(LeafNodeBase* pxLeafNode, std::vector<std::string>& xSplits);

	Node* m_pxRoot;

private:
	static void DeleteNode(Node* pxNode);
};

// Held on g_xEngine, accessed via g_xEngine.DebugVariables().
class Zenith_DebugVariables
{
public:
	Zenith_DebugVariables() = default;
	~Zenith_DebugVariables() = default;
	Zenith_DebugVariables(const Zenith_DebugVariables&) = delete;
	Zenith_DebugVariables& operator=(const Zenith_DebugVariables&) = delete;

	Zenith_DebugVariableTree m_xTree;

	inline void AddBoolean(std::vector<std::string> xName, bool& bVar)
	{
		Zenith_DebugVariableTree::LeafNode<bool>* pxLeaf = new Zenith_DebugVariableTree::LeafNode<bool>(xName, &bVar);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddVector2(std::vector<std::string> xName, Zenith_Maths::Vector2& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector2, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector2, float>(xName, &xVar, fMin, fMax);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddVector3(std::vector<std::string> xName, Zenith_Maths::Vector3& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector3, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector3, float>(xName, &xVar, fMin, fMax);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddUVector4(std::vector<std::string> xName, Zenith_Maths::UVector4& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::UVector4, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::UVector4, float>(xName, &xVar, fMin, fMax);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddVector4(std::vector<std::string> xName, Zenith_Maths::Vector4& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>(xName, &xVar, fMin, fMax);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddFloat(std::vector<std::string> xName, float& fVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<float, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<float, float>(xName, &fVar, fMin, fMax);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddUInt32(std::vector<std::string> xName, uint32_t& xVar, uint32_t uMin, uint32_t uMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>(xName, &xVar, uMin, uMax);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	template<typename T>
	inline void AddUInt32(std::vector<std::string> xName, T& xVar, uint32_t uMin, uint32_t uMax)
	{
		static_assert(std::is_enum<T>(), "Not an enum");
		Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>(xName, reinterpret_cast<uint32_t*>(&xVar), uMin, uMax);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddUInt32_ReadOnly(std::vector<std::string> xName, uint32_t& xVar)
	{
		Zenith_DebugVariableTree::LeafNodeReadOnly<uint32_t>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeReadOnly<uint32_t>(xName, &xVar);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddUInt64_ReadOnly(std::vector<std::string> xName, uint64_t& xVar)
	{
		Zenith_DebugVariableTree::LeafNodeReadOnly<uint64_t>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeReadOnly<uint64_t>(xName, &xVar);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddFloat_ReadOnly(std::vector<std::string> xName, float& fVar)
	{
		Zenith_DebugVariableTree::LeafNodeReadOnly<float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeReadOnly<float>(xName, &fVar);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddButton(std::vector<std::string> xName, void(*pfnCallback)())
	{
		Zenith_DebugVariableTree::PfnLeafNode* pxLeaf = new Zenith_DebugVariableTree::PfnLeafNode(xName, pfnCallback);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddTexture(std::vector<std::string> xName, const Flux_ShaderResourceView& xTexture)
	{
		Zenith_DebugVariableTree::LeafNode<const Flux_ShaderResourceView>* pxLeaf = new Zenith_DebugVariableTree::LeafNode(xName, &xTexture);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddTextureCallback(std::vector<std::string> xName, const Flux_ShaderResourceView*(*pfnGetSRV)())
	{
		Zenith_DebugVariableTree::TextureCallbackLeafNode* pxLeaf = new Zenith_DebugVariableTree::TextureCallbackLeafNode(xName, pfnGetSRV);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
	inline void AddText(std::vector<std::string> xName, std::string& strText)
	{
		Zenith_DebugVariableTree::TextNode* pxLeaf = new Zenith_DebugVariableTree::TextNode(xName, strText);
		m_xTree.AddLeafNode(pxLeaf, xName);
	}
};

#endif