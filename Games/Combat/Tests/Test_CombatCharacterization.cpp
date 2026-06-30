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
#include "Components/Combat_GameComponent.h"
#include "Components/Combat_PlayerComponent.h"
#include "Components/Combat_DamageSystem.h"

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

	void ReleaseMovementKeys()
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
	}

	bool WaitForPlaying()
	{
		return Combat_GameComponent::IsInPlayingState()
			&& Combat_GameComponent::GetPlayerEntityID().IsValid()
			&& Combat_GameComponent::GetEnemyEntityIDs().size() == 3;
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
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, fDz > 0.15f);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, fDz < -0.15f);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, fDx > 0.15f);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, fDx < -0.15f);
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

#endif // ZENITH_INPUT_SIMULATOR
