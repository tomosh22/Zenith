#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPLiminalHub_Component - the Liminal's spend-Knots-to-unlock controller
 * (2026-07-01, metagame v1; GDD §5.4).
 *
 * Lives on the Liminal scene's GameManager entity next to its
 * Zenith_UIComponent. OnStart wires every authored hermit-node button
 * ("LiminalNode_T<track>_N<node>") to the spend handler and refreshes the
 * visuals; a successful spend deducts Knots, sets the node bit (prefix
 * ordering enforced by DP_MetaSave::TrySpendUnlock), persists to disk, and
 * re-tints the tree. "LiminalBack" returns to the FrontEnd menu.
 *
 * NOTE: this is deliberately plain deterministic C++ in a component, not a
 * Behaviour Graph — a documented divergence from the "logic lives in
 * graphs" doctrine. The hub's logic is a pure state-machine over the
 * persistent meta save (no per-frame gameplay, no designer iteration
 * surface), and the graph node library has no meta-save verbs; revisit if
 * the Liminal grows designer-authored flow.
 *
 * Button callbacks are function pointers with a user-data payload; the
 * payload points into a STATIC node-descriptor table (never into this
 * component, which can relocate with its pool), and the handler reaches
 * the live instance through the s_pxInstance singleton — the same
 * relocation-safe pattern as the other DP singletons.
 */

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "DataStream/Zenith_DataStream.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIText.h"

#include "Source/PublicInterfaces.h"

#include <cstdio>

class DPLiminalHub_Component ZENITH_FINAL
{
public:
	DPLiminalHub_Component() = delete;
	DPLiminalHub_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	// Heap-stability: singleton repoints on pool relocation (standard DP
	// singleton pattern); the static node table means no button user-data
	// ever points at the component itself.
	DPLiminalHub_Component(const DPLiminalHub_Component&) = delete;
	DPLiminalHub_Component& operator=(const DPLiminalHub_Component&) = delete;

	DPLiminalHub_Component(DPLiminalHub_Component&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
	{
		if (s_pxInstance == &xOther) s_pxInstance = this;
	}

	DPLiminalHub_Component& operator=(DPLiminalHub_Component&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity = xOther.m_xParentEntity;
			if (s_pxInstance == &xOther) s_pxInstance = this;
		}
		return *this;
	}

	~DPLiminalHub_Component()
	{
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	void OnAwake()
	{
		Zenith_Assert(s_pxInstance == nullptr,
			"DPLiminalHub_Component singleton double-instantiated");
		s_pxInstance = this;
	}

	void OnDestroy()
	{
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	void OnStart()
	{
		WireButtons();
		RefreshVisuals();
	}

	// Component contract: version-only payload (everything derives from the
	// persistent meta save).
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() {}
#endif

	static DPLiminalHub_Component* Instance() { return s_pxInstance; }

	// The UI click handler's body, exposed as the atomic action twin so
	// tests can spend without simulating a click. Returns true iff the
	// unlock succeeded (affordable + next-in-track); persists on success.
	bool TrySpendNode(DP_MetaSave::HermitTrack eTrack, uint32_t uNode)
	{
		DP_MetaState xState = DP_MetaSave::Cached();
		if (!DP_MetaSave::TrySpendUnlock(xState, eTrack, uNode))
		{
			return false;
		}
		DP_MetaSave::SaveToDisk(xState);
		RefreshVisuals();
		return true;
	}

private:
	// Static node-descriptor table: button user-data payloads point here,
	// never at the (relocatable) component.
	struct NodeRef
	{
		uint8_t m_uTrack;
		uint8_t m_uNode;
	};
	static inline NodeRef s_axNodeRefs[DP_MetaSave::kTRACK_COUNT * DP_MetaSave::kNODES_PER_TRACK] = {};

	static void OnNodeClicked(void* pxUserData)
	{
		const NodeRef* pxRef = static_cast<const NodeRef*>(pxUserData);
		DPLiminalHub_Component* pxHub = Instance();
		if (pxRef == nullptr || pxHub == nullptr) return;
		pxHub->TrySpendNode(static_cast<DP_MetaSave::HermitTrack>(pxRef->m_uTrack), pxRef->m_uNode);
	}

	static void OnBackClicked(void* /*pxUserData*/)
	{
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE); // FrontEnd
	}

	Zenith_UIComponent* GetUI()
	{
		return m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
	}

	void WireButtons()
	{
		Zenith_UIComponent* pxUI = GetUI();
		if (pxUI == nullptr) return;

		for (uint32_t uTrack = 0; uTrack < DP_MetaSave::kTRACK_COUNT; ++uTrack)
		{
			for (uint32_t uNode = 0; uNode < DP_MetaSave::kNODES_PER_TRACK; ++uNode)
			{
				const uint32_t uIdx = uTrack * DP_MetaSave::kNODES_PER_TRACK + uNode;
				s_axNodeRefs[uIdx] = { static_cast<uint8_t>(uTrack), static_cast<uint8_t>(uNode) };
				char szName[40];
				std::snprintf(szName, sizeof(szName), "LiminalNode_T%u_N%u", uTrack, uNode);
				if (Zenith_UI::Zenith_UIButton* pxBtn = pxUI->FindElement<Zenith_UI::Zenith_UIButton>(szName))
				{
					pxBtn->SetOnClick(&OnNodeClicked, &s_axNodeRefs[uIdx]);
				}
			}
		}
		if (Zenith_UI::Zenith_UIButton* pxBack = pxUI->FindElement<Zenith_UI::Zenith_UIButton>("LiminalBack"))
		{
			pxBack->SetOnClick(&OnBackClicked);
		}
	}

	void RefreshVisuals()
	{
		Zenith_UIComponent* pxUI = GetUI();
		if (pxUI == nullptr) return;
		const DP_MetaState& xState = DP_MetaSave::Cached();

		if (Zenith_UI::Zenith_UIText* pxBalance = pxUI->FindElement<Zenith_UI::Zenith_UIText>("LiminalKnotBalance"))
		{
			char szText[64];
			std::snprintf(szText, sizeof(szText), "Knots: %u", xState.m_uKnotBalance);
			pxBalance->SetText(szText);
		}
		if (Zenith_UI::Zenith_UIText* pxLastRun = pxUI->FindElement<Zenith_UI::Zenith_UIText>("LiminalLastRun"))
		{
			char szText[64];
			std::snprintf(szText, sizeof(szText), "Last run: +%u Knots", xState.m_uEarnedUnspentKnotsLastRun);
			pxLastRun->SetText(szText);
		}

		for (uint32_t uTrack = 0; uTrack < DP_MetaSave::kTRACK_COUNT; ++uTrack)
		{
			const DP_MetaSave::HermitTrack eTrack = static_cast<DP_MetaSave::HermitTrack>(uTrack);
			for (uint32_t uNode = 0; uNode < DP_MetaSave::kNODES_PER_TRACK; ++uNode)
			{
				char szName[40];
				std::snprintf(szName, sizeof(szName), "LiminalNode_T%u_N%u", uTrack, uNode);
				Zenith_UI::Zenith_UIButton* pxBtn = pxUI->FindElement<Zenith_UI::Zenith_UIButton>(szName);
				if (pxBtn == nullptr) continue;
				if (DP_MetaSave::IsNodeUnlocked(xState, eTrack, uNode))
				{
					// Unlocked: green.
					pxBtn->SetColor(Zenith_Maths::Vector4(0.30f, 0.62f, 0.32f, 1.0f));
				}
				else if (DP_MetaSave::CanUnlockNode(xState, eTrack, uNode))
				{
					// Purchasable now: warm highlight.
					pxBtn->SetColor(Zenith_Maths::Vector4(0.85f, 0.72f, 0.35f, 1.0f));
				}
				else
				{
					// Locked: grey.
					pxBtn->SetColor(Zenith_Maths::Vector4(0.35f, 0.35f, 0.40f, 1.0f));
				}
			}
		}
	}

	static inline DPLiminalHub_Component* s_pxInstance = nullptr;

	Zenith_Entity m_xParentEntity;
};
