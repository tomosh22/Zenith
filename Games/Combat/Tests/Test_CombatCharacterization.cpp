#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_CombatCharacterization - characterization tests for the wave-2 graph
 * conversion of Combat's attack flow + round flow.
 *
 * Written against the C++ versions FIRST (Combat_PlayerComponent::UpdateAttack
 * and Combat_GameComponent::CheckGameState / TickComboTimer); the graph
 * versions must keep every one of these green unchanged. All probes go
 * through surfaces that survive the conversion: Combat_GameComponent statics,
 * Combat_DamageSystem health queries, and the real input path
 * (Zenith_InputSimulator -> Combat_PlayerController).
 *
 *   Combat_AttackFlow_Test    - steer the player into light-attack range via
 *                               held WASD (real input), left-click, assert the
 *                               enemy lost EXACTLY the light-attack 10 HP
 *                               (combo 1 => no multiplier), the combo counter
 *                               read 1, and the same swing never double-dips.
 *   Combat_RoundVictory_Test  - kill all enemies through the real damage-event
 *                               path; assert the round flow flips to VICTORY.
 *   Combat_RoundGameOver_Test - kill the player the same way; assert GAME_OVER.
 *   Combat_ComboTimer_Test    - NotifyComboHit(3) then idle; assert the timer
 *                               expiry resets the combo counter to 0.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "Combat/Combat_Bindings.h"
#include "Components/Combat_GameComponent.h"
#include "Components/Combat_PlayerComponent.h"
#include "Components/Combat_EnemyComponent.h"
#include "Components/Combat_DamageSystem.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "UI/Zenith_UIButton.h"
#include "Flux/Particles/Flux_ParticleGPUImpl.h"
#include "ZenithECS/Zenith_Query.h"

#include <cmath>

namespace
{
	bool GetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	Combat_PlayerComponent* GetPlayerComponent()
	{
		const Zenith_EntityID xId = Combat_GameComponent::GetPlayerEntityID();
		if (!xId.IsValid()) return nullptr;
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		Combat_PlayerComponent* pxPlayer = xEnt.TryGetComponent<Combat_PlayerComponent>();
		if (pxPlayer == nullptr) return nullptr;
		return pxPlayer;
	}

	// Nearest alive enemy to the player (XZ). Returns INVALID when none.
	Zenith_EntityID FindNearestAliveEnemy(const Zenith_Maths::Vector3& xFrom, float& fDistOut)
	{
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		for (const Zenith_EntityID xEnemy : Combat_GameComponent::GetEnemyEntityIDs())
		{
			if (Combat_DamageSystem::IsDead(xEnemy)) continue;
			Zenith_Maths::Vector3 xPos;
			if (!GetEntityPos(xEnemy, xPos)) continue;
			const float fDx = xPos.x - xFrom.x;
			const float fDz = xPos.z - xFrom.z;
			const float fSq = fDx * fDx + fDz * fDz;
			if (fSq < fBestSq) { fBestSq = fSq; xBest = xEnemy; }
		}
		fDistOut = std::sqrt(fBestSq);
		return xBest;
	}

	// ★ C1a -- MOVEMENT IS STEERED WITH EDGES NOW, NOT LEVELS. Combat's MOVE is
	// an ACTION (Combat_Bindings), and the action layer's key rows are fed by
	// the ORDERED TRANSITION LOG rather than polled. Zenith_InputSimulator::
	// SetKeyHeld writes only the simulator's LEVEL table, so a key "held" with
	// it reaches IsKeyDown and NOTHING ELSE: the player never moves, no
	// assertion fires at the call site, and the test dies much later on a steer
	// timeout with an unrelated message. SimulateKeyDown / SimulateKeyUp set the
	// same level AND queue the transition.
	//
	// Edges are emitted only on a CHANGE of intent -- re-pressing every frame
	// would queue four injections a frame into a 128-slot buffer for no gain.
	const Zenith_KeyCode g_aiMoveKeys[4] = { ZENITH_KEY_W, ZENITH_KEY_S, ZENITH_KEY_D, ZENITH_KEY_A };
	bool g_abMoveKeyDown[4] = {};

	void DriveMovementKeys(bool bForward, bool bBack, bool bRight, bool bLeft)
	{
		const bool abWanted[4] = { bForward, bBack, bRight, bLeft };
		for (int iKey = 0; iKey < 4; iKey++)
		{
			if (abWanted[iKey] == g_abMoveKeyDown[iKey])
			{
				continue;
			}
			if (abWanted[iKey])
			{
				Zenith_InputSimulator::SimulateKeyDown(g_aiMoveKeys[iKey]);
			}
			else
			{
				Zenith_InputSimulator::SimulateKeyUp(g_aiMoveKeys[iKey]);
			}
			g_abMoveKeyDown[iKey] = abWanted[iKey];
		}
	}

	void ReleaseMovementKeys()
	{
		DriveMovementKeys(false, false, false, false);
	}

	// The harness wipes held input between tests WITHOUT emitting releases, so a
	// Setup has to forget what it believes is down -- otherwise the first steer
	// frame skips a press it thinks it already sent and the player stands still.
	void ForgetMovementKeys()
	{
		for (bool& bDown : g_abMoveKeyDown)
		{
			bDown = false;
		}
	}

	bool WaitForPlaying()
	{
		return Combat_GameComponent::IsInPlayingState()
			&& Combat_GameComponent::GetPlayerEntityID().IsValid()
			&& Combat_GameComponent::GetEnemyEntityIDs().size() == 3;
	}

	Combat_EnemyComponent* GetEnemyComponent(Zenith_EntityID xId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		return xEnt.TryGetComponent<Combat_EnemyComponent>();
	}

	Zenith_UI::Zenith_UIButton* FindMenuPlayButton()
	{
		Zenith_UI::Zenith_UIButton* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Zenith_UIComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Zenith_UIComponent& xUI)
			{
				if (pxFound == nullptr)
					pxFound = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			});
		return pxFound;
	}
}

// ============================================================================
// Combat_AttackFlow_Test
// ============================================================================

namespace
{
	enum class AttackPhase { Boot, WaitPlaying, Steer, ClickUp, AwaitHit, Settle, Done };

	AttackPhase     g_eAttackPhase = AttackPhase::Boot;
	int             g_iPhaseFrame = 0;
	int             g_iClickRetries = 0;
	Zenith_EntityID g_xTargetEnemy;
	float           g_fHealthBefore = 0.0f;
	float           g_fObservedDelta = 0.0f;
	float           g_fHealthAfterHit = 0.0f;
	uint32_t        g_uComboAfterHit = 0;
	bool            g_bNoDoubleDip = false;
	bool            g_bAttackDone = false;
}

static void Setup_AttackFlow()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	ForgetMovementKeys();
	g_eAttackPhase = AttackPhase::Boot;
	g_iPhaseFrame = 0;
	g_iClickRetries = 0;
	g_xTargetEnemy = Zenith_EntityID();
	g_fHealthBefore = 0.0f;
	g_fObservedDelta = 0.0f;
	g_fHealthAfterHit = 0.0f;
	g_uComboAfterHit = 0;
	g_bNoDoubleDip = false;
	g_bAttackDone = false;
}

static bool Step_AttackFlow(int iFrame)
{
	switch (g_eAttackPhase)
	{
	case AttackPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eAttackPhase = AttackPhase::WaitPlaying;
		return true;

	case AttackPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_eAttackPhase = AttackPhase::Steer;
			return true;
		}
		return iFrame < 600;

	case AttackPhase::Steer:
	{
		Zenith_Maths::Vector3 xPlayerPos;
		if (!GetEntityPos(Combat_GameComponent::GetPlayerEntityID(), xPlayerPos)) return false;
		float fDist = 1e30f;
		const Zenith_EntityID xEnemy = FindNearestAliveEnemy(xPlayerPos, fDist);
		if (!xEnemy.IsValid()) return false;

		Zenith_Maths::Vector3 xEnemyPos;
		GetEntityPos(xEnemy, xEnemyPos);
		const float fDx = xEnemyPos.x - xPlayerPos.x;
		const float fDz = xEnemyPos.z - xPlayerPos.z;

		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		const bool bCanAttackNow = pxPlayer != nullptr &&
			(pxPlayer->GetController().GetState() == Combat_PlayerState::IDLE ||
			 pxPlayer->GetController().GetState() == Combat_PlayerState::WALKING);

		if (fDist <= 1.2f && bCanAttackNow)
		{
			// In light-attack range, facing the enemy (we walked toward it).
			// Click on the REAL input path the human uses.
			ReleaseMovementKeys();
			g_xTargetEnemy = xEnemy;
			g_fHealthBefore = Combat_DamageSystem::GetHealth(xEnemy);
			Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT);
			g_eAttackPhase = AttackPhase::ClickUp;
			return true;
		}

		// Walk toward the enemy: raw world-axis WASD (Combat's movement input
		// is not camera-relative), facing follows the move direction.
		DriveMovementKeys(fDz > 0.15f, fDz < -0.15f, fDx > 0.15f, fDx < -0.15f);
		return iFrame < 1500;
	}

	case AttackPhase::ClickUp:
		Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT);
		g_iPhaseFrame = 0;
		g_eAttackPhase = AttackPhase::AwaitHit;
		return true;

	case AttackPhase::AwaitHit:
	{
		const float fHealthNow = Combat_DamageSystem::GetHealth(g_xTargetEnemy);
		if (fHealthNow < g_fHealthBefore - 0.01f)
		{
			g_fObservedDelta = g_fHealthBefore - fHealthNow;
			g_fHealthAfterHit = fHealthNow;
			g_uComboAfterHit = Combat_GameComponent::GetComboCount();
			g_iPhaseFrame = 0;
			g_eAttackPhase = AttackPhase::Settle;
			return true;
		}
		// The light attack lasts 0.3 s + 0.2 s recovery; if the swing whiffed
		// (e.g. the enemy strafed out or the click landed during hit-stun),
		// re-approach and try again.
		if (++g_iPhaseFrame > 90)
		{
			if (++g_iClickRetries > 5) return false;
			g_iPhaseFrame = 0;
			g_eAttackPhase = AttackPhase::Steer;
		}
		return iFrame < 1700;
	}

	case AttackPhase::Settle:
		// Same swing must never hit the same enemy twice (hit set + the
		// 0.2 s invulnerability window). The enemy AI doesn't damage itself,
		// so the target's health must hold until we attack again.
		if (++g_iPhaseFrame < 40)
		{
			return true;
		}
		g_bNoDoubleDip = std::abs(Combat_DamageSystem::GetHealth(g_xTargetEnemy) - g_fHealthAfterHit) < 0.01f;
		g_bAttackDone = true;
		g_eAttackPhase = AttackPhase::Done;
		return false;

	case AttackPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_AttackFlow()
{
	Zenith_InputSimulator::ClearFixedDt();
	ReleaseMovementKeys();
	if (!g_bAttackDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AttackFlow] never completed (phase %d)", static_cast<int>(g_eAttackPhase));
		return false;
	}
	// Light attack, combo count 1 => bIsCombo false => EXACTLY 10 damage.
	if (std::abs(g_fObservedDelta - 10.0f) > 0.01f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AttackFlow] expected 10 damage, saw %.2f", g_fObservedDelta);
		return false;
	}
	if (g_uComboAfterHit != 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AttackFlow] expected combo 1 after hit, saw %u", g_uComboAfterHit);
		return false;
	}
	if (!g_bNoDoubleDip)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AttackFlow] same swing dealt damage twice");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xAttackFlowTest = {
	"Combat_AttackFlow_Test",
	&Setup_AttackFlow,
	&Step_AttackFlow,
	&Verify_AttackFlow,
	/*maxFrames*/ 1800,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAttackFlowTest);

// ============================================================================
// Combat_RoundVictory_Test
// ============================================================================

namespace
{
	enum class RoundPhase { Boot, WaitPlaying, Act, AwaitState, Done };

	RoundPhase g_eRoundPhase = RoundPhase::Boot;
	int        g_iRoundFrame = 0;
	bool       g_bSawTargetState = false;
}

static void Setup_RoundVictory()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRoundPhase = RoundPhase::Boot;
	g_iRoundFrame = 0;
	g_bSawTargetState = false;
}

static bool Step_RoundVictory(int iFrame)
{
	switch (g_eRoundPhase)
	{
	case RoundPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRoundPhase = RoundPhase::WaitPlaying;
		return true;

	case RoundPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_eRoundPhase = RoundPhase::Act;
			g_iRoundFrame = 0;
			return true;
		}
		return iFrame < 600;

	case RoundPhase::Act:
	{
		// Kill one enemy every 15 frames through the SAME damage-event path a
		// real swing uses (Combat_HitDetection dispatches Combat_DamageEvent).
		if (++g_iRoundFrame % 15 == 0)
		{
			for (const Zenith_EntityID xEnemy : Combat_GameComponent::GetEnemyEntityIDs())
			{
				if (Combat_DamageSystem::IsDead(xEnemy)) continue;
				Combat_DamageEvent xEvent;
				xEvent.m_uTargetEntityID = xEnemy;
				xEvent.m_uAttackerEntityID = Combat_GameComponent::GetPlayerEntityID();
				xEvent.m_fDamage = 60.0f;	// one-shot vs the 50 HP enemies
				xEvent.m_xHitDirection = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
				Zenith_EventDispatcher::Get().Dispatch(xEvent);
				break;
			}
		}
		bool bAnyAlive = false;
		for (const Zenith_EntityID xEnemy : Combat_GameComponent::GetEnemyEntityIDs())
		{
			if (!Combat_DamageSystem::IsDead(xEnemy)) { bAnyAlive = true; break; }
		}
		if (!bAnyAlive)
		{
			g_eRoundPhase = RoundPhase::AwaitState;
			g_iRoundFrame = 0;
		}
		return iFrame < 900;
	}

	case RoundPhase::AwaitState:
		if (Combat_GameComponent::GetGameState() == Combat_GameState::VICTORY)
		{
			g_bSawTargetState = true;
			g_eRoundPhase = RoundPhase::Done;
			return false;
		}
		return ++g_iRoundFrame < 120;

	case RoundPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_RoundVictory()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSawTargetState)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RoundVictory] state never reached VICTORY (now %d)",
			static_cast<int>(Combat_GameComponent::GetGameState()));
	}
	return g_bSawTargetState;
}

static const Zenith_AutomatedTest g_xRoundVictoryTest = {
	"Combat_RoundVictory_Test",
	&Setup_RoundVictory,
	&Step_RoundVictory,
	&Verify_RoundVictory,
	/*maxFrames*/ 1200,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoundVictoryTest);

// ============================================================================
// Combat_RoundGameOver_Test
// ============================================================================

static void Setup_RoundGameOver()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRoundPhase = RoundPhase::Boot;
	g_iRoundFrame = 0;
	g_bSawTargetState = false;
}

static bool Step_RoundGameOver(int iFrame)
{
	switch (g_eRoundPhase)
	{
	case RoundPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRoundPhase = RoundPhase::WaitPlaying;
		return true;

	case RoundPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_eRoundPhase = RoundPhase::Act;
			g_iRoundFrame = 0;
			return true;
		}
		return iFrame < 600;

	case RoundPhase::Act:
	{
		// One lethal hit on the player through the real damage-event path
		// (player max health 100).
		Combat_DamageEvent xEvent;
		xEvent.m_uTargetEntityID = Combat_GameComponent::GetPlayerEntityID();
		xEvent.m_fDamage = 200.0f;
		xEvent.m_xHitDirection = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		Zenith_EventDispatcher::Get().Dispatch(xEvent);
		g_eRoundPhase = RoundPhase::AwaitState;
		g_iRoundFrame = 0;
		return true;
	}

	case RoundPhase::AwaitState:
		if (Combat_GameComponent::GetGameState() == Combat_GameState::GAME_OVER)
		{
			g_bSawTargetState = true;
			g_eRoundPhase = RoundPhase::Done;
			return false;
		}
		return ++g_iRoundFrame < 120;

	case RoundPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_RoundGameOver()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSawTargetState)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RoundGameOver] state never reached GAME_OVER (now %d)",
			static_cast<int>(Combat_GameComponent::GetGameState()));
	}
	return g_bSawTargetState;
}

static const Zenith_AutomatedTest g_xRoundGameOverTest = {
	"Combat_RoundGameOver_Test",
	&Setup_RoundGameOver,
	&Step_RoundGameOver,
	&Verify_RoundGameOver,
	/*maxFrames*/ 900,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoundGameOverTest);

// ============================================================================
// Combat_ComboTimer_Test
// ============================================================================

namespace
{
	enum class ComboPhase { Boot, WaitPlaying, Prime, AwaitReset, Done };

	ComboPhase g_eComboPhase = ComboPhase::Boot;
	int        g_iComboFrame = 0;
	bool       g_bComboWasSet = false;
	bool       g_bComboReset = false;
}

static void Setup_ComboTimer()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eComboPhase = ComboPhase::Boot;
	g_iComboFrame = 0;
	g_bComboWasSet = false;
	g_bComboReset = false;
}

static bool Step_ComboTimer(int iFrame)
{
	switch (g_eComboPhase)
	{
	case ComboPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eComboPhase = ComboPhase::WaitPlaying;
		return true;

	case ComboPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_eComboPhase = ComboPhase::Prime;
			return true;
		}
		return iFrame < 600;

	case ComboPhase::Prime:
		// The same notification a successful hit pushes (UpdateAttack ->
		// NotifyComboHit); the round flow's timer tick must reset it.
		Combat_GameComponent::NotifyComboHit(3, 0.5f);
		g_bComboWasSet = (Combat_GameComponent::GetComboCount() == 3);
		g_iComboFrame = 0;
		g_eComboPhase = ComboPhase::AwaitReset;
		return true;

	case ComboPhase::AwaitReset:
		if (Combat_GameComponent::GetComboCount() == 0)
		{
			g_bComboReset = true;
			g_eComboPhase = ComboPhase::Done;
			return false;
		}
		// 0.5 s timer at fixed 60 Hz => reset by ~frame 30; allow slack.
		return ++g_iComboFrame < 180;

	case ComboPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ComboTimer()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bComboWasSet)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ComboTimer] NotifyComboHit(3) did not read back as 3");
	}
	if (!g_bComboReset)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ComboTimer] combo never reset (now %u)",
			Combat_GameComponent::GetComboCount());
	}
	return g_bComboWasSet && g_bComboReset;
}

static const Zenith_AutomatedTest g_xComboTimerTest = {
	"Combat_ComboTimer_Test",
	&Setup_ComboTimer,
	&Step_ComboTimer,
	&Verify_ComboTimer,
	/*maxFrames*/ 900,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xComboTimerTest);

// ============================================================================
// Combat_HeavyAttack_Test
// ----------------------------------------------------------------------------
// Steers into range and RIGHT-clicks a heavy attack. Asserts the enemy loses
// EXACTLY 25 HP (heavy base, combo 0 => bIsCombo false => no multiplier), the
// combo counter reads 0 (heavy resets the chain), the player was seen in the
// HEAVY_ATTACK state, and the swing never double-dips. Gates the heavy branch
// of the decomposed Combat_PlayerAttack graph (ActivateHitbox 25/2.0).
// ============================================================================

namespace
{
	enum class HeavyPhase { Boot, WaitPlaying, Steer, ClickUp, AwaitHit, Settle, Done };

	HeavyPhase      g_eHeavyPhase = HeavyPhase::Boot;
	int             g_iHeavyFrame = 0;
	int             g_iHeavyRetries = 0;
	Zenith_EntityID g_xHeavyTarget;
	float           g_fHeavyHealthBefore = 0.0f;
	float           g_fHeavyDelta = 0.0f;
	float           g_fHeavyHealthAfter = 0.0f;
	uint32_t        g_uHeavyComboAfter = 0;
	bool            g_bSawHeavyState = false;
	bool            g_bHeavyNoDoubleDip = false;
	bool            g_bHeavyDone = false;
}

static void Setup_HeavyAttack()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	ForgetMovementKeys();
	g_eHeavyPhase = HeavyPhase::Boot;
	g_iHeavyFrame = 0;
	g_iHeavyRetries = 0;
	g_xHeavyTarget = Zenith_EntityID();
	g_fHeavyHealthBefore = 0.0f;
	g_fHeavyDelta = 0.0f;
	g_fHeavyHealthAfter = 0.0f;
	g_uHeavyComboAfter = 0;
	g_bSawHeavyState = false;
	g_bHeavyNoDoubleDip = false;
	g_bHeavyDone = false;
}

static bool Step_HeavyAttack(int iFrame)
{
	switch (g_eHeavyPhase)
	{
	case HeavyPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eHeavyPhase = HeavyPhase::WaitPlaying;
		return true;

	case HeavyPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_eHeavyPhase = HeavyPhase::Steer;
			return true;
		}
		return iFrame < 600;

	case HeavyPhase::Steer:
	{
		Zenith_Maths::Vector3 xPlayerPos;
		if (!GetEntityPos(Combat_GameComponent::GetPlayerEntityID(), xPlayerPos)) return false;
		float fDist = 1e30f;
		const Zenith_EntityID xEnemy = FindNearestAliveEnemy(xPlayerPos, fDist);
		if (!xEnemy.IsValid()) return false;

		Zenith_Maths::Vector3 xEnemyPos;
		GetEntityPos(xEnemy, xEnemyPos);
		const float fDx = xEnemyPos.x - xPlayerPos.x;
		const float fDz = xEnemyPos.z - xPlayerPos.z;

		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		const bool bCanAttackNow = pxPlayer != nullptr &&
			(pxPlayer->GetController().GetState() == Combat_PlayerState::IDLE ||
			 pxPlayer->GetController().GetState() == Combat_PlayerState::WALKING);

		if (fDist <= 1.2f && bCanAttackNow)
		{
			ReleaseMovementKeys();
			g_xHeavyTarget = xEnemy;
			g_fHeavyHealthBefore = Combat_DamageSystem::GetHealth(xEnemy);
			Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_RIGHT);
			g_eHeavyPhase = HeavyPhase::ClickUp;
			return true;
		}

		DriveMovementKeys(fDz > 0.15f, fDz < -0.15f, fDx > 0.15f, fDx < -0.15f);
		return iFrame < 1500;
	}

	case HeavyPhase::ClickUp:
		Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_RIGHT);
		g_iHeavyFrame = 0;
		g_eHeavyPhase = HeavyPhase::AwaitHit;
		return true;

	case HeavyPhase::AwaitHit:
	{
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer != nullptr &&
			pxPlayer->GetController().GetState() == Combat_PlayerState::HEAVY_ATTACK)
		{
			g_bSawHeavyState = true;
		}
		const float fHealthNow = Combat_DamageSystem::GetHealth(g_xHeavyTarget);
		if (fHealthNow < g_fHeavyHealthBefore - 0.01f)
		{
			g_fHeavyDelta = g_fHeavyHealthBefore - fHealthNow;
			g_fHeavyHealthAfter = fHealthNow;
			g_uHeavyComboAfter = Combat_GameComponent::GetComboCount();
			g_iHeavyFrame = 0;
			g_eHeavyPhase = HeavyPhase::Settle;
			return true;
		}
		// Heavy attack lasts 0.6 s; give it a longer whiff window than light.
		if (++g_iHeavyFrame > 120)
		{
			if (++g_iHeavyRetries > 5) return false;
			g_iHeavyFrame = 0;
			g_eHeavyPhase = HeavyPhase::Steer;
		}
		return iFrame < 2400;
	}

	case HeavyPhase::Settle:
		if (++g_iHeavyFrame < 40)
		{
			return true;
		}
		g_bHeavyNoDoubleDip = std::abs(Combat_DamageSystem::GetHealth(g_xHeavyTarget) - g_fHeavyHealthAfter) < 0.01f;
		g_bHeavyDone = true;
		g_eHeavyPhase = HeavyPhase::Done;
		return false;

	case HeavyPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_HeavyAttack()
{
	Zenith_InputSimulator::ClearFixedDt();
	ReleaseMovementKeys();
	if (!g_bHeavyDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[HeavyAttack] never completed (phase %d)", static_cast<int>(g_eHeavyPhase));
		return false;
	}
	// Heavy attack, combo count 0 => bIsCombo false => EXACTLY 25 damage.
	if (std::abs(g_fHeavyDelta - 25.0f) > 0.01f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[HeavyAttack] expected 25 damage, saw %.2f", g_fHeavyDelta);
		return false;
	}
	if (g_uHeavyComboAfter != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[HeavyAttack] expected combo 0 after heavy hit, saw %u", g_uHeavyComboAfter);
		return false;
	}
	if (!g_bSawHeavyState)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[HeavyAttack] never observed HEAVY_ATTACK state");
		return false;
	}
	if (!g_bHeavyNoDoubleDip)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[HeavyAttack] same swing dealt damage twice");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xHeavyAttackTest = {
	"Combat_HeavyAttack_Test",
	&Setup_HeavyAttack,
	&Step_HeavyAttack,
	&Verify_HeavyAttack,
	/*maxFrames*/ 2600,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xHeavyAttackTest);

// ============================================================================
// Combat_PlayerCombo_Test
// ----------------------------------------------------------------------------
// Pure controller state-machine (no enemy needed): left-click starts
// LIGHT_ATTACK_1 (chain combo 1); after the swing finishes the combo window
// opens (state stays LIGHT_ATTACK_1); a second left-click inside the window
// chains to LIGHT_ATTACK_2 (chain combo 2). Gates the attack/combo transitions
// of the PlayerState state machine.
// ============================================================================

namespace
{
	enum class PComboPhase { Boot, WaitPlaying, Click1Down, Click1Up, WaitWindow, Click2Down, Click2Up, Observe, Done };

	PComboPhase g_ePCombo = PComboPhase::Boot;
	int         g_iPComboFrame = 0;
	uint32_t    g_uPChain1 = 99;
	uint32_t    g_uPChain2 = 99;
	bool        g_bSawLA1 = false;
	bool        g_bSawLA2 = false;
	bool        g_bPComboDone = false;
}

static void Setup_PlayerCombo()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_ePCombo = PComboPhase::Boot;
	g_iPComboFrame = 0;
	g_uPChain1 = 99;
	g_uPChain2 = 99;
	g_bSawLA1 = false;
	g_bSawLA2 = false;
	g_bPComboDone = false;
}

static bool Step_PlayerCombo(int iFrame)
{
	switch (g_ePCombo)
	{
	case PComboPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_ePCombo = PComboPhase::WaitPlaying;
		return true;

	case PComboPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_ePCombo = PComboPhase::Click1Down;
			return true;
		}
		return iFrame < 600;

	case PComboPhase::Click1Down:
		Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT);
		g_ePCombo = PComboPhase::Click1Up;
		return true;

	case PComboPhase::Click1Up:
		Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT);
		g_iPComboFrame = 0;
		g_ePCombo = PComboPhase::WaitWindow;
		return true;

	case PComboPhase::WaitWindow:
	{
		// The swing lasts 0.3 s (18 frames); the combo window then opens (state
		// stays LIGHT_ATTACK_1). Click again at frame ~22 - safely inside the
		// 0.5 s window - to chain.
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer != nullptr &&
			pxPlayer->GetController().GetState() == Combat_PlayerState::LIGHT_ATTACK_1)
		{
			g_bSawLA1 = true;
			g_uPChain1 = pxPlayer->GetController().GetComboCount();
		}
		if (++g_iPComboFrame >= 22)
		{
			g_ePCombo = PComboPhase::Click2Down;
		}
		return iFrame < 400;
	}

	case PComboPhase::Click2Down:
		Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT);
		g_ePCombo = PComboPhase::Click2Up;
		return true;

	case PComboPhase::Click2Up:
		Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT);
		g_iPComboFrame = 0;
		g_ePCombo = PComboPhase::Observe;
		return true;

	case PComboPhase::Observe:
	{
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer != nullptr &&
			pxPlayer->GetController().GetState() == Combat_PlayerState::LIGHT_ATTACK_2)
		{
			g_bSawLA2 = true;
			g_uPChain2 = pxPlayer->GetController().GetComboCount();
			g_bPComboDone = true;
			g_ePCombo = PComboPhase::Done;
			return false;
		}
		if (++g_iPComboFrame > 30)
		{
			g_bPComboDone = true;
			g_ePCombo = PComboPhase::Done;
			return false;
		}
		return iFrame < 500;
	}

	case PComboPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PlayerCombo()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bPComboDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerCombo] never completed (phase %d)", static_cast<int>(g_ePCombo));
		return false;
	}
	if (!g_bSawLA1 || g_uPChain1 != 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerCombo] first swing not LIGHT_ATTACK_1/combo1 (saw=%d chain=%u)",
			g_bSawLA1 ? 1 : 0, g_uPChain1);
		return false;
	}
	if (!g_bSawLA2 || g_uPChain2 != 2)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerCombo] chain did not reach LIGHT_ATTACK_2/combo2 (saw=%d chain=%u)",
			g_bSawLA2 ? 1 : 0, g_uPChain2);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPlayerComboTest = {
	"Combat_PlayerCombo_Test",
	&Setup_PlayerCombo,
	&Step_PlayerCombo,
	&Verify_PlayerCombo,
	/*maxFrames*/ 700,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPlayerComboTest);

// ============================================================================
// Combat_PlayerDodge_Test
// ----------------------------------------------------------------------------
// Space triggers a dodge (DODGING state); after the 0.4 s dodge the controller
// returns to IDLE. Gates the dodge transition + recovery of the PlayerState
// state machine.
// ============================================================================

namespace
{
	enum class DodgePhase { Boot, WaitPlaying, Dodge, ObserveDodge, AwaitRecover, Done };

	DodgePhase g_eDodge = DodgePhase::Boot;
	int        g_iDodgeFrame = 0;
	bool       g_bSawDodging = false;
	bool       g_bDodgeRecovered = false;
	bool       g_bDodgeDone = false;
}

static void Setup_PlayerDodge()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eDodge = DodgePhase::Boot;
	g_iDodgeFrame = 0;
	g_bSawDodging = false;
	g_bDodgeRecovered = false;
	g_bDodgeDone = false;
}

static bool Step_PlayerDodge(int iFrame)
{
	switch (g_eDodge)
	{
	case DodgePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eDodge = DodgePhase::WaitPlaying;
		return true;

	case DodgePhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_eDodge = DodgePhase::Dodge;
			return true;
		}
		return iFrame < 600;

	case DodgePhase::Dodge:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_SPACE);
		g_iDodgeFrame = 0;
		g_eDodge = DodgePhase::ObserveDodge;
		return true;

	case DodgePhase::ObserveDodge:
	{
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer != nullptr &&
			pxPlayer->GetController().GetState() == Combat_PlayerState::DODGING)
		{
			g_bSawDodging = true;
			g_iDodgeFrame = 0;
			g_eDodge = DodgePhase::AwaitRecover;
			return true;
		}
		if (++g_iDodgeFrame > 20)
		{
			g_bDodgeDone = true;
			g_eDodge = DodgePhase::Done;
			return false;
		}
		return iFrame < 400;
	}

	case DodgePhase::AwaitRecover:
	{
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer != nullptr &&
			pxPlayer->GetController().GetState() == Combat_PlayerState::IDLE)
		{
			g_bDodgeRecovered = true;
			g_bDodgeDone = true;
			g_eDodge = DodgePhase::Done;
			return false;
		}
		if (++g_iDodgeFrame > 90)
		{
			g_bDodgeDone = true;
			g_eDodge = DodgePhase::Done;
			return false;
		}
		return iFrame < 500;
	}

	case DodgePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PlayerDodge()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bDodgeDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerDodge] never completed (phase %d)", static_cast<int>(g_eDodge));
		return false;
	}
	if (!g_bSawDodging)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerDodge] Space never produced DODGING state");
		return false;
	}
	if (!g_bDodgeRecovered)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerDodge] dodge never recovered to IDLE");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPlayerDodgeTest = {
	"Combat_PlayerDodge_Test",
	&Setup_PlayerDodge,
	&Step_PlayerDodge,
	&Verify_PlayerDodge,
	/*maxFrames*/ 700,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPlayerDodgeTest);

// ============================================================================
// Combat_PlayerHitStun_Test
// ----------------------------------------------------------------------------
// TriggerHitStun (the external damage-target entry point) forces HIT_STUN for
// 0.3 s, then the controller returns to IDLE. Gates the hit-stun transition +
// recovery of the PlayerState state machine (the contract the graph must keep,
// whether or not gameplay currently wires it).
// ============================================================================

namespace
{
	enum class PHitStunPhase { Boot, WaitPlaying, Stun, ObserveStun, AwaitRecover, Done };

	PHitStunPhase g_ePHitStun = PHitStunPhase::Boot;
	int           g_iPHitStunFrame = 0;
	bool          g_bSawPHitStun = false;
	bool          g_bPHitStunRecovered = false;
	bool          g_bPHitStunDone = false;
}

static void Setup_PlayerHitStun()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_ePHitStun = PHitStunPhase::Boot;
	g_iPHitStunFrame = 0;
	g_bSawPHitStun = false;
	g_bPHitStunRecovered = false;
	g_bPHitStunDone = false;
}

static bool Step_PlayerHitStun(int iFrame)
{
	switch (g_ePHitStun)
	{
	case PHitStunPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_ePHitStun = PHitStunPhase::WaitPlaying;
		return true;

	case PHitStunPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_ePHitStun = PHitStunPhase::Stun;
			return true;
		}
		return iFrame < 600;

	case PHitStunPhase::Stun:
	{
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer == nullptr) return false;
		pxPlayer->TriggerHitStun(0.3f);
		g_iPHitStunFrame = 0;
		g_ePHitStun = PHitStunPhase::ObserveStun;
		return true;
	}

	case PHitStunPhase::ObserveStun:
	{
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer != nullptr &&
			pxPlayer->GetController().GetState() == Combat_PlayerState::HIT_STUN)
		{
			g_bSawPHitStun = true;
			g_iPHitStunFrame = 0;
			g_ePHitStun = PHitStunPhase::AwaitRecover;
			return true;
		}
		if (++g_iPHitStunFrame > 10)
		{
			g_bPHitStunDone = true;
			g_ePHitStun = PHitStunPhase::Done;
			return false;
		}
		return iFrame < 400;
	}

	case PHitStunPhase::AwaitRecover:
	{
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer != nullptr &&
			pxPlayer->GetController().GetState() == Combat_PlayerState::IDLE)
		{
			g_bPHitStunRecovered = true;
			g_bPHitStunDone = true;
			g_ePHitStun = PHitStunPhase::Done;
			return false;
		}
		if (++g_iPHitStunFrame > 60)
		{
			g_bPHitStunDone = true;
			g_ePHitStun = PHitStunPhase::Done;
			return false;
		}
		return iFrame < 500;
	}

	case PHitStunPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PlayerHitStun()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bPHitStunDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerHitStun] never completed (phase %d)", static_cast<int>(g_ePHitStun));
		return false;
	}
	if (!g_bSawPHitStun)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerHitStun] TriggerHitStun never produced HIT_STUN state");
		return false;
	}
	if (!g_bPHitStunRecovered)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerHitStun] hit-stun never recovered to IDLE");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPlayerHitStunTest = {
	"Combat_PlayerHitStun_Test",
	&Setup_PlayerHitStun,
	&Step_PlayerHitStun,
	&Verify_PlayerHitStun,
	/*maxFrames*/ 700,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPlayerHitStunTest);

// ============================================================================
// Combat_EnemyEngages_Test
// ----------------------------------------------------------------------------
// The player stands still; the enemies chase in and attack. Asserts that some
// enemy reaches the CHASING state AND the player takes damage (an enemy reached
// ATTACKING range and its hitbox connected). Gates the enemy brain's chase +
// attack decisions and the enemy->player damage seam. We stop at the first
// damage, well before the player could die.
// ============================================================================

namespace
{
	enum class EngagePhase { Boot, WaitPlaying, Observe, Done };

	EngagePhase g_eEngage = EngagePhase::Boot;
	int         g_iEngageFrame = 0;
	float       g_fEngagePlayerHealth = 0.0f;
	bool        g_bSawChasing = false;
	bool        g_bPlayerDamaged = false;
	bool        g_bEngageDone = false;
}

static void Setup_EnemyEngages()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eEngage = EngagePhase::Boot;
	g_iEngageFrame = 0;
	g_fEngagePlayerHealth = 0.0f;
	g_bSawChasing = false;
	g_bPlayerDamaged = false;
	g_bEngageDone = false;
}

static bool Step_EnemyEngages(int iFrame)
{
	switch (g_eEngage)
	{
	case EngagePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eEngage = EngagePhase::WaitPlaying;
		return true;

	case EngagePhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_fEngagePlayerHealth = Combat_DamageSystem::GetHealth(Combat_GameComponent::GetPlayerEntityID());
			g_iEngageFrame = 0;
			g_eEngage = EngagePhase::Observe;
			return true;
		}
		return iFrame < 600;

	case EngagePhase::Observe:
	{
		if (!g_bSawChasing)
		{
			for (const Zenith_EntityID xId : Combat_GameComponent::GetEnemyEntityIDs())
			{
				Combat_EnemyComponent* pxEnemy = GetEnemyComponent(xId);
				if (pxEnemy != nullptr && pxEnemy->GetAI().GetState() == Combat_EnemyState::CHASING)
				{
					g_bSawChasing = true;
					break;
				}
			}
		}
		if (!g_bPlayerDamaged)
		{
			const float fNow = Combat_DamageSystem::GetHealth(Combat_GameComponent::GetPlayerEntityID());
			if (fNow < g_fEngagePlayerHealth - 0.01f)
			{
				g_bPlayerDamaged = true;
			}
		}
		if (g_bSawChasing && g_bPlayerDamaged)
		{
			g_bEngageDone = true;
			g_eEngage = EngagePhase::Done;
			return false;
		}
		if (++g_iEngageFrame > 1200)
		{
			g_bEngageDone = true;
			g_eEngage = EngagePhase::Done;
			return false;
		}
		return iFrame < 1400;
	}

	case EngagePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_EnemyEngages()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bEngageDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[EnemyEngages] never completed (phase %d)", static_cast<int>(g_eEngage));
		return false;
	}
	if (!g_bSawChasing)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[EnemyEngages] no enemy ever reached CHASING");
		return false;
	}
	if (!g_bPlayerDamaged)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[EnemyEngages] player never took enemy damage");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xEnemyEngagesTest = {
	"Combat_EnemyEngages_Test",
	&Setup_EnemyEngages,
	&Step_EnemyEngages,
	&Verify_EnemyEngages,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xEnemyEngagesTest);

// ============================================================================
// Combat_EnemyHitStun_Test
// ----------------------------------------------------------------------------
// TriggerHitStun on a live enemy forces HIT_STUN for 0.3 s, then the enemy
// recovers (to CHASING, or IDLE if its target is gone). Gates the enemy brain's
// hit-stun transition + recovery.
// ============================================================================

namespace
{
	enum class EHitStunPhase { Boot, WaitPlaying, Stun, ObserveStun, AwaitRecover, Done };

	EHitStunPhase   g_eEHitStun = EHitStunPhase::Boot;
	int             g_iEHitStunFrame = 0;
	Zenith_EntityID g_xEHitStunTarget;
	bool            g_bSawEHitStun = false;
	bool            g_bEHitStunRecovered = false;
	bool            g_bEHitStunDone = false;
}

static void Setup_EnemyHitStun()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eEHitStun = EHitStunPhase::Boot;
	g_iEHitStunFrame = 0;
	g_xEHitStunTarget = Zenith_EntityID();
	g_bSawEHitStun = false;
	g_bEHitStunRecovered = false;
	g_bEHitStunDone = false;
}

static bool Step_EnemyHitStun(int iFrame)
{
	switch (g_eEHitStun)
	{
	case EHitStunPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eEHitStun = EHitStunPhase::WaitPlaying;
		return true;

	case EHitStunPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_eEHitStun = EHitStunPhase::Stun;
			return true;
		}
		return iFrame < 600;

	case EHitStunPhase::Stun:
	{
		for (const Zenith_EntityID xId : Combat_GameComponent::GetEnemyEntityIDs())
		{
			Combat_EnemyComponent* pxEnemy = GetEnemyComponent(xId);
			if (pxEnemy != nullptr && pxEnemy->IsAlive())
			{
				g_xEHitStunTarget = xId;
				pxEnemy->TriggerHitStun();
				break;
			}
		}
		if (!g_xEHitStunTarget.IsValid()) return false;
		g_iEHitStunFrame = 0;
		g_eEHitStun = EHitStunPhase::ObserveStun;
		return true;
	}

	case EHitStunPhase::ObserveStun:
	{
		Combat_EnemyComponent* pxEnemy = GetEnemyComponent(g_xEHitStunTarget);
		if (pxEnemy != nullptr && pxEnemy->GetAI().GetState() == Combat_EnemyState::HIT_STUN)
		{
			g_bSawEHitStun = true;
			g_iEHitStunFrame = 0;
			g_eEHitStun = EHitStunPhase::AwaitRecover;
			return true;
		}
		if (++g_iEHitStunFrame > 10)
		{
			g_bEHitStunDone = true;
			g_eEHitStun = EHitStunPhase::Done;
			return false;
		}
		return iFrame < 400;
	}

	case EHitStunPhase::AwaitRecover:
	{
		Combat_EnemyComponent* pxEnemy = GetEnemyComponent(g_xEHitStunTarget);
		if (pxEnemy != nullptr)
		{
			const Combat_EnemyState eState = pxEnemy->GetAI().GetState();
			if (eState == Combat_EnemyState::CHASING || eState == Combat_EnemyState::IDLE)
			{
				g_bEHitStunRecovered = true;
				g_bEHitStunDone = true;
				g_eEHitStun = EHitStunPhase::Done;
				return false;
			}
		}
		if (++g_iEHitStunFrame > 60)
		{
			g_bEHitStunDone = true;
			g_eEHitStun = EHitStunPhase::Done;
			return false;
		}
		return iFrame < 500;
	}

	case EHitStunPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_EnemyHitStun()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bEHitStunDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[EnemyHitStun] never completed (phase %d)", static_cast<int>(g_eEHitStun));
		return false;
	}
	if (!g_bSawEHitStun)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[EnemyHitStun] TriggerHitStun never produced HIT_STUN state");
		return false;
	}
	if (!g_bEHitStunRecovered)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[EnemyHitStun] enemy never recovered from hit-stun");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xEnemyHitStunTest = {
	"Combat_EnemyHitStun_Test",
	&Setup_EnemyHitStun,
	&Step_EnemyHitStun,
	&Verify_EnemyHitStun,
	/*maxFrames*/ 700,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xEnemyHitStunTest);

// ============================================================================
// Combat_PauseResume_Test
// ----------------------------------------------------------------------------
// P toggles PLAYING <-> PAUSED. Gates the GameFlow pause/resume transition.
// ============================================================================

namespace
{
	enum class PausePhase { Boot, WaitPlaying, PressPause, AwaitPaused, PressResume, AwaitResumed, Done };

	PausePhase g_ePause = PausePhase::Boot;
	int        g_iPauseFrame = 0;
	bool       g_bSawPaused = false;
	bool       g_bSawResumed = false;
	bool       g_bPauseDone = false;
}

static void Setup_PauseResume()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_ePause = PausePhase::Boot;
	g_iPauseFrame = 0;
	g_bSawPaused = false;
	g_bSawResumed = false;
	g_bPauseDone = false;
}

static bool Step_PauseResume(int iFrame)
{
	switch (g_ePause)
	{
	case PausePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_ePause = PausePhase::WaitPlaying;
		return true;

	case PausePhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_ePause = PausePhase::PressPause;
			return true;
		}
		return iFrame < 600;

	case PausePhase::PressPause:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P);
		g_iPauseFrame = 0;
		g_ePause = PausePhase::AwaitPaused;
		return true;

	case PausePhase::AwaitPaused:
		if (Combat_GameComponent::GetGameState() == Combat_GameState::PAUSED)
		{
			g_bSawPaused = true;
			g_ePause = PausePhase::PressResume;
			return true;
		}
		if (++g_iPauseFrame > 20) { g_bPauseDone = true; g_ePause = PausePhase::Done; return false; }
		return iFrame < 400;

	case PausePhase::PressResume:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P);
		g_iPauseFrame = 0;
		g_ePause = PausePhase::AwaitResumed;
		return true;

	case PausePhase::AwaitResumed:
		if (Combat_GameComponent::GetGameState() == Combat_GameState::PLAYING)
		{
			g_bSawResumed = true;
			g_bPauseDone = true;
			g_ePause = PausePhase::Done;
			return false;
		}
		if (++g_iPauseFrame > 20) { g_bPauseDone = true; g_ePause = PausePhase::Done; return false; }
		return iFrame < 500;

	case PausePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PauseResume()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bPauseDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PauseResume] never completed (phase %d)", static_cast<int>(g_ePause));
		return false;
	}
	if (!g_bSawPaused)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PauseResume] P did not pause the game");
		return false;
	}
	if (!g_bSawResumed)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PauseResume] P did not resume the game");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPauseResumeTest = {
	"Combat_PauseResume_Test",
	&Setup_PauseResume,
	&Step_PauseResume,
	&Verify_PauseResume,
	/*maxFrames*/ 700,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPauseResumeTest);

// ============================================================================
// Combat_ReturnToMenu_Test
// ----------------------------------------------------------------------------
// Escape during play returns to MAIN_MENU. Gates the GameFlow menu transition.
// ============================================================================

namespace
{
	enum class MenuPhase { Boot, WaitPlaying, PressEsc, AwaitMenu, Done };

	MenuPhase g_eMenu = MenuPhase::Boot;
	int       g_iMenuFrame = 0;
	bool      g_bSawMenu = false;
	bool      g_bMenuDone = false;
}

static void Setup_ReturnToMenu()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eMenu = MenuPhase::Boot;
	g_iMenuFrame = 0;
	g_bSawMenu = false;
	g_bMenuDone = false;
}

static bool Step_ReturnToMenu(int iFrame)
{
	switch (g_eMenu)
	{
	case MenuPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eMenu = MenuPhase::WaitPlaying;
		return true;

	case MenuPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_eMenu = MenuPhase::PressEsc;
			return true;
		}
		return iFrame < 600;

	case MenuPhase::PressEsc:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iMenuFrame = 0;
		g_eMenu = MenuPhase::AwaitMenu;
		return true;

	case MenuPhase::AwaitMenu:
		if (Combat_GameComponent::GetGameState() == Combat_GameState::MAIN_MENU)
		{
			g_bSawMenu = true;
			g_bMenuDone = true;
			g_eMenu = MenuPhase::Done;
			return false;
		}
		if (++g_iMenuFrame > 120) { g_bMenuDone = true; g_eMenu = MenuPhase::Done; return false; }
		return iFrame < 800;

	case MenuPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ReturnToMenu()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bMenuDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ReturnToMenu] never completed (phase %d)", static_cast<int>(g_eMenu));
		return false;
	}
	if (!g_bSawMenu)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ReturnToMenu] Escape did not return to MAIN_MENU (now %d)",
			static_cast<int>(Combat_GameComponent::GetGameState()));
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xReturnToMenuTest = {
	"Combat_ReturnToMenu_Test",
	&Setup_ReturnToMenu,
	&Step_ReturnToMenu,
	&Verify_ReturnToMenu,
	/*maxFrames*/ 1000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xReturnToMenuTest);

// ============================================================================
// Combat_Restart_Test
// ----------------------------------------------------------------------------
// R during play resets the round: a damaged enemy is replaced by a fresh wave
// of 3 full-health enemies. Gates the GameFlow reset transition.
// ============================================================================

namespace
{
	enum class RestartPhase { Boot, WaitPlaying, Damage, PressReset, AwaitReset, Done };

	RestartPhase g_eRestart = RestartPhase::Boot;
	int          g_iRestartFrame = 0;
	bool         g_bRestartFresh = false;
	bool         g_bRestartDone = false;
}

static void Setup_Restart()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRestart = RestartPhase::Boot;
	g_iRestartFrame = 0;
	g_bRestartFresh = false;
	g_bRestartDone = false;
}

static bool Step_Restart(int iFrame)
{
	switch (g_eRestart)
	{
	case RestartPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRestart = RestartPhase::WaitPlaying;
		return true;

	case RestartPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			g_eRestart = RestartPhase::Damage;
			return true;
		}
		return iFrame < 600;

	case RestartPhase::Damage:
	{
		// Wound the first enemy so a fresh wave is distinguishable from a no-op.
		for (const Zenith_EntityID xId : Combat_GameComponent::GetEnemyEntityIDs())
		{
			Combat_DamageEvent xEvent;
			xEvent.m_uTargetEntityID = xId;
			xEvent.m_uAttackerEntityID = Combat_GameComponent::GetPlayerEntityID();
			xEvent.m_fDamage = 20.0f;
			xEvent.m_xHitDirection = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
			Zenith_EventDispatcher::Get().Dispatch(xEvent);
			break;
		}
		g_eRestart = RestartPhase::PressReset;
		return true;
	}

	case RestartPhase::PressReset:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_R);
		g_iRestartFrame = 0;
		g_eRestart = RestartPhase::AwaitReset;
		return true;

	case RestartPhase::AwaitReset:
	{
		// After a reset there should again be exactly 3 enemies, all at full
		// (50) health - the wounded one is gone.
		if (++g_iRestartFrame > 20)
		{
			const auto& xEnemies = Combat_GameComponent::GetEnemyEntityIDs();
			bool bAllFull = (xEnemies.size() == 3);
			for (const Zenith_EntityID xId : xEnemies)
			{
				if (Combat_DamageSystem::GetHealth(xId) < 49.9f)
				{
					bAllFull = false;
				}
			}
			g_bRestartFresh = bAllFull &&
				Combat_GameComponent::GetGameState() == Combat_GameState::PLAYING;
			g_bRestartDone = true;
			g_eRestart = RestartPhase::Done;
			return false;
		}
		return iFrame < 900;
	}

	case RestartPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_Restart()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bRestartDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Restart] never completed (phase %d)", static_cast<int>(g_eRestart));
		return false;
	}
	if (!g_bRestartFresh)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Restart] R did not respawn a fresh 3-enemy wave");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xRestartTest = {
	"Combat_Restart_Test",
	&Setup_Restart,
	&Step_Restart,
	&Verify_Restart,
	/*maxFrames*/ 1000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRestartTest);

// ============================================================================
// Combat_MenuPlay_Test
// ----------------------------------------------------------------------------
// From the MAIN_MENU scene, clicking the Play button loads the arena and starts
// the round. Gates the GameFlow menu Play -> load-scene transition.
// ============================================================================

namespace
{
	enum class MPlayPhase { Boot, WaitMenu, ClickPlay, AwaitPlaying, Done };

	MPlayPhase g_eMPlay = MPlayPhase::Boot;
	int        g_iMPlayFrame = 0;
	bool       g_bMPlayStarted = false;
	bool       g_bMPlayDone = false;
}

static void Setup_MenuPlay()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eMPlay = MPlayPhase::Boot;
	g_iMPlayFrame = 0;
	g_bMPlayStarted = false;
	g_bMPlayDone = false;
}

static bool Step_MenuPlay(int iFrame)
{
	switch (g_eMPlay)
	{
	case MPlayPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eMPlay = MPlayPhase::WaitMenu;
		return true;

	case MPlayPhase::WaitMenu:
		if (Combat_GameComponent::GetGameState() == Combat_GameState::MAIN_MENU &&
			FindMenuPlayButton() != nullptr)
		{
			g_eMPlay = MPlayPhase::ClickPlay;
			return true;
		}
		return iFrame < 600;

	case MPlayPhase::ClickPlay:
	{
		Zenith_UI::Zenith_UIButton* pxPlay = FindMenuPlayButton();
		if (pxPlay == nullptr) return false;
		pxPlay->Activate();
		g_iMPlayFrame = 0;
		g_eMPlay = MPlayPhase::AwaitPlaying;
		return true;
	}

	case MPlayPhase::AwaitPlaying:
		if (WaitForPlaying())
		{
			g_bMPlayStarted = true;
			g_bMPlayDone = true;
			g_eMPlay = MPlayPhase::Done;
			return false;
		}
		if (++g_iMPlayFrame > 600) { g_bMPlayDone = true; g_eMPlay = MPlayPhase::Done; return false; }
		return iFrame < 1200;

	case MPlayPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_MenuPlay()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bMPlayDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MenuPlay] never completed (phase %d)", static_cast<int>(g_eMPlay));
		return false;
	}
	if (!g_bMPlayStarted)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MenuPlay] clicking Play did not start the arena");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xMenuPlayTest = {
	"Combat_MenuPlay_Test",
	&Setup_MenuPlay,
	&Step_MenuPlay,
	&Verify_MenuPlay,
	/*maxFrames*/ 1400,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMenuPlayTest);

// ============================================================================
// Combat_GPUParticles_Test -- the arena candles are the engine's in-tree consumer
// of the GPU-driven particle path, and this is the ONLY check that can see the
// thing that path was missing: that the compute pass produces instances the draw
// actually consumes.
//
// Nothing CPU-side can stand in for it. Every other signal -- registrations,
// ring occupancy, the frame latch -- was equally true back when the compute
// shader wrote an instance buffer nobody read, `Flux_ParticleGPUImpl::Initialise`
// had no caller, and no emitter ever registered. The instanceCount in the
// partition's VkDrawIndexedIndirectCommand is written by the GPU and read back
// here, so it is non-zero only if the dispatch really ran over live particles.
//
// requiresGraphics: DownloadBufferData zero-fills on the Null backend, so this is
// windowed-only truth (same contract as Flux_GrassImpl::ReadbackVisibleBladeCount).
// ============================================================================

namespace
{
	enum class GPUPPhase { Boot, WaitMenu, ClickPlay, AwaitPlaying, Settle, Done };

	GPUPPhase g_eGPUP = GPUPPhase::Boot;
	int       g_iGPUPFrame = 0;
	bool      g_bGPUPReachedArena = false;
	bool      g_bGPUPDone = false;
	u_int     g_uGPUPEmitters = 0;
	u_int     g_uGPUPRingOccupancy = 0;
	u_int     g_uGPUPAdditiveInstances = 0;
	u_int     g_uGPUPAlphaInstances = 0;

	// Candles emit for a while before the readback: a freshly loaded arena has
	// spawned nothing yet, and the point is to observe SURVIVING particles being
	// compacted, not the empty first frame.
	constexpr int kiGPUP_SETTLE_FRAMES = 90;
}

static void Setup_GPUParticles()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eGPUP = GPUPPhase::Boot;
	g_iGPUPFrame = 0;
	g_bGPUPReachedArena = false;
	g_bGPUPDone = false;
	g_uGPUPEmitters = 0;
	g_uGPUPRingOccupancy = 0;
	g_uGPUPAdditiveInstances = 0;
	g_uGPUPAlphaInstances = 0;
}

static bool Step_GPUParticles(int iFrame)
{
	switch (g_eGPUP)
	{
	case GPUPPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eGPUP = GPUPPhase::WaitMenu;
		return true;

	case GPUPPhase::WaitMenu:
		if (Combat_GameComponent::GetGameState() == Combat_GameState::MAIN_MENU &&
			FindMenuPlayButton() != nullptr)
		{
			g_eGPUP = GPUPPhase::ClickPlay;
			return true;
		}
		return iFrame < 600;

	case GPUPPhase::ClickPlay:
	{
		Zenith_UI::Zenith_UIButton* pxPlay = FindMenuPlayButton();
		if (pxPlay == nullptr) return false;
		pxPlay->Activate();
		g_iGPUPFrame = 0;
		g_eGPUP = GPUPPhase::AwaitPlaying;
		return true;
	}

	case GPUPPhase::AwaitPlaying:
		if (WaitForPlaying())
		{
			g_bGPUPReachedArena = true;
			g_iGPUPFrame = 0;
			g_eGPUP = GPUPPhase::Settle;
			return true;
		}
		if (++g_iGPUPFrame > 600) { g_bGPUPDone = true; g_eGPUP = GPUPPhase::Done; return false; }
		return iFrame < 1200;

	case GPUPPhase::Settle:
		if (++g_iGPUPFrame < kiGPUP_SETTLE_FRAMES)
		{
			return true;
		}
		// Sample the CPU-side facts on the LAST stepped frame; the GPU readback
		// happens in Verify (it idles the device, so it must not sit in a frame path).
		g_xEngine.Scenes().QueryAllScenes<Zenith_ParticleEmitterComponent>()
			.ForEach([](Zenith_EntityID, Zenith_ParticleEmitterComponent& xEmitter)
		{
			if (!xEmitter.UsesGPUCompute()) return;
			g_uGPUPEmitters++;
			g_uGPUPRingOccupancy += g_xEngine.ParticleGPU().GetEmitterParticleCount(xEmitter.GetGPUEmitterID());
		});
		g_bGPUPDone = true;
		g_eGPUP = GPUPPhase::Done;
		return false;

	case GPUPPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_GPUParticles()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bGPUPDone || !g_bGPUPReachedArena)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[GPUParticles] never reached the arena (phase %d)", static_cast<int>(g_eGPUP));
		return false;
	}

	Flux_ParticleGPUImpl& xGPU = g_xEngine.ParticleGPU();
	g_uGPUPAlphaInstances    = xGPU.ReadbackPartitionInstanceCount(uFLUX_PARTICLE_PARTITION_ALPHA);
	g_uGPUPAdditiveInstances = xGPU.ReadbackPartitionInstanceCount(uFLUX_PARTICLE_PARTITION_ADDITIVE);

	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[GPUParticles] %u GPU emitters, %u ring slots spawned, instanceCount alpha=%u additive=%u",
		g_uGPUPEmitters, g_uGPUPRingOccupancy, g_uGPUPAlphaInstances, g_uGPUPAdditiveInstances);

	if (g_uGPUPEmitters == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[GPUParticles] no emitter registered on the GPU path -- Combat_Flame's m_bUseGPUCompute, "
			"or the pool reservation, regressed");
		return false;
	}
	if (g_uGPUPRingOccupancy == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[GPUParticles] the emitters registered but never spawned -- Emit() is not reaching QueueSpawn, "
			"or PreExecuteCompute is not draining it");
		return false;
	}
	if (g_uGPUPAdditiveInstances == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[GPUParticles] the candles are additive and spawned %u particles, but the additive partition's "
			"indirect instanceCount read 0 -- the compute pass is not compacting survivors into the buffer "
			"the draw fetches (this is exactly the state the path shipped in before it was wired)",
			g_uGPUPRingOccupancy);
		return false;
	}
	if (g_uGPUPAdditiveInstances > Flux_ParticleGPUImpl::s_uPartitionCapacity)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[GPUParticles] the additive partition counted %u instances but holds %u -- the draw would fetch "
			"past the end of its slice",
			g_uGPUPAdditiveInstances, Flux_ParticleGPUImpl::s_uPartitionCapacity);
		return false;
	}
	if (g_uGPUPAdditiveInstances > g_uGPUPRingOccupancy)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[GPUParticles] %u instances from %u spawned slots -- a particle may emit at most ONE instance, "
			"so the atomic is being double-counted",
			g_uGPUPAdditiveInstances, g_uGPUPRingOccupancy);
		return false;
	}
	if (g_uGPUPAlphaInstances != 0)
	{
		// Combat registers no alpha-blended GPU emitter. A non-zero count here means
		// the blend partitions are crossed -- the additive candles would be drawn by
		// the alpha pipeline as well as their own.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[GPUParticles] the alpha partition counted %u instances with no alpha GPU emitter registered "
			"-- the two blend partitions are crossed",
			g_uGPUPAlphaInstances);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xGPUParticlesTest = {
	"Combat_GPUParticles_Test",
	&Setup_GPUParticles,
	&Step_GPUParticles,
	&Verify_GPUParticles,
	/*maxFrames*/ 1400,
	/*requiresGraphics*/ true,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xGPUParticlesTest);

// ============================================================================
// Combat_SimPad_Test
// ----------------------------------------------------------------------------
// The GAMEPAD column of the C2 binding table, end to end on the real input
// path: no key, no mouse, no C++ shortcut. A simulated pad drives MOVE (left
// stick), LIGHT_ATTACK (X) and DODGE (B) through Combat_Bindings and the
// action layer, and the game reacts exactly as it does to a human's pad.
//
// It also proves the AUTO PROFILE SWITCH, which is the reason the pad column
// works at all: the game boots into P_DESKTOP, and the first stick deflection
// past the deadzone is what makes P_GAMEPAD active (after which the keyboard
// and mouse rows are dead and only the pad answers).
//
// ★ AND IT PINS THE PAD-B COLLISION, DELIBERATELY. The C2 table binds pad B to
// BOTH DODGE and RETURN_TO_MENU. RETURN_TO_MENU is consumed by
// Combat_GameFlow.bgraph at component order 60 -- BEFORE the player component
// fires "PlayerTick" at order 101 -- so pressing B in the arena unloads the
// arena on the same frame the dodge would have begun. The DODGE press edge is
// real and is asserted on the action layer (which is scene-independent), and
// the return-to-menu that ate it is asserted too. If the table is ever revised
// to give the two actions separate pad buttons, this test reds and the change
// gets read rather than silently absorbed.
//
// requiresGraphics is FALSE: nothing here reads a pixel, so CI sees it.
// ============================================================================

namespace
{
	enum class PadPhase
	{
		Boot, WaitPlaying, Move, AttackWait, AttackDown, AttackUp, AttackObserve,
		DodgeSettle, DodgeDown, DodgeObserve, AwaitMenu, Done
	};

	PadPhase              g_ePad = PadPhase::Boot;
	int                   g_iPadFrame = 0;
	int                   g_iPadAttackRetries = 0;
	Zenith_Maths::Vector3 g_xPadStartPos(0.0f);
	float                 g_fPadDisplacement = 0.0f;
	bool                  g_bPadProfileSwitched = false;
	bool                  g_bPadSawForwardInput = false;
	bool                  g_bPadSawLightAttack = false;
	bool                  g_bPadPressedDodgeButton = false;
	bool                  g_bPadSawDodgeEdge = false;
	bool                  g_bPadSawReturnToMenu = false;
	bool                  g_bPadDone = false;

	bool PadPlayerCanAct()
	{
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer == nullptr)
		{
			return false;
		}
		const Combat_PlayerState eState = pxPlayer->GetController().GetState();
		return eState == Combat_PlayerState::IDLE || eState == Combat_PlayerState::WALKING;
	}
}

static void Setup_SimPad()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	ForgetMovementKeys();
	g_ePad = PadPhase::Boot;
	g_iPadFrame = 0;
	g_iPadAttackRetries = 0;
	g_xPadStartPos = Zenith_Maths::Vector3(0.0f);
	g_fPadDisplacement = 0.0f;
	g_bPadProfileSwitched = false;
	g_bPadSawForwardInput = false;
	g_bPadSawLightAttack = false;
	g_bPadPressedDodgeButton = false;
	g_bPadSawDodgeEdge = false;
	g_bPadSawReturnToMenu = false;
	g_bPadDone = false;
	// The harness wiped every simulated device before Setup ran, so the pad has
	// to announce itself again: activity detection skips a disconnected pad.
	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);
}

static bool Step_SimPad(int iFrame)
{
	switch (g_ePad)
	{
	case PadPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_ePad = PadPhase::WaitPlaying;
		return true;

	case PadPhase::WaitPlaying:
		if (WaitForPlaying())
		{
			GetEntityPos(Combat_GameComponent::GetPlayerEntityID(), g_xPadStartPos);
			// Left stick fully FORWARD. GLFW reports a forward push as -1 and
			// the MOVE row inverts y, so this is +y = forward = +z in world.
			// The level persists until it is re-published, so once is enough.
			Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 0.0f, -1.0f);
			g_iPadFrame = 0;
			g_ePad = PadPhase::Move;
			return true;
		}
		return iFrame < 600;

	case PadPhase::Move:
	{
		if (g_xEngine.Actions().GetActiveProfile() == Combat_Bindings::uPROFILE_GAMEPAD)
		{
			g_bPadProfileSwitched = true;
		}
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer != nullptr && pxPlayer->GetController().GetMoveDirection().z > 0.5f)
		{
			g_bPadSawForwardInput = true;
		}
		Zenith_Maths::Vector3 xNow;
		if (GetEntityPos(Combat_GameComponent::GetPlayerEntityID(), xNow))
		{
			const float fDx = xNow.x - g_xPadStartPos.x;
			const float fDz = xNow.z - g_xPadStartPos.z;
			const float fMoved = std::sqrt(fDx * fDx + fDz * fDz);
			if (fMoved > g_fPadDisplacement)
			{
				g_fPadDisplacement = fMoved;
			}
		}
		if (++g_iPadFrame >= 90)
		{
			// Stick back to centre so the player stops before the attack phase.
			Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 0.0f, 0.0f);
			g_iPadFrame = 0;
			g_ePad = PadPhase::AttackWait;
		}
		return iFrame < 800;
	}

	case PadPhase::AttackWait:
		// An enemy can put the player in hit-stun; wait for a state that accepts
		// an attack rather than pressing into a window that cannot take it.
		if (PadPlayerCanAct())
		{
			g_ePad = PadPhase::AttackDown;
			return true;
		}
		if (++g_iPadFrame > 180) { g_bPadDone = true; g_ePad = PadPhase::Done; return false; }
		return iFrame < 1000;

	case PadPhase::AttackDown:
		Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_X);
		g_ePad = PadPhase::AttackUp;
		return true;

	case PadPhase::AttackUp:
		Zenith_InputSimulator::SimulateGamepadButtonUp(ZENITH_GAMEPAD_BUTTON_X);
		g_iPadFrame = 0;
		g_ePad = PadPhase::AttackObserve;
		return true;

	case PadPhase::AttackObserve:
	{
		Combat_PlayerComponent* pxPlayer = GetPlayerComponent();
		if (pxPlayer != nullptr &&
			pxPlayer->GetController().GetState() == Combat_PlayerState::LIGHT_ATTACK_1)
		{
			g_bPadSawLightAttack = true;
			g_iPadFrame = 0;
			g_ePad = PadPhase::DodgeSettle;
			return true;
		}
		if (++g_iPadFrame > 60)
		{
			// The swing can be eaten by a hit landing on the same frame; retry
			// from a clean state a couple of times before calling it broken.
			if (++g_iPadAttackRetries > 2) { g_bPadDone = true; g_ePad = PadPhase::Done; return false; }
			g_iPadFrame = 0;
			g_ePad = PadPhase::AttackWait;
		}
		return iFrame < 1200;
	}

	case PadPhase::DodgeSettle:
		// Let the swing land, then press B regardless of the player's state.
		// ★ Waiting for a dodge-capable state here would be waiting for
		// something that cannot help: the controller is not the consumer that
		// gets to see this press (RETURN_TO_MENU takes the arena down first, at
		// graph order 60), and three enemies swinging can keep the player out of
		// IDLE / WALKING for seconds. What is being proved is the pad ROW, and
		// that is player-state-independent.
		if (++g_iPadFrame < 15)
		{
			return true;
		}
		g_ePad = PadPhase::DodgeDown;
		return true;

	case PadPhase::DodgeDown:
		Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_B);
		g_bPadPressedDodgeButton = true;
		g_iPadFrame = 0;
		g_ePad = PadPhase::DodgeObserve;
		return true;

	case PadPhase::DodgeObserve:
		// ★ C1b -- this reads the edge injected on the PREVIOUS frame. A Step
		// runs at PumpAutomatedTest, before this frame's step 7/8, so what it
		// sees on the action layer is exactly the state closed at 10e last
		// frame. Reading it in the injecting Step would always be false.
		if (g_xEngine.Actions().WasPressedThisFrame(Combat_Bindings::COMBAT_ACTION_DODGE))
		{
			g_bPadSawDodgeEdge = true;
		}
		Zenith_InputSimulator::SimulateGamepadButtonUp(ZENITH_GAMEPAD_BUTTON_B);
		g_iPadFrame = 0;
		g_ePad = PadPhase::AwaitMenu;
		return true;

	case PadPhase::AwaitMenu:
		// The other half of the same button: RETURN_TO_MENU ran at graph order
		// 60 and took the arena down before the dodge could start.
		if (Combat_GameComponent::GetGameState() == Combat_GameState::MAIN_MENU)
		{
			g_bPadSawReturnToMenu = true;
			g_bPadDone = true;
			g_ePad = PadPhase::Done;
			return false;
		}
		if (++g_iPadFrame > 120) { g_bPadDone = true; g_ePad = PadPhase::Done; return false; }
		return iFrame < 1400;

	case PadPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_SimPad()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bPadDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[SimPad] never completed (phase %d)", static_cast<int>(g_ePad));
		return false;
	}
	if (!g_bPadProfileSwitched)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[SimPad] the stick never switched the active profile to P_GAMEPAD (now %u) -- pad rows resolve "
			"only while P_GAMEPAD is active, so nothing below would mean anything",
			static_cast<u_int32>(g_xEngine.Actions().GetActiveProfile()));
		return false;
	}
	if (!g_bPadSawForwardInput)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[SimPad] the left stick never reached the controller as +z movement -- the MOVE row's stick "
			"binding or its y inversion regressed");
		return false;
	}
	if (g_fPadDisplacement < 0.5f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[SimPad] the player moved %.2f m on a full forward stick over 90 frames", g_fPadDisplacement);
		return false;
	}
	if (!g_bPadSawLightAttack)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[SimPad] pad X did not start a light attack");
		return false;
	}
	if (!g_bPadSawDodgeEdge)
	{
		// The two failure modes are different bugs: never pressing means an
		// earlier phase timed out, pressing without an edge means the binding
		// or the frame contract moved.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[SimPad] no DODGE press edge (B pressed: %s, phase reached %d) -- if B WAS pressed, the pad row "
			"or the 10e close regressed; if not, an earlier phase timed out",
			g_bPadPressedDodgeButton ? "yes" : "no", static_cast<int>(g_ePad));
		return false;
	}
	if (!g_bPadSawReturnToMenu)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[SimPad] pad B raised DODGE but did NOT return to the menu -- if the C2 table was revised to "
			"stop sharing B between DODGE and RETURN_TO_MENU, delete this check and assert the dodge STATE "
			"instead (the controller can finally see it)");
		return false;
	}
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[SimPad] pad drove MOVE (%.2f m), LIGHT_ATTACK and the DODGE edge; B also returned to the menu "
		"(the shared-button collision)", g_fPadDisplacement);
	return true;
}

static const Zenith_AutomatedTest g_xSimPadTest = {
	"Combat_SimPad_Test",
	&Setup_SimPad,
	&Step_SimPad,
	&Verify_SimPad,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xSimPadTest);

#endif // ZENITH_INPUT_SIMULATOR
