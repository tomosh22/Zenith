#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPHUDController_Component - in-game HUD. Updates:
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
 * AuthorDPGameSceneFrame). Missing elements are silently skipped — this
 * lets gym scenes opt out of, e.g., the Objectives counter if they don't
 * care about the win condition.
 *
 * Heap-stability: the event subscriptions capture `this`, and component
 * pools relocate on resize / swap-and-pop / cross-scene transfer. The
 * hand-written move operations unsubscribe the old address and re-subscribe
 * the new one (SubscribeRunEvents is the single factored subscription site).
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "Input/Zenith_Input.h"
#include "Source/DPInputActions.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DPTutorial.h"

#include <cmath>
#include <cstdio>
#include <cstring>

class DPHUDController_Component ZENITH_FINAL
{
public:
	DPHUDController_Component() = delete;
	DPHUDController_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	// Heap-stability: the subscriptions capture `this` — hand-written moves
	// re-point them at the relocated instance; copies deleted.
	DPHUDController_Component(const DPHUDController_Component&) = delete;
	DPHUDController_Component& operator=(const DPHUDController_Component&) = delete;

	DPHUDController_Component(DPHUDController_Component&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_fStatusHoldRemaining(xOther.m_fStatusHoldRemaining)
		, m_bRunLostReceived(xOther.m_bRunLostReceived)
		, m_bRunOver(xOther.m_bRunOver)
		, m_eLastRunLostCause(xOther.m_eLastRunLostCause)
		, m_fLockedDoorAlertRemaining(xOther.m_fLockedDoorAlertRemaining)
		, m_eLastLockedDoorRequiredKey(xOther.m_eLastLockedDoorRequiredKey)
		, m_fAwarenessFlashRemaining(xOther.m_fAwarenessFlashRemaining)
		, m_eLastAlertKind(xOther.m_eLastAlertKind)
		, m_bTimerStarted(xOther.m_bTimerStarted)
		, m_fRunTimerSeconds(xOther.m_fRunTimerSeconds)
		, m_bHelpVisible(xOther.m_bHelpVisible)
	{
		TakeOverSubscriptionsFrom(xOther);
	}

	DPHUDController_Component& operator=(DPHUDController_Component&& xOther) noexcept
	{
		if (this != &xOther)
		{
			TearDown();
			m_xParentEntity              = xOther.m_xParentEntity;
			m_fStatusHoldRemaining       = xOther.m_fStatusHoldRemaining;
			m_bRunLostReceived           = xOther.m_bRunLostReceived;
			m_bRunOver                   = xOther.m_bRunOver;
			m_eLastRunLostCause          = xOther.m_eLastRunLostCause;
			m_fLockedDoorAlertRemaining  = xOther.m_fLockedDoorAlertRemaining;
			m_eLastLockedDoorRequiredKey = xOther.m_eLastLockedDoorRequiredKey;
			m_fAwarenessFlashRemaining   = xOther.m_fAwarenessFlashRemaining;
			m_eLastAlertKind             = xOther.m_eLastAlertKind;
			m_bTimerStarted              = xOther.m_bTimerStarted;
			m_fRunTimerSeconds           = xOther.m_fRunTimerSeconds;
			m_bHelpVisible               = xOther.m_bHelpVisible;
			TakeOverSubscriptionsFrom(xOther);
		}
		return *this;
	}

	~DPHUDController_Component()
	{
		// Belt-and-braces: OnDisable/OnDestroy already unsubscribe; this
		// covers teardown paths that skip the lifecycle hooks. Moved-from
		// sources hold INVALID handles so this is a no-op for them.
		TearDown();
	}

	void OnAwake()
	{
		// Subscribe with lambdas that capture `this` — we MUST unsubscribe in
		// OnDisable/OnDestroy or the captured `this` will be dangling after
		// component tear-down (and the move operations must re-subscribe).
		SubscribeRunEvents();
	}

	void OnDisable() { TearDown(); }
	void OnDestroy() { TearDown(); }

	// Component contract: version-only payload (HUD state is per-run).
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

	void OnUpdate(const float fDt)
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr) return;
		Zenith_UIComponent& xUI = *pxUI;

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
			else
			{
				// A resolved villager always has max life > 0 (archetype /
				// default seed); use that as the "villager resolved" gate so
				// the bar is only built+shown when the forwarders resolve the
				// villager -- matching the previous TryGetVillager non-null
				// branch. Avoids a divide-by-zero too.
				const float fMax = DP_Player::GetVillagerMaxLife(xV);
				if (fMax > 0.0f)
				{
					const float fT = glm::clamp(
						DP_Player::GetVillagerRemainingLife(xV) / fMax, 0.0f, 1.0f);
					BuildLifeBar(*pxBar, fT);
					pxBar->SetVisible(true);
				}
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
		// 2026-05-21: paired ScentBar element shows the 10-segment
		// ASCII bar with hound-bark threshold marker. Both elements
		// share the colour-coded gradient so peripheral vision picks
		// up rising scent without reading the number.
		const float fScent = bPossessed ? DP_Player::GetDemonScent(xV) : 0.0f;
		const float fScentThreshold =
			DP_Tuning::Get<float>("possession.demon_scent_hound_bark_threshold");
		const Zenith_Maths::Vector4 xScentColor = ScentBarColor(fScent, fScentThreshold);

		if (auto* pxScent = xUI.FindElement<Zenith_UI::Zenith_UIText>("ScentIndicator"))
		{
			if (!bPossessed)
			{
				pxScent->SetVisible(false);
			}
			else
			{
				char buf[32];
				BuildScentText(buf, sizeof(buf), fScent);
				pxScent->SetText(buf);
				pxScent->SetColor(xScentColor);
				pxScent->SetVisible(true);
			}
		}
		if (auto* pxScentBar = xUI.FindElement<Zenith_UI::Zenith_UIText>("ScentBar"))
		{
			if (!bPossessed)
			{
				pxScentBar->SetVisible(false);
			}
			else
			{
				char buf[40];
				BuildScentBar(buf, sizeof(buf), fScent, fScentThreshold);
				pxScentBar->SetText(buf);
				pxScentBar->SetColor(xScentColor);
				pxScentBar->SetVisible(true);
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

		// 2026-05-21: tick the awareness flash timer (set by
		// DP_OnPriestAlerted subscriber). Range 0..1 maps to
		// AwarenessColor's flash mix.
		if (m_fAwarenessFlashRemaining > 0.0f)
		{
			m_fAwarenessFlashRemaining = glm::max(0.0f, m_fAwarenessFlashRemaining - fDt);
		}

		if (auto* pxAwareness = xUI.FindElement<Zenith_UI::Zenith_UIText>("AelfricAwareness"))
		{
			char buf[64];
			BuildAwarenessText(buf, sizeof(buf), eState);
			pxAwareness->SetText(buf);
			pxAwareness->SetColor(AwarenessColor(eState, m_fAwarenessFlashRemaining));
			pxAwareness->SetVisible(true);
		}

		// 2026-05-21: locked-door alert. Counts down from 2 s on
		// DP_OnDoorLockRejected (set by the subscribe lambda).
		// Visibility is gated on the timer being positive; the text
		// is rebuilt each frame from m_eLastLockedDoorRequiredKey so
		// post-MVP keys (SkeletonKey) telegraph correctly.
		if (m_fLockedDoorAlertRemaining > 0.0f)
		{
			m_fLockedDoorAlertRemaining = glm::max(0.0f, m_fLockedDoorAlertRemaining - fDt);
		}
		if (auto* pxLocked = xUI.FindElement<Zenith_UI::Zenith_UIText>("LockedDoorAlert"))
		{
			if (m_fLockedDoorAlertRemaining > 0.0f)
			{
				char buf[64];
				BuildLockedDoorAlertText(buf, sizeof(buf), m_eLastLockedDoorRequiredKey);
				pxLocked->SetText(buf);
				pxLocked->SetVisible(true);
			}
			else
			{
				pxLocked->SetVisible(false);
			}
		}

		// 2026-05-21: archetype status. Reads the possessed villager's
		// archetype id and shows the special-rule line below VillagerInfo.
		// Hidden when not possessing, or possessing a Farmhand (baseline).
		// Local resolve of the villager because the broader detailed-HUD
		// block (which has its own pxVB) sits below this.
		if (auto* pxArch = xUI.FindElement<Zenith_UI::Zenith_UIText>("ArchetypeStatus"))
		{
			const char* szLine = nullptr;
			// Forwarder returns "" when the villager can't be resolved OR the
			// archetype id is empty -- both map to the previous "skip" branch.
			const char* szArchetype = bPossessed ? DP_Player::GetVillagerArchetypeId(xV) : "";
			if (szArchetype[0] != '\0')
			{
				szLine = BuildArchetypeStatusText(szArchetype);
			}
			if (szLine != nullptr)
			{
				pxArch->SetText(szLine);
				pxArch->SetColor(ArchetypeStatusColor(szArchetype));
				pxArch->SetVisible(true);
			}
			else
			{
				pxArch->SetVisible(false);
			}
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

		// ---------------------------------------------------------------
		// Detailed HUD readouts (user feedback 2026-05-15: HUD needs more
		// detail). Eight new elements complement the primary readouts:
		//   VillagerInfo  -- archetype name of the possessed villager
		//   LifeNumeric   -- "Life: 23.4 / 30.0 s" alongside the ASCII bar
		//   MovementMode  -- "SPRINT" / "WALK QUIET" / "MOVE"
		//   VillagersAlive -- countdown of live villagers (toward NoVessels)
		//   PriestDistance -- meters to the closest priest
		//   RunTimer      -- mm:ss since first possession
		//   InteractHint  -- "F to interact" when in range of an interactable
		//   ReagentHelp   -- one-line description of held special item
		// Each gated to its visibility rule -- nothing rendered when irrelevant.
		// ---------------------------------------------------------------

		// Tick run timer once we've possessed for the first time.
		if (bPossessed && !m_bTimerStarted)
		{
			m_bTimerStarted = true;
			m_fRunTimerSeconds = 0.0f;
		}
		if (m_bTimerStarted && !m_bRunOver)
		{
			m_fRunTimerSeconds += fDt;
		}

		// VillagerInfo + LifeNumeric + MovementMode -- show possessed
		// villager's archetype, life-seconds, and movement mode. All gated
		// on the villager resolving. The forwarders replace the previous
		// TryGetVillager helper; a resolved villager always has max life > 0
		// (archetype / default seed), so that doubles as the "resolved" gate
		// (matching the previous `pxVB != nullptr` checks). The archetype
		// forwarder returns "" when unresolved, so VillagerInfo's empty-id
		// guard is preserved.
		const float fVMax = bPossessed ? DP_Player::GetVillagerMaxLife(xV) : 0.0f;
		const bool bVillagerResolved = (fVMax > 0.0f);
		if (auto* pxInfo = xUI.FindElement<Zenith_UI::Zenith_UIText>("VillagerInfo"))
		{
			const char* szInfoArchetype =
				bVillagerResolved ? DP_Player::GetVillagerArchetypeId(xV) : "";
			if (szInfoArchetype[0] != '\0')
			{
				char buf[80];
				BuildArchetypeText(buf, sizeof(buf), szInfoArchetype);
				pxInfo->SetText(buf);
				pxInfo->SetVisible(true);
			}
			else
			{
				pxInfo->SetVisible(false);
			}
		}
		if (auto* pxLifeNum = xUI.FindElement<Zenith_UI::Zenith_UIText>("LifeNumeric"))
		{
			if (bVillagerResolved)
			{
				char buf[48];
				BuildLifeNumericText(buf, sizeof(buf),
					DP_Player::GetVillagerRemainingLife(xV), fVMax);
				pxLifeNum->SetText(buf);
				pxLifeNum->SetVisible(true);
			}
			else
			{
				pxLifeNum->SetVisible(false);
			}
		}

		// MovementMode -- Sprint / Walk-Quiet / Move. Reads the possessed
		// villager's per-frame cache; format via BuildMovementModeText.
		if (auto* pxMove = xUI.FindElement<Zenith_UI::Zenith_UIText>("MovementMode"))
		{
			if (bVillagerResolved)
			{
				char buf[40];
				BuildMovementModeText(buf, sizeof(buf),
					DP_Player::IsVillagerSprintingNow(xV),
					DP_Player::IsVillagerWalkQuietNow(xV));
				pxMove->SetText(buf);
				pxMove->SetVisible(true);
			}
			else
			{
				pxMove->SetVisible(false);
			}
		}

		// VillagersAlive -- count live villagers (RemainingLife > 0). Shows
		// "N / total" so the player tracks attrition toward NoVessels.
		if (auto* pxCount = xUI.FindElement<Zenith_UI::Zenith_UIText>("VillagersAlive"))
		{
			int iAlive = 0;
			int iTotal = 0;
			DP_Player::CountVillagers(iAlive, iTotal);
			if (iTotal > 0)
			{
				char buf[40];
				BuildVillagersAliveText(buf, sizeof(buf), iAlive, iTotal);
				pxCount->SetText(buf);
				pxCount->SetVisible(true);
			}
			else
			{
				pxCount->SetVisible(false);
			}
		}

		// PriestDistance -- meters to closest priest from possessed villager.
		// Hidden when not possessing. Threshold-based colour coding lives in
		// PriestDangerForDistance + PriestDistanceColor so tests can pin the
		// breakpoints without authoring a priest entity.
		if (auto* pxDist = xUI.FindElement<Zenith_UI::Zenith_UIText>("PriestDistance"))
		{
			if (bPossessed)
			{
				Zenith_Maths::Vector3 xMyPos(0.0f);
				bool bHaveMyPos = TryGetEntityPos(xV, xMyPos);
				float fClosestDist = -1.0f;
				if (bHaveMyPos)
				{
					// Forwarder does the same priest scan (nearest XZ distance,
					// -1 if no priest) so the HUD header no longer includes
					// the priest component header.
					fClosestDist = DP_AI::GetNearestPriestDistanceFrom(xMyPos);
				}
				if (fClosestDist >= 0.0f)
				{
					char buf[40];
					BuildPriestDistanceText(buf, sizeof(buf), fClosestDist);
					pxDist->SetColor(PriestDistanceColor(PriestDangerForDistance(fClosestDist)));
					pxDist->SetText(buf);
					pxDist->SetVisible(true);
				}
				else
				{
					pxDist->SetVisible(false);
				}
			}
			else
			{
				pxDist->SetVisible(false);
			}
		}

		// RunTimer -- mm:ss elapsed since first possession.
		if (auto* pxTimer = xUI.FindElement<Zenith_UI::Zenith_UIText>("RunTimer"))
		{
			if (m_bTimerStarted)
			{
				char buf[32];
				BuildRunTimerText(buf, sizeof(buf), m_fRunTimerSeconds);
				pxTimer->SetText(buf);
				pxTimer->SetVisible(true);
			}
			else
			{
				pxTimer->SetVisible(false);
			}
		}

		// InteractHint -- show "F to interact with <type>" when possessing
		// AND within range of an interactable. Iterates each interactable
		// subclass since IsPlayerInRange is defined on the base.
		if (auto* pxHint = xUI.FindElement<Zenith_UI::Zenith_UIText>("InteractHint"))
		{
			const char* szNearestType = bPossessed ? DP_Interactables::FindNearestInteractableType(xV) : nullptr;
			if (szNearestType != nullptr)
			{
				char buf[64];
				BuildInteractHintText(buf, sizeof(buf), szNearestType);
				pxHint->SetText(buf);
				pxHint->SetVisible(true);
			}
			else
			{
				pxHint->SetVisible(false);
			}
		}

		// ReagentHelp -- one-line description of held item if it's a special
		// reagent. Helps the player remember each reagent's quirk.
		if (auto* pxReagent = xUI.FindElement<Zenith_UI::Zenith_UIText>("ReagentHelp"))
		{
			const DP_ItemTag eTag = bPossessed ? DP_Player::GetHeldItemTag(xV) : DP_ItemTag::None;
			const char* szHelp = ReagentHelpText(eTag);
			if (szHelp != nullptr)
			{
				pxReagent->SetText(szHelp);
				pxReagent->SetVisible(true);
			}
			else
			{
				pxReagent->SetVisible(false);
			}
		}

		// ---------------------------------------------------------------
		// Instructional HUD (user feedback 2026-05-16: HUD should teach
		// the entire game). Three layers:
		//   ControlsHint -- static multi-line hotkey cheat sheet
		//                   (authored once, never touched here).
		//   TutorialHint -- single-line context-sensitive guidance
		//                   ("Click a villager to possess them" /
		//                   "Carry to the pentagram" / etc.)
		//   HelpOverlay  -- full-screen modal toggled with [H], lists
		//                   every mechanic + every hotkey.
		// ---------------------------------------------------------------

		// Toggle full-screen help overlay on [H]. The HelpBg rect and
		// HelpOverlay/HelpTitle text elements are authored hidden by
		// default; here we follow m_bHelpVisible.
		if (DP_Input::ReadHelpTogglePressed())
		{
			m_bHelpVisible = !m_bHelpVisible;
		}
		if (auto* pxHelpBg = xUI.FindElement<Zenith_UI::Zenith_UIRect>("HelpBg"))
		{
			pxHelpBg->SetVisible(m_bHelpVisible);
		}
		if (auto* pxHelpTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("HelpTitle"))
		{
			pxHelpTitle->SetVisible(m_bHelpVisible);
		}
		if (auto* pxHelp = xUI.FindElement<Zenith_UI::Zenith_UIText>("HelpOverlay"))
		{
			pxHelp->SetVisible(m_bHelpVisible);
		}

		// TutorialHint: pick the single most useful instruction for the
		// current game state. Hidden when no hint is relevant.
		if (auto* pxTut = xUI.FindElement<Zenith_UI::Zenith_UIText>("TutorialHint"))
		{
			const char* szHint = BuildTutorialHint(bPossessed, xV, eState, m_bRunOver);
			if (szHint != nullptr)
			{
				pxTut->SetText(szHint);
				pxTut->SetVisible(true);
			}
			else
			{
				pxTut->SetVisible(false);
			}
		}

		// 2026-05-21: TutorialOverlay -- first-encounter tips driven by
		// DP_Tutorial. The tutorialisation system subscribes to DP
		// events and surfaces a one-shot text + auto-clearing 5 s
		// timer. We tick the timer here (every frame, paused or not --
		// the tip text deserves the player's reading time even
		// mid-pause), then read the active tip + visibility.
		DP_Tutorial::Tick(fDt);
		if (auto* pxOverlay = xUI.FindElement<Zenith_UI::Zenith_UIText>("TutorialOverlay"))
		{
			const char* szTip = DP_Tutorial::GetActiveTipText();
			if (szTip != nullptr)
			{
				pxOverlay->SetText(szTip);
				// Optional fade in the last 0.5 s of the tip's
				// lifetime so it doesn't pop-clear. Alpha lerps from
				// 1.0 to 0.4 across that window.
				const float fRem = DP_Tutorial::GetActiveTipRemaining();
				const float fAlpha = (fRem < 0.5f) ? 0.4f + 1.2f * fRem : 1.0f;
				pxOverlay->SetColor(Zenith_Maths::Vector4(1.0f, 0.95f, 0.75f, fAlpha));
				pxOverlay->SetVisible(true);
			}
			else
			{
				pxOverlay->SetVisible(false);
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

	// 2026-05-21: ScentBar -- 10-cell ASCII bar visualisation of the
	// possessed villager's scent saturation (0..1 mapped to 0..10
	// segments). Filled cells use '#', empty use '.', and the
	// hound-bark threshold (0.5 by Tuning.json default) is marked with
	// a '|' separator so the player can SEE when scent crosses it.
	// Format example at fScent=0.42, fThreshold=0.5:
	//   "Scent [####|.....]"
	// At fScent=0.65:
	//   "Scent [#####|##..]"
	// At fScent=1.0:
	//   "Scent [#####|#####]" (saturated)
	static void BuildScentBar(char* szBuf, size_t uBufSize, float fScent, float fThreshold)
	{
		if (fScent < 0.0f)     fScent = 0.0f;
		if (fScent > 1.0f)     fScent = 1.0f;
		if (fThreshold < 0.0f) fThreshold = 0.0f;
		if (fThreshold > 1.0f) fThreshold = 1.0f;
		const int kBarLen = 10;
		const int iThresholdCell = static_cast<int>(fThreshold * static_cast<float>(kBarLen) + 0.5f);
		const int iFilledCells   = static_cast<int>(fScent     * static_cast<float>(kBarLen) + 0.5f);

		char xBar[24] = { 0 };
		int iWrite = 0;
		xBar[iWrite++] = '[';
		for (int i = 0; i < kBarLen; ++i)
		{
			// Insert the threshold separator just BEFORE the iThresholdCell
			// (so the bar reads "filled segments | empty segments" with
			// the bar showing scent vs threshold visually).
			if (i == iThresholdCell && iThresholdCell > 0 && iThresholdCell < kBarLen)
			{
				xBar[iWrite++] = '|';
			}
			xBar[iWrite++] = (i < iFilledCells) ? '#' : '.';
		}
		xBar[iWrite++] = ']';
		xBar[iWrite] = '\0';
		std::snprintf(szBuf, uBufSize, "Scent %s", xBar);
	}

	// 2026-05-21: scent-bar colour. Grey/violet when below threshold,
	// bright red once we cross the hound-bark line. Lets the player's
	// peripheral vision catch the danger transition without parsing
	// the numeric "0.51 -> 0.48".
	static Zenith_Maths::Vector4 ScentBarColor(float fScent, float fThreshold)
	{
		if (fScent < fThreshold)
		{
			// Gradient from neutral grey to deep violet as scent climbs.
			const float fT = (fThreshold > 0.0001f) ? (fScent / fThreshold) : 0.0f;
			return Zenith_Maths::Vector4(
				0.50f + 0.20f * fT,
				0.30f + 0.05f * fT,
				0.70f + 0.20f * fT,
				1.0f);
		}
		// At/above threshold: alarm red.
		return Zenith_Maths::Vector4(1.0f, 0.25f, 0.20f, 1.0f);
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

	// Awareness-icon text: state-named with an ASCII glyph that
	// telegraphs the urgency at a glance. Pre-fix this was just
	// "Aelfric: <state>" -- legible only if you parsed the word.
	// 2026-05-21: glyph + parens prefix so the icon line reads
	// distinctly from the surrounding HUD text.
	//   Calm        -> "[ ~ ] Aelfric: Patrolling"  (idle wave)
	//   Suspicious  -> "[ ? ] Aelfric: SUSPICIOUS"  (question mark)
	//   Pursuing    -> "[ ! ] Aelfric: PURSUING"    (alarm)
	static void BuildAwarenessText(char* szBuf, size_t uBufSize, AelfricState eState)
	{
		const char* szGlyph = "~";
		const char* szLabel = "Patrolling";
		if (eState == AelfricState::Pursuing)
		{
			szGlyph = "!";
			szLabel = "PURSUING";
		}
		else if (eState == AelfricState::Suspicious)
		{
			szGlyph = "?";
			szLabel = "SUSPICIOUS";
		}
		std::snprintf(szBuf, uBufSize, "[ %s ] Aelfric: %s", szGlyph, szLabel);
	}

	// 2026-05-21: awareness-icon colour. Threshold maps state to a
	// risk-band gradient so the player's peripheral vision can read
	// the priest's danger level without parsing the glyph.
	//   Calm        -> dim green (safe-but-watch)
	//   Suspicious  -> amber (rising)
	//   Pursuing    -> alarm red (immediate)
	// A short "flash" boost (via fFlashT in [0..1]) brightens the
	// colour for ~1 s after a rising-edge DP_OnPriestAlerted -- helps
	// the player notice the transition.
	static Zenith_Maths::Vector4 AwarenessColor(AelfricState eState, float fFlashT)
	{
		if (fFlashT < 0.0f) fFlashT = 0.0f;
		if (fFlashT > 1.0f) fFlashT = 1.0f;
		Zenith_Maths::Vector4 xBase(0.45f, 0.85f, 0.45f, 1.0f);  // calm green
		if (eState == AelfricState::Suspicious)
		{
			xBase = Zenith_Maths::Vector4(0.95f, 0.75f, 0.30f, 1.0f);
		}
		else if (eState == AelfricState::Pursuing)
		{
			xBase = Zenith_Maths::Vector4(1.0f, 0.30f, 0.20f, 1.0f);
		}
		// Flash mix: lerp toward bright white over 0..0.4, back over 0.4..1.
		const float fMix = (fFlashT > 0.4f)
			? (1.0f - fFlashT) / 0.6f
			: fFlashT / 0.4f;
		const float fK = glm::clamp(fMix, 0.0f, 1.0f) * 0.5f;
		return Zenith_Maths::Vector4(
			xBase.x + (1.0f - xBase.x) * fK,
			xBase.y + (1.0f - xBase.y) * fK,
			xBase.z + (1.0f - xBase.z) * fK,
			1.0f);
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

	// 2026-05-16 instructional HUD: pick the single most useful
	// instruction for the current game state. Priority order:
	//   run-over        -> hide (RestartPrompt already covers this)
	//   not-possessed   -> "Click a villager to possess them"
	//   priest-pursuing -> "Aelfric sees you -- break LOS"
	//   holding-objective -> "Carry to the pentagram"
	//   holding-iron      -> "Carry to a forge for a key"
	//   holding-skeletonkey -> "[F] on any locked door"
	//   holding-reagent   -> "See the reagent line"
	//   priest-suspicious -> "He's suspicious -- walk quiet"
	//   default           -> "Find an objective"
	// Returns nullptr when no hint applies (HUD line hidden).
	//
	// Two forms:
	//   BuildTutorialHintForState -- pure dispatch table; takes the
	//     held-item tag directly. Unit-testable in isolation.
	//   BuildTutorialHint -- thin wrapper for OnUpdate; resolves the
	//     tag via DP_Player::GetHeldItemTag (which needs a real entity
	//     + scene). Tests cover the pure form.
	static const char* BuildTutorialHintForState(bool bPossessed, DP_ItemTag eTag,
	                                             AelfricState eState, bool bRunOver)
	{
		if (bRunOver) return nullptr;
		if (!bPossessed)
			return "Click a villager to possess them -- the demon needs a body.";
		if (eState == AelfricState::Pursuing)
			return "Aelfric sees you -- break line of sight or [Shift] to sprint.";
		if (DP_IsObjectiveTag(eTag))
			return "Carry to the pentagram. 5 objectives end the night.";
		if (eTag == DP_ItemTag::SkeletonKey)
			return "Skeleton Key in hand -- press [F] on any locked door.";
		if (eTag == DP_ItemTag::Iron)
			return "Carry the iron to a forge. Forge it into a Skeleton Key.";
		if (DP_IsReagentTag(eTag))
			return "Reagent in hand -- see the description above for what it does.";
		if (eState == AelfricState::Suspicious)
			return "Aelfric is suspicious. Hold [Ctrl] to walk quietly.";
		return "Find an objective -- check chests, search the village.";
	}

	static const char* BuildTutorialHint(bool bPossessed, Zenith_EntityID xV,
	                                     AelfricState eState, bool bRunOver)
	{
		const DP_ItemTag eTag = bPossessed ? DP_Player::GetHeldItemTag(xV) : DP_ItemTag::None;
		return BuildTutorialHintForState(bPossessed, eTag, eState, bRunOver);
	}

	// 2026-05-21: archetype-specific status line. Each MVP archetype
	// has a unique gameplay rule that the placeholder cube visual
	// doesn't telegraph; this line surfaces the rule textually so the
	// player knows "I'm a Devout, possession is slow" etc.
	// Returns nullptr for Farmhand (baseline -- no special rule) so
	// the HUD line is hidden in that case.
	static const char* BuildArchetypeStatusText(const char* szArchetypeId)
	{
		if (szArchetypeId == nullptr) return nullptr;
		// String compare against the canonical archetype IDs from
		// Config/Archetypes.json. Beggar / Devout / Child are the
		// MVP variants with special rules; Farmhand returns null
		// (no surface needed).
		if (std::strcmp(szArchetypeId, "Beggar") == 0)
			return "BEGGAR -- Aelfric will not pursue you";
		if (std::strcmp(szArchetypeId, "Devout") == 0)
			return "DEVOUT -- possession requires a 0.8 s channel (priest LOS breaks it)";
		if (std::strcmp(szArchetypeId, "Child") == 0)
			return "CHILD -- cannot carry tools (Iron / Key); half life timer";
		// Farmhand / unknown -> hide.
		return nullptr;
	}

	// Per-archetype HUD tint matching DPVillager_Component's per-tag
	// villager-body tint, so the HUD line reads as "this villager".
	static Zenith_Maths::Vector4 ArchetypeStatusColor(const char* szArchetypeId)
	{
		if (szArchetypeId == nullptr) return Zenith_Maths::Vector4(0.95f, 0.85f, 0.65f, 1.0f);
		if (std::strcmp(szArchetypeId, "Beggar") == 0)
			return Zenith_Maths::Vector4(0.85f, 0.85f, 1.0f, 1.0f);   // safe blue
		if (std::strcmp(szArchetypeId, "Devout") == 0)
			return Zenith_Maths::Vector4(0.95f, 0.85f, 0.50f, 1.0f);  // candlelight yellow
		if (std::strcmp(szArchetypeId, "Child") == 0)
			return Zenith_Maths::Vector4(0.90f, 0.60f, 0.85f, 1.0f);  // pink
		return Zenith_Maths::Vector4(0.95f, 0.85f, 0.65f, 1.0f);      // farmhand neutral
	}

	// 2026-05-21: locked-door alert. Telegraphs the result of an
	// F-press on a key-gated door when the villager has no matching
	// key (or wrong tag). Reads the required key tag from the event
	// payload so the player knows what they need to find.
	static void BuildLockedDoorAlertText(char* szBuf, size_t uBufSize, DP_ItemTag eRequired)
	{
		const char* szKey = "a Key";
		if      (eRequired == DP_ItemTag::Key)         szKey = "a Key";
		else if (eRequired == DP_ItemTag::SkeletonKey) szKey = "a Skeleton Key";
		else if (eRequired == DP_ItemTag::Iron)        szKey = "an Iron"; // unusual but possible
		std::snprintf(szBuf, uBufSize, "LOCKED -- needs %s", szKey);
	}

	// =============== Detailed-HUD formatter helpers ===============
	// Each returns the formatted text via the caller-supplied buffer
	// (matches the BuildDawnText / BuildScentText style). Pure +
	// side-effect-free so unit tests pin the format without authoring
	// a UI canvas.

	// "Movement: SPRINT" / "Movement: WALK QUIET" / "Movement: Move".
	// Sprint wins ties (matches the actual gameplay precedence in
	// DPVillager::OnUpdate where sprint+quiet resolves to sprint).
	static void BuildMovementModeText(char* szBuf, size_t uBufSize,
	                                  bool bSprintingNow, bool bWalkQuietNow)
	{
		const char* szMode = "Move";
		if      (bSprintingNow)  szMode = "SPRINT";
		else if (bWalkQuietNow)  szMode = "WALK QUIET";
		std::snprintf(szBuf, uBufSize, "Movement: %s", szMode);
	}

	// "Vessels: alive / total". Caller guarantees iTotal > 0; for
	// iTotal == 0 the HUD hides the element entirely (no formatting).
	static void BuildVillagersAliveText(char* szBuf, size_t uBufSize,
	                                    int iAlive, int iTotal)
	{
		if (iAlive < 0) iAlive = 0;
		if (iTotal < 0) iTotal = 0;
		std::snprintf(szBuf, uBufSize, "Vessels: %d / %d", iAlive, iTotal);
	}

	// "Priest: <m> m" rounded to nearest metre. Negative input -> "0 m"
	// so the readout never shows nonsense if the distance is unset.
	static void BuildPriestDistanceText(char* szBuf, size_t uBufSize, float fMeters)
	{
		if (fMeters < 0.0f) fMeters = 0.0f;
		std::snprintf(szBuf, uBufSize, "Priest: %.0f m", fMeters);
	}

	// 3-level urgency for the PriestDistance readout colour. Thresholds:
	//   < 5 m  -> Danger (red)        priest can apprehend
	//   < 15 m -> Caution (amber)     priest within hearing
	//   else   -> Calm (grey)
	enum class PriestDanger : uint8_t
	{
		Calm    = 0,
		Caution = 1,
		Danger  = 2,
	};
	static PriestDanger PriestDangerForDistance(float fMeters)
	{
		if (fMeters < 5.0f)  return PriestDanger::Danger;
		if (fMeters < 15.0f) return PriestDanger::Caution;
		return PriestDanger::Calm;
	}
	static Zenith_Maths::Vector4 PriestDistanceColor(PriestDanger eDanger)
	{
		switch (eDanger)
		{
		case PriestDanger::Danger:  return Zenith_Maths::Vector4(1.0f, 0.3f, 0.3f, 1.0f);
		case PriestDanger::Caution: return Zenith_Maths::Vector4(1.0f, 0.8f, 0.4f, 1.0f);
		case PriestDanger::Calm:
		default:                    return Zenith_Maths::Vector4(0.85f, 0.85f, 0.85f, 1.0f);
		}
	}

	// "Time: M:SS" -- minutes:seconds (zero-padded seconds). Negative
	// input clamps to 0 so the timer never goes backwards on the HUD.
	static void BuildRunTimerText(char* szBuf, size_t uBufSize, float fSeconds)
	{
		if (fSeconds < 0.0f) fSeconds = 0.0f;
		const int iSec = static_cast<int>(fSeconds);
		std::snprintf(szBuf, uBufSize, "Time: %d:%02d", iSec / 60, iSec % 60);
	}

	// "F: interact with <type>" -- type is one of "door" / "chest" /
	// "forge" / "pentagram" / "noise machine" returned by
	// FindNearestInteractableType. nullptr type yields an empty string.
	static void BuildInteractHintText(char* szBuf, size_t uBufSize, const char* szType)
	{
		if (szType == nullptr || szType[0] == '\0')
		{
			if (uBufSize > 0) szBuf[0] = '\0';
			return;
		}
		std::snprintf(szBuf, uBufSize, "F: interact with %s", szType);
	}

	// "Life: X.X / X.X s". Caller passes the remaining + max life.
	static void BuildLifeNumericText(char* szBuf, size_t uBufSize, float fRemaining, float fMax)
	{
		if (fRemaining < 0.0f) fRemaining = 0.0f;
		if (fMax < 0.0f)       fMax = 0.0f;
		std::snprintf(szBuf, uBufSize, "Life: %.1f / %.0f s", fRemaining, fMax);
	}

	// "Archetype: <id>" -- id is the villager's m_strArchetypeId.
	// Empty id yields an empty string (HUD hides the element).
	static void BuildArchetypeText(char* szBuf, size_t uBufSize, const char* szArchetypeId)
	{
		if (szArchetypeId == nullptr || szArchetypeId[0] == '\0')
		{
			if (uBufSize > 0) szBuf[0] = '\0';
			return;
		}
		std::snprintf(szBuf, uBufSize, "Archetype: %s", szArchetypeId);
	}

	// One-line reagent description text. Returns nullptr for non-reagent
	// tags (Iron, Key, Spike, Wood, Objective*, None) so the help line
	// hides. Public so unit tests can pin every case.
	static const char* ReagentHelpText(DP_ItemTag eTag)
	{
		switch (eTag)
		{
		case DP_ItemTag::BellSoul:    return "BellSoul: rings on pickup -- alerts every priest on the map.";
		case DP_ItemTag::BogWater:    return "BogWater: evaporates 8 seconds after you drop it.";
		case DP_ItemTag::SkeletonKey: return "Skeleton Key: opens any locked door.";
		default:                       return nullptr;
		}
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
		m_bTimerStarted = false;
		m_fRunTimerSeconds = 0.0f;
	}
#endif

	// Compute the current Aelfric state. Classified from the DP_AI
	// priest-state forwarders (which iterate Priest components in the
	// active scene + read each agent's blackboard) so the HUD header no longer
	// includes the priest header. Priority order Pursuing > Suspicious > Calm
	// matches the previous inline classification.
	static AelfricState ComputeAelfricState()
	{
		if (DP_AI::IsAnyPriestPursuing())     return AelfricState::Pursuing;
		if (DP_AI::IsAnyPriestInvestigating()) return AelfricState::Suspicious;
		return AelfricState::Calm;
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

	static bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	// (FindNearestInteractableType + its ScanInteractables<T> helper live in
	// DP_Interactables::FindNearestInteractableType so the HUD header doesn't
	// include the interactable component headers (cross-component rule).)

	void SetStatusText(const char* szText, const Zenith_Maths::Vector4& xColor, float fHoldSeconds)
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr) return;
		Zenith_UIComponent& xUI = *pxUI;
		if (auto* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status"))
		{
			pxStatus->SetText(szText);
			pxStatus->SetColor(xColor);
			pxStatus->SetVisible(true);
		}
		// Negative or zero hold means "permanent" — Victory uses this.
		m_fStatusHoldRemaining = (fHoldSeconds > 0.0f) ? fHoldSeconds : -1.0f;
	}

	// The single factored subscription site. Called from OnAwake and from
	// the move operations (so the relocated instance re-captures the new
	// `this`). All five lambdas capture `this`.
	void SubscribeRunEvents()
	{
		m_xVictoryHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVictory>(
			[this](const DP_OnVictory&)
			{
				m_bRunOver = true;
				SetStatusText("VICTORY",
					Zenith_Maths::Vector4(0.3f, 1.0f, 0.3f, 1.0f),
					/*fHoldSeconds=*/0.0f /* permanent */);
			});
		m_xDeathHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVillagerDied>(
			[this](const DP_OnVillagerDied&)
			{
				SetStatusText("Possess another villager",
					Zenith_Maths::Vector4(0.9f, 0.2f, 0.2f, 1.0f),
					/*fHoldSeconds=*/2.5f);
			});
		// MVP-4.2 / 4.3.2: run-lost banner. Each cause has its own
		// copy line that hangs permanently (run is over). The pause-
		// menu R / Q shortcuts still work to restart / quit.
		m_xRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(
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

		// 2026-05-21: locked-door alert. The DPDoor's F-press fail path
		// dispatches DP_OnDoorLockRejected; the HUD turns that into a
		// brief "LOCKED -- needs Key" line so the player understands
		// why nothing happened. Tag is captured for the formatter so
		// post-MVP keys (SkeletonKey) telegraph correctly.
		m_xLockedDoorHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnDoorLockRejected>(
			[this](const DP_OnDoorLockRejected& xEvt)
			{
				m_eLastLockedDoorRequiredKey = xEvt.m_eRequiredKey;
				m_fLockedDoorAlertRemaining = 2.0f;
			});

		// 2026-05-21: priest-alerted -- bump the awareness indicator's
		// recent-alert flash so the player notices the transition even
		// if their eyes were on the playfield. Falling edge auto-clears
		// when the priest returns to patrol.
		m_xPriestAlertHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnPriestAlerted>(
			[this](const DP_OnPriestAlerted& xEvt)
			{
				m_fAwarenessFlashRemaining = 1.0f;
				m_eLastAlertKind = xEvt.m_eKind;
			});
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
		if (m_xLockedDoorHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(m_xLockedDoorHandle);
			m_xLockedDoorHandle = INVALID_EVENT_HANDLE;
		}
		if (m_xPriestAlertHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(m_xPriestAlertHandle);
			m_xPriestAlertHandle = INVALID_EVENT_HANDLE;
		}
	}

	// Move-helper: drop the source's this-capturing subscriptions and stand
	// up fresh ones against the new address iff the source was subscribed
	// (i.e. its OnAwake had run).
	void TakeOverSubscriptionsFrom(DPHUDController_Component& xOther)
	{
		const bool bWasSubscribed =
			xOther.m_xVictoryHandle     != INVALID_EVENT_HANDLE ||
			xOther.m_xDeathHandle       != INVALID_EVENT_HANDLE ||
			xOther.m_xRunLostHandle     != INVALID_EVENT_HANDLE ||
			xOther.m_xLockedDoorHandle  != INVALID_EVENT_HANDLE ||
			xOther.m_xPriestAlertHandle != INVALID_EVENT_HANDLE;
		xOther.TearDown();
		if (bWasSubscribed)
		{
			SubscribeRunEvents();
		}
	}

	Zenith_Entity m_xParentEntity;

	Zenith_EventHandle m_xVictoryHandle       = INVALID_EVENT_HANDLE;
	Zenith_EventHandle m_xDeathHandle         = INVALID_EVENT_HANDLE;
	Zenith_EventHandle m_xRunLostHandle       = INVALID_EVENT_HANDLE;
	Zenith_EventHandle m_xLockedDoorHandle    = INVALID_EVENT_HANDLE;
	Zenith_EventHandle m_xPriestAlertHandle   = INVALID_EVENT_HANDLE;
	float              m_fStatusHoldRemaining = -1.0f;  // <0 = permanent / not set
	bool               m_bRunLostReceived     = false;
	// MVP-4.3.2: set by EITHER the Victory or RunLost subscriber. Drives
	// the RestartPrompt UI element + gates the pause-menu R/Q shortcuts
	// when the player isn't paused but the run is over (e.g., they
	// watched "VICTORY" appear and just want to start a new run).
	bool               m_bRunOver             = false;
	DP_RunLostCause    m_eLastRunLostCause    = DP_RunLostCause::Apprehended;
	// 2026-05-21 player-feedback fields. m_fLockedDoorAlertRemaining
	// counts down from 2 s on DP_OnDoorLockRejected, gating the
	// LockedDoorAlert UI element's visibility. m_fAwarenessFlashRemaining
	// gates a brief "recently alerted" colour flash on the awareness
	// indicator so the player notices priest-state transitions even when
	// not looking at the icon.
	float              m_fLockedDoorAlertRemaining = 0.0f;
	DP_ItemTag         m_eLastLockedDoorRequiredKey = DP_ItemTag::Key;
	float              m_fAwarenessFlashRemaining   = 0.0f;
	DP_PriestAlertKind m_eLastAlertKind             = DP_PriestAlertKind::HeardNoise;
	// Detailed-HUD: run timer. Starts ticking on first possession and
	// freezes when the run ends (Victory or RunLost). Drives the
	// RunTimer UI element.
	bool               m_bTimerStarted        = false;
	float              m_fRunTimerSeconds     = 0.0f;
	// 2026-05-16 instructional HUD: [H] toggles the full-screen help
	// overlay (HelpBg rect + HelpTitle/HelpOverlay text). Defaults to
	// closed -- player opts in. Persists across frames; not reset by
	// possession changes (player likely wants the overlay back if they
	// re-open it mid-run).
	bool               m_bHelpVisible         = false;
};
