#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIButton.h"

#include <cstdio>

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library - UI domain.
//
// Elements are name-addressed (recursive first-match lookup, no handles) and
// canvas-owned: nodes re-find their element every Execute and never cache
// element pointers. FindElement<T> is an unchecked static_cast (RTTI off),
// so every typed node guards on GetType() before down-casting.
//
// OnUIButtonClicked is an ON_UPDATE-anchored self-wiring source: each tick
// it (idempotently) installs a captureless trampoline on the named button
// whose userdata is a file-scope, never-freed click-counter context - no
// pointer into graphs/nodes/buttons ever crosses a lifetime boundary, so
// hot reload / slot removal / element recreation cannot dangle. A button
// watched by this node is OWNED by it: game-side SetOnClick wiring on the
// same button is overwritten every tick.
//------------------------------------------------------------------------------

namespace
{
	Zenith_UIComponent* ResolveTargetUI(Zenith_GraphContext& xContext, const std::string& strTargetVar)
	{
		Zenith_Entity xTarget = xContext.ResolveTargetEntity(strTargetVar);
		if (!xTarget.IsValid())
		{
			return nullptr;
		}
		return xTarget.TryGetComponent<Zenith_UIComponent>();
	}

	// Blackboard value -> display string (type-dispatched; the property
	// getters assert on mismatch, so switch on GetType first). iDecimals < 0
	// = shortest float form.
	std::string PropertyValueToDisplayString(const Zenith_PropertyValue& xValue, int32_t iDecimals)
	{
		char acBuffer[128];
		switch (xValue.GetType())
		{
		case PROPERTY_TYPE_FLOAT:
			if (iDecimals < 0)
			{
				std::snprintf(acBuffer, sizeof(acBuffer), "%g", xValue.GetFloat());
			}
			else
			{
				std::snprintf(acBuffer, sizeof(acBuffer), "%.*f", iDecimals, xValue.GetFloat());
			}
			return acBuffer;
		case PROPERTY_TYPE_INT32:
			std::snprintf(acBuffer, sizeof(acBuffer), "%d", xValue.GetInt32());
			return acBuffer;
		case PROPERTY_TYPE_BOOL:
			return xValue.GetBool() ? "true" : "false";
		case PROPERTY_TYPE_STRING:
			return xValue.GetString();
		case PROPERTY_TYPE_VECTOR2:
		{
			const Zenith_Maths::Vector2 xVec = xValue.GetVector2();
			std::snprintf(acBuffer, sizeof(acBuffer), "(%g, %g)", xVec.x, xVec.y);
			return acBuffer;
		}
		case PROPERTY_TYPE_VECTOR3:
		{
			const Zenith_Maths::Vector3 xVec = xValue.GetVector3();
			std::snprintf(acBuffer, sizeof(acBuffer), "(%g, %g, %g)", xVec.x, xVec.y, xVec.z);
			return acBuffer;
		}
		case PROPERTY_TYPE_VECTOR4:
		{
			const Zenith_Maths::Vector4 xVec = xValue.GetVector4();
			std::snprintf(acBuffer, sizeof(acBuffer), "(%g, %g, %g, %g)", xVec.x, xVec.y, xVec.z, xVec.w);
			return acBuffer;
		}
		default:
			return "";
		}
	}

	// Sets an element's text. m_strValueVar formats a blackboard value in:
	// the first "{}" in m_strText is replaced by the value; with no
	// placeholder the value is appended (the "Score: " label pattern).
	// Works on Text and Button elements.
	class Zenith_GraphNode_SetUIText : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetUIText)
	public:
		ZENITH_PROPERTY(std::string, m_strElement, "")
		ZENITH_PROPERTY(std::string, m_strText, "")
		ZENITH_PROPERTY(std::string, m_strValueVar, "")
		ZENITH_PROPERTY(int32_t, m_iDecimals, -1)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_UIComponent* pxUI = ResolveTargetUI(xContext, m_strTargetVar);
			if (pxUI == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(m_strElement);
			if (pxElement == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}

			std::string strText = m_strText;
			if (!m_strValueVar.empty())
			{
				const Zenith_PropertyValue* pxValue = xContext.m_pxBlackboard->TryGetValue(m_strValueVar);
				const std::string strValue = pxValue ? PropertyValueToDisplayString(*pxValue, m_iDecimals) : "";
				const size_t uPlaceholder = strText.find("{}");
				if (uPlaceholder != std::string::npos)
				{
					strText.replace(uPlaceholder, 2, strValue);
				}
				else
				{
					strText += strValue;
				}
			}

			if (pxElement->GetType() == Zenith_UI::UIElementType::Text)
			{
				static_cast<Zenith_UI::Zenith_UIText*>(pxElement)->SetText(strText);
				return GRAPH_NODE_STATUS_SUCCESS;
			}
			if (pxElement->GetType() == Zenith_UI::UIElementType::Button)
			{
				static_cast<Zenith_UI::Zenith_UIButton*>(pxElement)->SetText(strText);
				return GRAPH_NODE_STATUS_SUCCESS;
			}
			return GRAPH_NODE_STATUS_FAILURE;	// not a text-bearing element
		}
		const char* GetTypeName() const override { return "SetUIText"; }
	};

	// Sets an element's color (RGBA 0-1; const or vec4 var). Buttons render
	// per-state styles and ignore the base color - for them this sets the
	// NORMAL state (snapping the current style) and leaves hover/pressed
	// styling alone.
	class Zenith_GraphNode_SetUIColor : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetUIColor)
	public:
		ZENITH_PROPERTY(std::string, m_strElement, "")
		ZENITH_PROPERTY(Zenith_Maths::Vector4, m_xColor, Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f))
		ZENITH_PROPERTY(std::string, m_strColorVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_UIComponent* pxUI = ResolveTargetUI(xContext, m_strTargetVar);
			if (pxUI == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(m_strElement);
			if (pxElement == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector4 xColor = m_strColorVar.empty()
				? m_xColor : xContext.m_pxBlackboard->GetVector4(m_strColorVar, m_xColor);
			if (pxElement->GetType() == Zenith_UI::UIElementType::Button)
			{
				static_cast<Zenith_UI::Zenith_UIButton*>(pxElement)->SetNormalColor(xColor);
			}
			else
			{
				pxElement->SetColor(xColor);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetUIColor"; }
	};

	// Shows/hides one element ("" = the whole canvas/component - which also
	// stops its button input processing).
	class Zenith_GraphNode_SetUIVisible : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetUIVisible)
	public:
		ZENITH_PROPERTY(std::string, m_strElement, "")
		ZENITH_PROPERTY(bool, m_bVisible, true)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_UIComponent* pxUI = ResolveTargetUI(xContext, m_strTargetVar);
			if (pxUI == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			if (m_strElement.empty())
			{
				pxUI->SetVisible(m_bVisible);
				return GRAPH_NODE_STATUS_SUCCESS;
			}
			Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(m_strElement);
			if (pxElement == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxElement->SetVisible(m_bVisible);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetUIVisible"; }
	};

	// Progress-bar fill on a Rect element (0-1, engine-clamped; const or
	// float var). Wrong element type = FAILURE, never a blind cast.
	class Zenith_GraphNode_SetUIFillAmount : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetUIFillAmount)
	public:
		ZENITH_PROPERTY(std::string, m_strElement, "")
		ZENITH_PROPERTY_RANGED(float, m_fAmount, 1.0f, 0.0f, 1.0f)
		ZENITH_PROPERTY(std::string, m_strAmountVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_UIComponent* pxUI = ResolveTargetUI(xContext, m_strTargetVar);
			if (pxUI == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(m_strElement);
			if (pxElement == nullptr || pxElement->GetType() != Zenith_UI::UIElementType::Rect)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const float fAmount = m_strAmountVar.empty()
				? m_fAmount : xContext.m_pxBlackboard->GetFloat(m_strAmountVar, m_fAmount);
			static_cast<Zenith_UI::Zenith_UIRect*>(pxElement)->SetFillAmount(fAmount);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetUIFillAmount"; }
	};

	//==========================================================================
	// OnUIButtonClicked - the UIButton -> graph trampoline.
	//==========================================================================

	// One context per watched (entity, button). Owned by the file-scope store
	// for process lifetime (bounded by the number of watched buttons) so the
	// button's userdata pointer can never dangle - the context holds a bare
	// counter and no object references.
	struct UIButtonClickContext
	{
		u_int64 m_ulEntityPacked = 0;
		std::string m_strButton;
		u_int m_uClickCount = 0;
	};

	Zenith_Vector<UIButtonClickContext*> g_axButtonClickContexts;

	UIButtonClickContext& GetOrCreateClickContext(u_int64 ulEntityPacked, const std::string& strButton)
	{
		UIButtonClickContext* pxDead = nullptr;
		for (u_int u = 0; u < g_axButtonClickContexts.GetSize(); ++u)
		{
			UIButtonClickContext* pxContext = g_axButtonClickContexts.Get(u);
			if (pxContext->m_ulEntityPacked == ulEntityPacked && pxContext->m_strButton == strButton)
			{
				return *pxContext;
			}
			// A context whose entity died is unreachable by any trampoline
			// (the button lived on that entity's canvas) - reusable, which
			// bounds the store across scene reload / entity churn.
			if (pxDead == nullptr
				&& !g_xEngine.Scenes().ResolveEntity(Zenith_EntityID::FromPacked(pxContext->m_ulEntityPacked)).IsValid())
			{
				pxDead = pxContext;
			}
		}
		if (pxDead != nullptr)
		{
			pxDead->m_ulEntityPacked = ulEntityPacked;
			pxDead->m_strButton = strButton;
			pxDead->m_uClickCount = 0;
			return *pxDead;
		}
		UIButtonClickContext* pxContext = new UIButtonClickContext();
		pxContext->m_ulEntityPacked = ulEntityPacked;
		pxContext->m_strButton = strButton;
		g_axButtonClickContexts.PushBack(pxContext);
		return *pxContext;
	}

	// Captureless per the engine's fn-pointer mandate; fires from inside
	// Zenith_UISystem's click pass (or Activate() in tests).
	void UIButtonClickTrampoline(void* pxUserData)
	{
		++static_cast<UIButtonClickContext*>(pxUserData)->m_uClickCount;
	}

	// ON_UPDATE-anchored source: wires the trampoline (idempotently) each
	// tick, then gates on clicks recorded since the last tick. Clicks land
	// during the UI pass AFTER scene update, so the chain runs on the NEXT
	// frame's OnUpdate dispatch (one frame of latency by design).
	class Zenith_GraphNode_OnUIButtonClicked : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnUIButtonClicked)
	public:
		ZENITH_PROPERTY(std::string, m_strButton, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (m_strButton.empty())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_UIComponent* pxUI = xTarget.TryGetComponent<Zenith_UIComponent>();
			if (pxUI == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;	// no canvas (yet) - retry next tick
			}
			Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(m_strButton);
			if (pxElement == nullptr || pxElement->GetType() != Zenith_UI::UIElementType::Button)
			{
				return GRAPH_NODE_STATUS_FAILURE;	// button not created (yet)
			}

			UIButtonClickContext& xClickContext = GetOrCreateClickContext(xTarget.GetEntityID().GetPacked(), m_strButton);
			// Re-wire every tick: survives element recreation and scene
			// reload; deliberately claims the callback slot (documented).
			static_cast<Zenith_UI::Zenith_UIButton*>(pxElement)->SetOnClick(&UIButtonClickTrampoline, &xClickContext);

			// A fresh instance (hot reload, slot re-add, late listener)
			// latches to the persisted count first - clicks from before this
			// instance existed are never replayed as phantom fires.
			if (!m_bLatched)
			{
				m_uSeenClickCount = xClickContext.m_uClickCount;
				m_bLatched = true;
			}

			if (xClickContext.m_uClickCount == m_uSeenClickCount)
			{
				return GRAPH_NODE_STATUS_FAILURE;	// gate closed - no click since last tick
			}
			m_uSeenClickCount = xClickContext.m_uClickCount;
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "OnUIButtonClicked"; }

	private:
		// Per-instance last-seen counter: multiple listeners on one button
		// each see every click edge.
		u_int m_uSeenClickCount = 0;
		bool m_bLatched = false;
	};
}

void Zenith_RegisterEngineGraphNodes_UI()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	xRegistry.RegisterNodeType<Zenith_GraphNode_SetUIText>("SetUIText", GRAPH_EVENT_NONE, 1, false, "UI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetUIColor>("SetUIColor", GRAPH_EVENT_NONE, 1, false, "UI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetUIVisible>("SetUIVisible", GRAPH_EVENT_NONE, 1, false, "UI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetUIFillAmount>("SetUIFillAmount", GRAPH_EVENT_NONE, 1, false, "UI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnUIButtonClicked>("OnUIButtonClicked", GRAPH_EVENT_ON_UPDATE, 1, false, "UI");
}
