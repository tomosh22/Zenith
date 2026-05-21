#pragma once

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

	// Callback-backed texture preview. The function pointer is invoked on every
	// ImGui pass to fetch the current SRV, so the displayed texture stays correct
	// across render-graph rebuilds that invalidate the SRV stored on the
	// previously-registered transient. Returning nullptr renders nothing.
	struct TextureCallbackLeafNode : public LeafNodeBase
	{
		const Flux_ShaderResourceView*(*m_pfnGetSRV)() = nullptr;

		TextureCallbackLeafNode(std::vector<std::string>& xName, const Flux_ShaderResourceView*(*pfnGetSRV)())
		{
			for (const std::string& strSection : xName)
			{
				m_xName.push_back(strSection);
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
				m_xName.push_back(strSection);
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
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddVector2(std::vector<std::string> xName, Zenith_Maths::Vector2& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector2, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector2, float>(xName, &xVar, fMin, fMax);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddVector3(std::vector<std::string> xName, Zenith_Maths::Vector3& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector3, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector3, float>(xName, &xVar, fMin, fMax);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddUVector4(std::vector<std::string> xName, Zenith_Maths::UVector4& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::UVector4, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::UVector4, float>(xName, &xVar, fMin, fMax);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddVector4(std::vector<std::string> xName, Zenith_Maths::Vector4& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>(xName, &xVar, fMin, fMax);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddVector3(std::vector<std::string> xName, Zenith_Maths::Vector4& xVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>(xName, &xVar, fMin, fMax);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddFloat(std::vector<std::string> xName, float& fVar, float fMin, float fMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<float, float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<float, float>(xName, &fVar, fMin, fMax);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddUInt32(std::vector<std::string> xName, uint32_t& xVar, uint32_t uMin, uint32_t uMax)
	{
		Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>(xName, &xVar, uMin, uMax);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	template<typename T>
	static void AddUInt32(std::vector<std::string> xName, T& xVar, uint32_t uMin, uint32_t uMax)
	{
		static_assert(std::is_enum<T>(), "Not an enum");
		Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>(xName, reinterpret_cast<uint32_t*>(&xVar), uMin, uMax);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddUInt32_ReadOnly(std::vector<std::string> xName, uint32_t& xVar)
	{
		Zenith_DebugVariableTree::LeafNodeReadOnly<uint32_t>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeReadOnly<uint32_t>(xName, &xVar);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddUInt64_ReadOnly(std::vector<std::string> xName, uint64_t& xVar)
	{
		Zenith_DebugVariableTree::LeafNodeReadOnly<uint64_t>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeReadOnly<uint64_t>(xName, &xVar);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddFloat_ReadOnly(std::vector<std::string> xName, float& fVar)
	{
		Zenith_DebugVariableTree::LeafNodeReadOnly<float>* pxLeaf = new Zenith_DebugVariableTree::LeafNodeReadOnly<float>(xName, &fVar);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddButton(std::vector<std::string> xName, void(*pfnCallback)())
	{
		Zenith_DebugVariableTree::PfnLeafNode* pxLeaf = new Zenith_DebugVariableTree::PfnLeafNode(xName, pfnCallback);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddTexture(std::vector<std::string> xName, const Flux_ShaderResourceView& xTexture)
	{
		Zenith_DebugVariableTree::LeafNode<const Flux_ShaderResourceView>* pxLeaf = new Zenith_DebugVariableTree::LeafNode(xName, &xTexture);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	// Register a texture preview whose SRV is resolved on every ImGui draw via
	// the supplied callback. Use this for render-graph transients whose backing
	// SRV is recreated on resize / graph rebuild — storing a stale pointer via
	// AddTexture would point at destroyed memory once the graph rebuilds.
	static void AddTextureCallback(std::vector<std::string> xName, const Flux_ShaderResourceView*(*pfnGetSRV)())
	{
		Zenith_DebugVariableTree::TextureCallbackLeafNode* pxLeaf = new Zenith_DebugVariableTree::TextureCallbackLeafNode(xName, pfnGetSRV);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}
	static void AddText(std::vector<std::string> xName, std::string& strText)
	{
		Zenith_DebugVariableTree::TextNode* pxLeaf = new Zenith_DebugVariableTree::TextNode(xName, strText);
		AddLeafNodeToEngineTree(pxLeaf, xName);
	}

private:
	// Phase 5.7: tree state lives on Zenith_DebugVariablesImpl held by
	// Zenith_Engine. Every inline Add* method above forwards through this
	// non-inline helper so the header doesn't have to drag Zenith_Engine.h
	// (and the Impl) into the 35 TUs that call Add*. Helper body lives in
	// Zenith_DebugVariables.cpp.
	static void AddLeafNodeToEngineTree(Zenith_DebugVariableTree::LeafNodeBase* pxLeaf, std::vector<std::string>& xName);
};

#endif