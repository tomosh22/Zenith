#pragma once
/**
 * DPHUDController_Behaviour - in-game HUD. Updates:
 *   - Life bar: 20-tick bar reflecting possessed villager's remaining life,
 *     with colour gradient green→yellow→red as life depletes (urgency cue).
 *   - Held item: shows the held item's tag name, or hidden when empty.
 *   - Objective counter: "Objectives: N/5", reflects DP_Win::GetCollected….
 *   - Status banner: VICTORY / death prompt, listens for DP_OnVictory and
 *     DP_OnVillagerDied. Auto-clears after a short timeout for death so
 *     the player isn't permanently overlaid.
 *
 * The Status / LifeBar / HeldItem / Objectives / PauseOverlay UI elements
 * are authored by the scene editor automation (see DevilsPlayground.cpp's
 * AuthorGameLevelScene + AuthorGymCommon). Missing elements are silently
 * skipped — this lets gym scenes opt out of, e.g., the Objectives counter
 * if they don't care about the win condition.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "UI/Zenith_UIText.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"

#include <cstdio>
#include <cstring>

class DPHUDController_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPHUDController_Behaviour)

	DPHUDController_Behaviour() = delete;
	DPHUDController_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnAwake() ZENITH_FINAL override
	{
		// Subscribe with a SubscribeLambda that captures `this` — we MUST
		// unsubscribe in OnDisable/OnDestroy or the captured `this` will be
		// dangling after script tear-down.
		m_xVictoryHandle = Zenith_EventDispatcher::Get().SubscribeLambda<DP_OnVictory>(
			[this](const DP_OnVictory&)
			{
				m_bRunOver = true;
				SetStatusText("VICTORY",
					Zenith_Maths::Vector4(0.3f, 1.0f, 0.3f, 1.0f),
					/*fHoldSeconds=*/0.0f /* permanent */);
			});
		m_xDeathHandle = Zenith_EventDispatcher::Get().SubscribeLambda<DP_OnVillagerDied>(
			[this](const DP_OnVillagerDied&)
			{
				SetStatusText("Possess another villager",
					Zenith_Maths::Vector4(0.9f, 0.2f, 0.2f, 1.0f),
					/*fHoldSeconds=*/2.5f);
			});
		// MVP-4.2 / 4.3.2: run-lost banner. Each cause has its own
		// copy line that hangs permanently (run is over). The pause-
		// menu R / Q shortcuts still work to restart / quit.
		m_xRunLostHandle = Zenith_EventDispatcher::Get().SubscribeLambda<DP_OnRunLost>(
			[this](const DP_OnRunLost& xEvt)
			{
				m_bRunLostReceived = true;
				m_bRunOver = true;
				m_eLastRunLostCause = xEvt.m_eCause;
				char buf[64];
				BuildRunLostText(buf, sizeof(buf), xEvt.m_eCause);
				SetStatusText(buf,
					Zenith_Maths::Vector4(0.9f, 0.2f, 0.2f, 1.0f),
					/*fHoldSeconds=*/0.0f /* permanent */);
			});
	}

	void OnDisable() ZENITH_FINAL override { TearDown(); }
	void OnDestroy() ZENITH_FINAL override { TearDown(); }

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>()) return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// Status banner timed clear.
		if (m_fStatusHoldRemaining > 0.0f)
		{
			m_fStatusHoldRemaining -= fDt;
			if (m_fStatusHoldRemaining <= 0.0f)
			{
				if (auto* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status"))
				{
					pxStatus->SetVisible(false);
				}
			}
		}

		const Zenith_EntityID xV = DP_Player::GetPossessedVillager();
		const bool bPossessed = xV.IsValid();

		// LifeBar — visible only when possessing.
		if (auto* pxBar = xUI.FindElement<Zenith_UI::Zenith_UIText>("LifeBar"))
		{
			if (!bPossessed)
			{
				pxBar->SetVisible(false);
			}
			else if (DPVillager_Behaviour* pxVB = TryGetVillager(xV))
			{
				const float fT = glm::clamp(pxVB->GetRemainingLife() / pxVB->GetMaxLife(), 0.0f, 1.0f);
				BuildLifeBar(*pxBar, fT);
				pxBar->SetVisible(true);
			}
		}

		// HeldItem — only when possessing AND something is held. Otherwise hide.
		if (auto* pxHeld = xUI.FindElement<Zenith_UI::Zenith_UIText>("HeldItem"))
		{
			const DP_ItemTag eTag = bPossessed ? DP_Player::GetHeldItemTag(xV) : DP_ItemTag::None;
			if (eTag == DP_ItemTag::None)
			{
				pxHeld->SetVisible(false);
			}
			else
			{
				char buf[64];
				std::snprintf(buf, sizeof(buf), "Holding: %s", DP_ItemTagToString(eTag));
				pxHeld->SetText(buf);
				// Match the tag-tint colour scheme from DPItemBase.
				pxHeld->SetColor(TagToColor(eTag));
				pxHeld->SetVisible(true);
			}
		}

		// Objectives — always visible during play, hidden in front-end /
		// gym scenes that don't author the element.
		if (auto* pxObj = xUI.FindElement<Zenith_UI::Zenith_UIText>("Objectives"))
		{
			const uint32_t uMask = DP_Win::GetCollectedObjectivesMask();
			// Count set bits in the low 5 (Objective1..Objective5).
			int iCount = 0;
			for (int i = 0; i < 5; ++i) if (uMask & (1u << i)) ++iCount;
			char buf[32];
			std::snprintf(buf, sizeof(buf), "Objectives: %d/5", iCount);
			pxObj->SetText(buf);
			pxObj->SetVisible(true);
		}

		// MVP-2.5.4: Dawn sun-gauge. Visible while a night is active;
		// hidden in scenes that haven't called DP_Night::StartNight.
		if (auto* pxDawn = xUI.FindElement<Zenith_UI::Zenith_UIText>("DawnGauge"))
		{
			if (!DP_Night::IsNightActive())
			{
				pxDawn->SetVisible(false);
			}
			else
			{
				char buf[32];
				BuildDawnText(buf, sizeof(buf), DP_Night::GetNightTimeRemaining());
				pxDawn->SetText(buf);
				pxDawn->SetVisible(true);
			}
		}

		// MVP-2.5.2: Scent indicator. Reads the possessed villager's
		// scent value. Hidden when nothing possessed.
		if (auto* pxScent = xUI.FindElement<Zenith_UI::Zenith_UIText>("ScentIndicator"))
		{
			if (!bPossessed)
			{
				pxScent->SetVisible(false);
			}
			else
			{
				const float fScent = DP_Player::GetDemonScent(xV);
				char buf[32];
				BuildScentText(buf, sizeof(buf), fScent);
				pxScent->SetText(buf);
				pxScent->SetVisible(true);
			}
		}

		// MVP-2.5.1 + 2.5.3: Aelfric awareness state. Compute once;
		// feed both the whisper line + the awareness icon.
		const AelfricState eState = ComputeAelfricState();

		if (auto* pxWhisper = xUI.FindElement<Zenith_UI::Zenith_UIText>("WhisperLine"))
		{
			char buf[64];
			BuildWhisperText(buf, sizeof(buf), eState);
			pxWhisper->SetText(buf);
			pxWhisper->SetVisible(true);
		}

		if (auto* pxAwareness = xUI.FindElement<Zenith_UI::Zenith_UIText>("AelfricAwareness"))
		{
			char buf[48];
			BuildAwarenessText(buf, sizeof(buf), eState);
			pxAwareness->SetText(buf);
			pxAwareness->SetVisible(true);
		}

		// MVP-4.3.2: post-victory / post-run-lost "press any key to
		// restart" overlay. Set m_bRunOver true when either Victory or
		// RunLost dispatches; show the RestartPrompt UI element with the
		// canonical R/Q hint. Element is authored hidden by default; only
		// becomes visible once the run is actually over.
		if (auto* pxPrompt = xUI.FindElement<Zenith_UI::Zenith_UIText>("RestartPrompt"))
		{
			if (m_bRunOver)
			{
				pxPrompt->SetText("Press R to restart, Q to quit");
				pxPrompt->SetVisible(true);
			}
			else
			{
				pxPrompt->SetVisible(false);
			}
		}
	}

public:
	// MVP-2.5.4 + 2.5.2: pure text-formatting helpers exposed for
	// unit-level tests (Test_P2HUD_*). The real OnUpdate calls them
	// internally; this lets tests verify the formatting WITHOUT
	// authoring a full UI subsystem in their scene.
	static void BuildDawnText(char* szBuf, size_t uBufSize, float fSecondsRemaining)
	{
		if (fSecondsRemaining < 0.0f) fSecondsRemaining = 0.0f;
		std::snprintf(szBuf, uBufSize, "Dawn: %.1f s", fSecondsRemaining);
	}
	static void BuildScentText(char* szBuf, size_t uBufSize, float fScent)
	{
		if (fScent < 0.0f) fScent = 0.0f;
		std::snprintf(szBuf, uBufSize, "Scent: %.2f", fScent);
	}

	// MVP-2.5.1 + 2.5.3: Aelfric awareness state. Derived from the
	// priest's blackboard each frame:
	//   Pursuing   -- BB_KEY_TARGET_WITH_DEVIL is a valid villager
	//                 (priest sees the demon and is heading for them).
	//   Suspicious -- BB_KEY_HAS_INVESTIGATE_POS is true (priest
	//                 heard / saw something and is investigating)
	//                 but no confirmed target.
	//   Calm       -- neither flag set (default patrol).
	enum class AelfricState : uint8_t
	{
		Calm = 0,
		Suspicious,
		Pursuing
	};

	// Awareness-icon text: the state name in caps. Placeholder per
	// the roadmap (MVP-2.5.3) until a real icon asset lands.
	static void BuildAwarenessText(char* szBuf, size_t uBufSize, AelfricState eState)
	{
		const char* szLabel = "CALM";
		if      (eState == AelfricState::Pursuing)   szLabel = "PURSUING";
		else if (eState == AelfricState::Suspicious) szLabel = "SUSPICIOUS";
		std::snprintf(szBuf, uBufSize, "Aelfric: %s", szLabel);
	}

	// Whisper-line text: vibe copy that reacts to the priest's state.
	// One line per state -- post-MVP polish would rotate through
	// variants. Per MVP-2.5.1, the whisper line is a single-line
	// bottom-centre text element.
	static void BuildWhisperText(char* szBuf, size_t uBufSize, AelfricState eState)
	{
		const char* szLine = "He patrols.";
		if      (eState == AelfricState::Pursuing)   szLine = "He sees you!";
		else if (eState == AelfricState::Suspicious) szLine = "He stirs...";
		std::snprintf(szBuf, uBufSize, "%s", szLine);
	}

	// MVP-4.2 / 4.3.2: run-lost banner copy. One line per cause from
	// DP_RunLostCause. Permanent display (run is over); the pause
	// menu's R / Q shortcuts continue to work for restart / quit.
	static void BuildRunLostText(char* szBuf, size_t uBufSize, DP_RunLostCause eCause)
	{
		const char* szLine = "RUN LOST";
		switch (eCause)
		{
		case DP_RunLostCause::Apprehended: szLine = "CAUGHT BY AELFRIC"; break;
		case DP_RunLostCause::Dawn:        szLine = "DAWN BREAKS"; break;
		case DP_RunLostCause::NoVessels:   szLine = "NO VESSELS REMAIN"; break;
		}
		std::snprintf(szBuf, uBufSize, "%s", szLine);
	}

#ifdef ZENITH_INPUT_SIMULATOR
	// MVP-4.2 test accessors: did the run-lost subscriber fire? Used
	// by Phase-4 loss playthrough tests to verify the HUD reacted.
	bool DidRunLostHandlerFireForTest() const { return m_bRunLostReceived; }
	DP_RunLostCause LastRunLostCauseForTest() const { return m_eLastRunLostCause; }
	// MVP-4.3.2 test accessor: did either Victory or RunLost handler set
	// the run-over flag? Used by Phase-4 restart-prompt tests.
	bool IsRunOverForTest() const { return m_bRunOver; }
	void ResetRunLostForTest()
	{
		m_bRunLostReceived = false;
		m_bRunOver = false;
		m_eLastRunLostCause = DP_RunLostCause::Apprehended;
	}
#endif

	// Compute the current Aelfric state. Iterates Priest_Behaviour
	// scripts in the active scene (typically just 1 priest), reads
	// the agent's blackboard, classifies.
	static AelfricState ComputeAelfricState()
	{
		AelfricState eOut = AelfricState::Calm;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&eOut](Zenith_EntityID xPriestId, Priest_Behaviour&)
			{
				Zenith_SceneData* pxScene =
					Zenith_SceneManager::GetSceneDataForEntity(xPriestId);
				if (pxScene == nullptr) return;
				Zenith_Entity xEnt = pxScene->TryGetEntity(xPriestId);
				if (!xEnt.IsValid()) return;
				if (!xEnt.HasComponent<Zenith_AIAgentComponent>()) return;
				Zenith_AIAgentComponent& xAg =
					xEnt.GetComponent<Zenith_AIAgentComponent>();
				Zenith_Blackboard& xBB = xAg.GetBlackboard();
				const Zenith_EntityID xTarget =
					xBB.GetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
				if (xTarget.IsValid())
				{
					eOut = AelfricState::Pursuing;
					return;
				}
				const bool bHasInvestigate =
					xBB.GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS);
				if (bHasInvestigate)
				{
					if (eOut < AelfricState::Suspicious)
					{
						eOut = AelfricState::Suspicious;
					}
				}
			});
		return eOut;
	}

private:
	// Gradient: green → yellow → red as life depletes. The interpolation
	// hands off at 0.5 to keep yellow visible for ~half the bar.
	static Zenith_Maths::Vector4 LifeColor(float fT)
	{
		if (fT >= 0.5f)
		{
			const float fU = (fT - 0.5f) * 2.0f;     // 0..1 over upper half
			return Zenith_Maths::Vector4(
				glm::mix(1.0f, 0.3f, fU),  // R: yellow→green R
				glm::mix(1.0f, 1.0f, fU),  // G: stays high
				glm::mix(0.3f, 0.3f, fU),  // B: low
				1.0f);
		}
		const float fU = fT * 2.0f;                   // 0..1 over lower half
		return Zenith_Maths::Vector4(
			glm::mix(1.0f, 1.0f, fU),  // R: red→yellow R
			glm::mix(0.2f, 1.0f, fU),  // G: red G→yellow G
			glm::mix(0.2f, 0.3f, fU),  // B: low
			1.0f);
	}

	// Mirror DPItemBase tag→color so the HUD readout matches the world tint.
	static Zenith_Maths::Vector4 TagToColor(DP_ItemTag eTag)
	{
		switch (eTag)
		{
			case DP_ItemTag::Iron:        return Zenith_Maths::Vector4(0.6f, 0.6f, 0.7f, 1.0f);
			case DP_ItemTag::Key:         return Zenith_Maths::Vector4(1.0f, 0.85f, 0.2f, 1.0f);
			case DP_ItemTag::SkeletonKey: return Zenith_Maths::Vector4(0.7f, 0.3f, 0.9f, 1.0f);
			case DP_ItemTag::Objective1:
			case DP_ItemTag::Objective2:
			case DP_ItemTag::Objective3:
			case DP_ItemTag::Objective4:
			case DP_ItemTag::Objective5:  return Zenith_Maths::Vector4(0.95f, 0.15f, 0.15f, 1.0f);
			default:                      return Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	void BuildLifeBar(Zenith_UI::Zenith_UIText& xBar, float fT)
	{
		const int iBars = static_cast<int>(fT * 20.0f + 0.5f);
		char buf[64];
		std::snprintf(buf, sizeof(buf), "Life: ");
		size_t off = std::strlen(buf);
		for (int i = 0; i < 20 && off + 1 < sizeof(buf); ++i)
		{
			buf[off++] = (i < iBars) ? '|' : '.';
		}
		buf[off] = '\0';
		xBar.SetText(buf);
		xBar.SetColor(LifeColor(fT));
	}

	DPVillager_Behaviour* TryGetVillager(Zenith_EntityID xV)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xV);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xV);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
	}

	void SetStatusText(const char* szText, const Zenith_Maths::Vector4& xColor, float fHoldSeconds)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>()) return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		if (auto* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status"))
		{
			pxStatus->SetText(szText);
			pxStatus->SetColor(xColor);
			pxStatus->SetVisible(true);
		}
		// Negative or zero hold means "permanent" — Victory uses this.
		m_fStatusHoldRemaining = (fHoldSeconds > 0.0f) ? fHoldSeconds : -1.0f;
	}

	void TearDown()
	{
		if (m_xVictoryHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(m_xVictoryHandle);
			m_xVictoryHandle = INVALID_EVENT_HANDLE;
		}
		if (m_xDeathHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(m_xDeathHandle);
			m_xDeathHandle = INVALID_EVENT_HANDLE;
		}
		if (m_xRunLostHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(m_xRunLostHandle);
			m_xRunLostHandle = INVALID_EVENT_HANDLE;
		}
	}

	Zenith_EventHandle m_xVictoryHandle       = INVALID_EVENT_HANDLE;
	Zenith_EventHandle m_xDeathHandle         = INVALID_EVENT_HANDLE;
	Zenith_EventHandle m_xRunLostHandle       = INVALID_EVENT_HANDLE;
	float              m_fStatusHoldRemaining = -1.0f;  // <0 = permanent / not set
	bool               m_bRunLostReceived     = false;
	// MVP-4.3.2: set by EITHER the Victory or RunLost subscriber. Drives
	// the RestartPrompt UI element + gates the pause-menu R/Q shortcuts
	// when the player isn't paused but the run is over (e.g., they
	// watched "VICTORY" appear and just want to start a new run).
	bool               m_bRunOver             = false;
	DP_RunLostCause    m_eLastRunLostCause    = DP_RunLostCause::Apprehended;
};
