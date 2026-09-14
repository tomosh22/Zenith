#include "Zenith.h"

// ============================================================================
// Combat_Tests_GraphPinTotality -- pin-table coverage for Combat's OWN node
// library (Games/Combat/Components/Combat_GraphNodes.h, 26 blackboard-variable
// name properties across 18 node classes).
//
// WHY THIS EXISTS AS A UNIT AND NOT AS A REVIEW HABIT. A node class with a
// m_str*Var* property and no pin descriptor is OPAQUE to
// Zenith_GraphDefinitionValidator: it contributes no writer and performs no
// read as far as any check can tell. So the failure this catches is a whole
// Combat node quietly dropping OUT of validation while every other signal --
// the characterization tests, the boot, the graph report -- stays green. Worse,
// the validator's writer set is graph-wide, so ONE un-annotated writer turns
// every downstream reader into a would-be error and would red A-8's latch.
//
// The walk itself, the registry swap and the RAII restore all live once in
// Zenith/EntityComponent/Zenith_GraphPinTotality.TestHarness.inl -- read that
// header for why the registry is SWAPPED to one registrar rather than filtered
// by category, and why the restore replays the snapshot in its original order
// (a game's rows sit at 0..N-1, so an engine-first rebuild would move them).
//
// This is a ZENITH_TEST rather than an automated test because the engine gate
// runs Combat's boot units through run_unit_gate.ps1 (combat.exe). It is the
// FIRST ZENITH_TEST in Games/Combat, so it moves Combat's pinned baseline by
// exactly the number of units in this file -- and only Combat's row: a
// game-exe-only unit cannot move Zenithmon's or RenderTest's.
// ============================================================================

#ifdef ZENITH_TESTING

#include "Core/Zenith_TestFramework.h"
#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"
#include "UnitTests/Zenith_TempScene.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_Pointers.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Combat/Combat_Bindings.h"
#include "Combat/Components/Combat_GraphNodes.h"     // Combat_RegisterGraphNodes

namespace
{
	static int32_t CombatSlotInt(const Zenith_PropertyValue* pxSlot, const char* szWhat)
	{
		ZENITH_ASSERT_NOT_NULL(pxSlot, "%s output slot is UNSET", szWhat);
		if (pxSlot == nullptr)
		{
			return 0;
		}
		ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_INT32),
			"%s output slot has the wrong type", szWhat);
		return pxSlot->GetType() == PROPERTY_TYPE_INT32 ? pxSlot->GetInt32() : 0;
	}

	static bool CombatSlotBool(const Zenith_PropertyValue* pxSlot, const char* szWhat)
	{
		ZENITH_ASSERT_NOT_NULL(pxSlot, "%s output slot is UNSET", szWhat);
		if (pxSlot == nullptr)
		{
			return false;
		}
		ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_BOOL),
			"%s output slot has the wrong type", szWhat);
		return pxSlot->GetType() == PROPERTY_TYPE_BOOL && pxSlot->GetBool();
	}

	static Zenith_PropertyValue CombatFloat(float fValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetFloat(fValue);
		return xValue;
	}
}

ZENITH_TEST(GraphPinTable, CombatNodesTotality)
{
	// No exemptions: every m_str*Var* property in Combat's library names
	// exactly ONE blackboard variable (there is no comma-separated LIST
	// property here, which is the only thing the exempt list exists for).
	Zenith_CheckPinTableTotality(&Combat_RegisterGraphNodes, "Combat_GraphNodes.h", nullptr, 0u);
}

ZENITH_TEST(GraphPinTable, CombatPinIndicesMatchTables)
{
	// Keep the public addresses Execute() uses tied to the descriptor order. The
	// reflective totality walk above cannot catch a valid-but-reordered table.
	const Zenith_GraphPinTable* apxTables[] = {
		&CombatNode_QueryAttackState::GetPinTableStatic(), &CombatNode_ActivateHitbox::GetPinTableStatic(),
		&CombatNode_RegisterHits::GetPinTableStatic(), &CombatNode_NotifyComboHit::GetPinTableStatic(),
		&CombatNode_TickComboTimer::GetPinTableStatic(), &CombatNode_CountAliveEnemies::GetPinTableStatic(),
		&CombatNode_CheckPlayerDead::GetPinTableStatic(), &CombatNode_PlayerPreTick::GetPinTableStatic(),
		&CombatNode_PlayerMovementTick::GetPinTableStatic(), &CombatNode_PlayerAttackTick::GetPinTableStatic(),
		&CombatNode_PlayerDodgeTick::GetPinTableStatic(), &CombatNode_PlayerHitStunTick::GetPinTableStatic(),
		&CombatNode_EnemyPreTick::GetPinTableStatic(), &CombatNode_EnemyChaseTick::GetPinTableStatic(),
		&CombatNode_EnemyAttackTick::GetPinTableStatic(), &CombatNode_EnemyHitStunTick::GetPinTableStatic(),
		&CombatNode_EnemyPostTick::GetPinTableStatic(), &CombatNode_GetGameState::GetPinTableStatic()
	};
	u_int uDescriptors = 0u, uInputs = 0u, uOutputs = 0u;
	for (const Zenith_GraphPinTable* pxTable : apxTables)
	{
		for (u_int uPin = 0u; uPin < pxTable->GetPinCount(); ++uPin)
		{
			const Zenith_GraphPinDesc& xPin = pxTable->GetPinAt(uPin);
			++uDescriptors;
			if (xPin.m_eRole == GRAPH_PIN_ROLE_INPUT) ++uInputs;
			if (xPin.m_eRole == GRAPH_PIN_ROLE_OUTPUT) ++uOutputs;
		}
	}
	ZENITH_ASSERT_EQ(uDescriptors, 26u);
	ZENITH_ASSERT_EQ(uInputs, 14u);
	ZENITH_ASSERT_EQ(uOutputs, 12u);

#define COMBAT_CHECK_PIN(NodeType, PinName) \
	ZENITH_ASSERT_EQ(NodeType::GetPinTableStatic().FindPinIndex(#PinName), NodeType::uPIN_##PinName)
	COMBAT_CHECK_PIN(CombatNode_QueryAttackState, AttackStarted); COMBAT_CHECK_PIN(CombatNode_QueryAttackState, IsAttacking);
	COMBAT_CHECK_PIN(CombatNode_QueryAttackState, AttackType); COMBAT_CHECK_PIN(CombatNode_QueryAttackState, ComboCount); COMBAT_CHECK_PIN(CombatNode_QueryAttackState, HitFrame);
	COMBAT_CHECK_PIN(CombatNode_ActivateHitbox, AttackType); COMBAT_CHECK_PIN(CombatNode_ActivateHitbox, ComboCount);
	COMBAT_CHECK_PIN(CombatNode_RegisterHits, HitCount); COMBAT_CHECK_PIN(CombatNode_NotifyComboHit, ComboCount); COMBAT_CHECK_PIN(CombatNode_TickComboTimer, Dt);
	COMBAT_CHECK_PIN(CombatNode_CountAliveEnemies, AliveCount); COMBAT_CHECK_PIN(CombatNode_CountAliveEnemies, HasEnemies); COMBAT_CHECK_PIN(CombatNode_CheckPlayerDead, PlayerDead);
	COMBAT_CHECK_PIN(CombatNode_PlayerPreTick, Dt); COMBAT_CHECK_PIN(CombatNode_PlayerPreTick, State); COMBAT_CHECK_PIN(CombatNode_PlayerMovementTick, Dt);
	COMBAT_CHECK_PIN(CombatNode_PlayerAttackTick, Dt); COMBAT_CHECK_PIN(CombatNode_PlayerDodgeTick, Dt); COMBAT_CHECK_PIN(CombatNode_PlayerHitStunTick, Dt);
	COMBAT_CHECK_PIN(CombatNode_EnemyPreTick, Dt); COMBAT_CHECK_PIN(CombatNode_EnemyPreTick, State); COMBAT_CHECK_PIN(CombatNode_EnemyChaseTick, Dt);
	COMBAT_CHECK_PIN(CombatNode_EnemyAttackTick, Dt); COMBAT_CHECK_PIN(CombatNode_EnemyHitStunTick, Dt); COMBAT_CHECK_PIN(CombatNode_EnemyPostTick, Dt);
	COMBAT_CHECK_PIN(CombatNode_GetGameState, State);
#undef COMBAT_CHECK_PIN
}

ZENITH_TEST(GraphPinTable, CombatInputOverridesChangeComboEffects)
{
	// These nodes have no entity prerequisite. A non-zero wire override must
	// reach their static witnesses even with the legacy blackboard name blank.
	const uint32_t uOriginalCount = Combat_GameComponent::GetComboCount();
	const float fOriginalTimer = Combat_GameComponent::GetComboTimer();
	Zenith_GraphContext xContext;
	CombatNode_NotifyComboHit xNotify;
	xNotify.m_strComboCountVar.clear();
	Zenith_PropertyValue xCombo;
	xCombo.SetInt32(3);
	xNotify.SetInputForTest(CombatNode_NotifyComboHit::uPIN_ComboCount, xCombo);
	ZENITH_ASSERT_EQ(static_cast<int>(xNotify.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(Combat_GameComponent::GetComboCount(), 3u);

	CombatNode_TickComboTimer xTick;
	xTick.m_strDtVar.clear();
	Zenith_PropertyValue xDt;
	xDt.SetFloat(0.75f);
	xTick.SetInputForTest(CombatNode_TickComboTimer::uPIN_Dt, xDt);
	ZENITH_ASSERT_EQ(static_cast<int>(xTick.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ_FLOAT(Combat_GameComponent::GetComboTimer(), 1.25f, 0.0001f);
	ZENITH_ASSERT_EQ(xNotify.GetFallbackUseCountForTest(CombatNode_NotifyComboHit::uPIN_ComboCount), 0u);
	ZENITH_ASSERT_EQ(xTick.GetFallbackUseCountForTest(CombatNode_TickComboTimer::uPIN_Dt), 0u);
	ZENITH_ASSERT_EQ(xNotify.GetBadAccessWarningCountForTest(), 0u);
	ZENITH_ASSERT_EQ(xTick.GetBadAccessWarningCountForTest(), 0u);
	Combat_GameComponent::NotifyComboHit(uOriginalCount, fOriginalTimer);
}

ZENITH_TEST(GraphPinTable, CombatGetGameStatePublishesOutputSlot)
{
	const Combat_GameState eOriginalState = Combat_GameComponent::GetGameState();
	Combat_GameComponent::SetGameState(Combat_GameState::PAUSED);
	CombatNode_GetGameState xNode;
	Zenith_GraphContext xContext;
	xNode.m_strStateVar.clear();
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(CombatSlotInt(xNode.GetOutputForTest(CombatNode_GetGameState::uPIN_State), "GetGameState PAUSED"),
		static_cast<int32_t>(Combat_GameState::PAUSED));
	Combat_GameComponent::SetGameState(Combat_GameState::PLAYING);
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(CombatSlotInt(xNode.GetOutputForTest(CombatNode_GetGameState::uPIN_State), "GetGameState PLAYING"),
		static_cast<int32_t>(Combat_GameState::PLAYING));
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	Combat_GameComponent::SetGameState(eOriginalState);
}

ZENITH_TEST(GraphPinTable, CombatRuntimeNodesPublishAndConsumePins)
{
	// The fixture is a real active scene: QueryAttackState and RegisterHits use
	// the player/Enemy naming and active-scene query contracts, while PreTick
	// needs the actual Transform + Collider component pair before it may publish.
	Zenith_TempScene xScene("CombatPinRuntime");
	const Zenith_EntityID xOriginalPlayer = Combat_GameComponent::GetPlayerEntityID();
	const std::vector<Zenith_EntityID> axOriginalEnemies = Combat_GameComponent::GetEnemyEntityIDs();
	for (const Zenith_EntityID xID : axOriginalEnemies) Combat_GameComponent::UnregisterEnemy(xID);
	CombatNode_CountAliveEnemies xEmptyCount;
	xEmptyCount.m_strAliveCountVar.clear(); xEmptyCount.m_strHasEnemiesVar.clear();
	Zenith_GraphContext xEmptyCountContext;
	ZENITH_ASSERT_EQ(static_cast<int>(xEmptyCount.Execute(xEmptyCountContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(CombatSlotInt(xEmptyCount.GetOutputForTest(CombatNode_CountAliveEnemies::uPIN_AliveCount), "empty AliveCount"), 0);
	ZENITH_ASSERT_FALSE(CombatSlotBool(xEmptyCount.GetOutputForTest(CombatNode_CountAliveEnemies::uPIN_HasEnemies), "empty HasEnemies"));
	ZENITH_ASSERT_EQ(xEmptyCount.GetBadAccessWarningCountForTest(), 0u);
	Zenith_Entity xPlayer = xScene.CreateEntity("PlayerPinRuntime");
	Zenith_ColliderComponent& xPlayerCollider = xPlayer.AddComponent<Zenith_ColliderComponent>();
	Zenith_AnimatorComponent& xPlayerAnimator = xPlayer.AddComponent<Zenith_AnimatorComponent>();
	Combat_PlayerComponent& xPlayerComponent = xPlayer.AddComponent<Combat_PlayerComponent>();
	xPlayerAnimator.OnStart();
	xPlayerComponent.OnAwake();
	xPlayerComponent.OnStart();
	Combat_DamageSystem::RegisterEntity(xPlayer.GetEntityID(), 100.0f);

	Zenith_Entity xEnemy = xScene.CreateEntity("EnemyPinRuntime");
	xEnemy.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 1.0f, 1.0f));
	Combat_DamageSystem::RegisterEntity(xEnemy.GetEntityID(), 50.0f);
	Combat_GameComponent::RegisterEnemy(xEnemy.GetEntityID());

	Zenith_GraphContext xContext;
	xContext.m_xSelf = xPlayer;

	// QueryAttackState has no test-only controller mutator. Drive its public
	// production input path through one complete Input -> Actions frame, then
	// the public player graph shims. Registrations already exist before units.
	Zenith_InputActions& xActions = g_xEngine.Actions();
	const bool bProfileWasOverridden = xActions.IsProfileOverridden();
	const u_int8 uSavedProfile = xActions.GetActiveProfile();
	g_xEngine.Input().ResetTransientForTest();
	g_xEngine.Pointers().ResetTransientForTest();
	xActions.ResetTransientForTest();
	xActions.SetProfileOverride(Combat_Bindings::uPROFILE_DESKTOP);
	g_xEngine.Input().DrainPendingPlatformEvents();
	g_xEngine.Pointers().BeginFrame(1.0f);
	Zenith_InputEvent xHeavyPress;
	xHeavyPress.m_eType = INPUT_EVENT_MOUSE_PRESS;
	xHeavyPress.m_iCode = ZENITH_MOUSE_BUTTON_RIGHT;
	g_xEngine.Input().AppendInjectedEvent(xHeavyPress);
	xActions.UpdateProfile();
	xActions.FinalizeReservedUI();
	xActions.FinalizeGameplay();
	ZENITH_ASSERT_TRUE(Combat_Bindings::ReadHeavyAttackPressed(), "the public action frame did not publish HeavyAttack");
	const bool bPlayerReady = xPlayerComponent.Graph_PreTick(0.0f);
	ZENITH_ASSERT_TRUE(bPlayerReady, "player fixture lost its Transform/Collider before the input-path witness");
	if (bPlayerReady)
	{
		xPlayerComponent.Graph_MovementTick(0.0f);
		ZENITH_ASSERT_TRUE(xPlayerAnimator.HasStateMachine(), "PlayerComponent::OnStart did not initialise the public combat state machine");
		if (xPlayerAnimator.HasStateMachine())
		{
			auto& xStateMachine = xPlayerAnimator.GetStateMachine();
			auto* pxAttackState = xStateMachine.GetState(CombatAnimStates::ATTACK1);
			ZENITH_ASSERT_NOT_NULL(pxAttackState);
			if (pxAttackState != nullptr)
			{
				auto* pxBlendTree = pxAttackState->GetBlendTree();
				ZENITH_ASSERT_NOT_NULL(pxBlendTree);
				if (pxBlendTree != nullptr)
				{
					xStateMachine.SetState(CombatAnimStates::ATTACK1);
					pxBlendTree->SetNormalizedTime(0.5f);
					const Zenith_AnimatorStateInfo xInfo = xPlayerAnimator.GetCurrentAnimatorStateInfo();
					ZENITH_ASSERT_TRUE(xInfo.IsName(CombatAnimStates::ATTACK1));
					ZENITH_ASSERT_TRUE(xInfo.m_fNormalizedTime >= 0.3f && xInfo.m_fNormalizedTime <= 0.7f);
					ZENITH_ASSERT_TRUE(xPlayerComponent.AnimController().IsAttackHitFrame());
				}
			}
		}
	}

	CombatNode_QueryAttackState xQuery;
	xQuery.m_strAttackStartedVar.clear(); xQuery.m_strIsAttackingVar.clear(); xQuery.m_strAttackTypeVar.clear();
	xQuery.m_strComboCountVar.clear(); xQuery.m_strHitFrameVar.clear();
	ZENITH_ASSERT_EQ(static_cast<int>(xQuery.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_TRUE(CombatSlotBool(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_AttackStarted), "QueryAttackState.AttackStarted"));
	ZENITH_ASSERT_TRUE(CombatSlotBool(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_IsAttacking), "QueryAttackState.IsAttacking"));
	ZENITH_ASSERT_EQ(CombatSlotInt(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_AttackType), "QueryAttackState.AttackType"), static_cast<int32_t>(Combat_AttackType::HEAVY));
	ZENITH_ASSERT_EQ(CombatSlotInt(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_ComboCount), "QueryAttackState.ComboCount"), 0);
	ZENITH_ASSERT_TRUE(CombatSlotBool(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_HitFrame), "QueryAttackState.HitFrame"));
	ZENITH_ASSERT_EQ(xQuery.GetBadAccessWarningCountForTest(), 0u);

	xPlayerComponent.HitDetection().SetOwner(xPlayer.GetEntityID());
	CombatNode_ActivateHitbox xActivate;
	xActivate.m_strAttackTypeVar.clear(); xActivate.m_strComboCountVar.clear();
	Zenith_PropertyValue xHeavyType; xHeavyType.SetInt32(static_cast<int32_t>(Combat_AttackType::HEAVY));
	Zenith_PropertyValue xComboCount; xComboCount.SetInt32(3);
	xActivate.SetInputForTest(CombatNode_ActivateHitbox::uPIN_AttackType, xHeavyType);
	xActivate.SetInputForTest(CombatNode_ActivateHitbox::uPIN_ComboCount, xComboCount);
	ZENITH_ASSERT_EQ(static_cast<int>(xActivate.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(xActivate.GetFallbackUseCountForTest(CombatNode_ActivateHitbox::uPIN_AttackType), 0u);
	ZENITH_ASSERT_EQ(xActivate.GetFallbackUseCountForTest(CombatNode_ActivateHitbox::uPIN_ComboCount), 0u);
	ZENITH_ASSERT_EQ(xActivate.GetBadAccessWarningCountForTest(), 0u);
	bool bCapturedDamage = false;
	Combat_DamageEvent xCapturedDamage{};
	Zenith_Subscription xDamageCapture = Zenith_EventDispatcher::Get().SubscribeScoped<Combat_DamageEvent>(
		[&](const Combat_DamageEvent& xEvent) { bCapturedDamage = true; xCapturedDamage = xEvent; });
	CombatNode_RegisterHits xHits;
	xHits.m_strHitCountVar.clear();
	ZENITH_ASSERT_EQ(static_cast<int>(xHits.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(CombatSlotInt(xHits.GetOutputForTest(CombatNode_RegisterHits::uPIN_HitCount), "RegisterHits.HitCount"), 1);
	ZENITH_ASSERT_EQ(xHits.GetBadAccessWarningCountForTest(), 0u);
	ZENITH_ASSERT_TRUE(bCapturedDamage, "ActivateHitbox-driven RegisterHits emitted no damage event");
	if (bCapturedDamage)
	{
		ZENITH_ASSERT_EQ_FLOAT(xCapturedDamage.m_fDamage, 25.0f, 0.0001f);
		ZENITH_ASSERT_EQ(xCapturedDamage.m_uComboCount, 3u);
		ZENITH_ASSERT_TRUE(xCapturedDamage.m_bIsComboHit);
	}

	CombatNode_CountAliveEnemies xCount;
	xCount.m_strAliveCountVar.clear(); xCount.m_strHasEnemiesVar.clear();
	ZENITH_ASSERT_EQ(static_cast<int>(xCount.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_TRUE(CombatSlotInt(xCount.GetOutputForTest(CombatNode_CountAliveEnemies::uPIN_AliveCount), "CountAliveEnemies.AliveCount") >= 1);
	ZENITH_ASSERT_TRUE(CombatSlotBool(xCount.GetOutputForTest(CombatNode_CountAliveEnemies::uPIN_HasEnemies), "CountAliveEnemies.HasEnemies"));
	ZENITH_ASSERT_EQ(xCount.GetBadAccessWarningCountForTest(), 0u);

	const Zenith_EntityID xSavedPlayer = Combat_GameComponent::GetPlayerEntityID();
	CombatNode_CheckPlayerDead xDead;
	xDead.m_strPlayerDeadVar.clear();
	ZENITH_ASSERT_EQ(static_cast<int>(xDead.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_FALSE(CombatSlotBool(xDead.GetOutputForTest(CombatNode_CheckPlayerDead::uPIN_PlayerDead), "CheckPlayerDead.PlayerDead alive"));
	ZENITH_ASSERT_EQ(xDead.GetBadAccessWarningCountForTest(), 0u);
	Combat_GameComponent::RegisterPlayer(xEnemy.GetEntityID()); // registered, but deliberately untracked by damage: dead
	Combat_DamageSystem::UnregisterEntity(xEnemy.GetEntityID());
	ZENITH_ASSERT_EQ(static_cast<int>(xCount.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(CombatSlotInt(xCount.GetOutputForTest(CombatNode_CountAliveEnemies::uPIN_AliveCount), "registered-dead AliveCount"), 0);
	ZENITH_ASSERT_TRUE(CombatSlotBool(xCount.GetOutputForTest(CombatNode_CountAliveEnemies::uPIN_HasEnemies), "registered-dead HasEnemies"));
	xDead.m_strPlayerDeadVar.clear();
	ZENITH_ASSERT_EQ(static_cast<int>(xDead.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_TRUE(CombatSlotBool(xDead.GetOutputForTest(CombatNode_CheckPlayerDead::uPIN_PlayerDead), "CheckPlayerDead.PlayerDead"));
	Combat_GameComponent::RegisterPlayer(xSavedPlayer);
	Combat_DamageSystem::RegisterEntity(xEnemy.GetEntityID(), 50.0f);

	// Dt is consumed before Graph_PreTick's dependent work; HIT_STUN makes the
	// State output visibly non-zero, so a stamped default cannot satisfy this row.
	xPlayerComponent.TriggerHitStun(1.0f);
	CombatNode_PlayerPreTick xPlayerPre;
	xPlayerPre.m_strDtVar.clear(); xPlayerPre.m_strStateVar.clear();
	xPlayerPre.SetInputForTest(CombatNode_PlayerPreTick::uPIN_Dt, CombatFloat(0.25f));
	ZENITH_ASSERT_EQ(static_cast<int>(xPlayerPre.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(CombatSlotInt(xPlayerPre.GetOutputForTest(CombatNode_PlayerPreTick::uPIN_State), "PlayerPreTick.State"), static_cast<int32_t>(Combat_PlayerState::HIT_STUN));
	ZENITH_ASSERT_EQ(xPlayerPre.GetFallbackUseCountForTest(CombatNode_PlayerPreTick::uPIN_Dt), 0u);
	ZENITH_ASSERT_EQ(xPlayerPre.GetBadAccessWarningCountForTest(), 0u);

	Zenith_Entity xBrainEnemy = xScene.CreateEntity("EnemyBrainPinRuntime");
	// Graph_PreTick changes an unregistered enemy to DEAD. Register health first
	// so TriggerHitStun remains the exact state this row observes.
	Combat_DamageSystem::RegisterEntity(xBrainEnemy.GetEntityID(), 50.0f);
	Combat_EnemyComponent& xBrainComponent = xBrainEnemy.AddComponent<Combat_EnemyComponent>();
	xBrainComponent.OnAwake();
	xBrainComponent.TriggerHitStun();
	Zenith_GraphContext xEnemyContext;
	xEnemyContext.m_xSelf = xBrainEnemy;
	CombatNode_EnemyPreTick xEnemyPre;
	xEnemyPre.m_strDtVar.clear(); xEnemyPre.m_strStateVar.clear();
	xEnemyPre.SetInputForTest(CombatNode_EnemyPreTick::uPIN_Dt, CombatFloat(0.125f));
	ZENITH_ASSERT_EQ(static_cast<int>(xEnemyPre.Execute(xEnemyContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(CombatSlotInt(xEnemyPre.GetOutputForTest(CombatNode_EnemyPreTick::uPIN_State), "EnemyPreTick.State"), static_cast<int32_t>(Combat_EnemyState::HIT_STUN));
	ZENITH_ASSERT_EQ(xEnemyPre.GetFallbackUseCountForTest(CombatNode_EnemyPreTick::uPIN_Dt), 0u);
	ZENITH_ASSERT_EQ(xEnemyPre.GetBadAccessWarningCountForTest(), 0u);
	// Both PreTick nodes read Dt before their component's dependent guard. A
	// later failure must retain the meaningful state from the successful fire.
	xPlayer.RemoveComponent<Zenith_ColliderComponent>();
	ZENITH_ASSERT_EQ(static_cast<int>(xPlayerPre.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(CombatSlotInt(xPlayerPre.GetOutputForTest(CombatNode_PlayerPreTick::uPIN_State), "retained PlayerPreTick.State"), static_cast<int32_t>(Combat_PlayerState::HIT_STUN));
	xBrainEnemy.RemoveComponent<Zenith_TransformComponent>();
	ZENITH_ASSERT_EQ(static_cast<int>(xEnemyPre.Execute(xEnemyContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(CombatSlotInt(xEnemyPre.GetOutputForTest(CombatNode_EnemyPreTick::uPIN_State), "retained EnemyPreTick.State"), static_cast<int32_t>(Combat_EnemyState::HIT_STUN));
	xBrainComponent.OnDestroy();
	Combat_DamageSystem::UnregisterEntity(xBrainEnemy.GetEntityID());

	// RegisterHits has already published its real count. Removing the dependent
	// transform makes the next call fail before publication; a FAILURE must not
	// erase the last successful output slot.
	xPlayer.RemoveComponent<Zenith_TransformComponent>();
	ZENITH_ASSERT_EQ(static_cast<int>(xHits.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(CombatSlotInt(xHits.GetOutputForTest(CombatNode_RegisterHits::uPIN_HitCount), "retained RegisterHits.HitCount"), 1);
	ZENITH_ASSERT_EQ(static_cast<int>(xQuery.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_TRUE(CombatSlotBool(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_AttackStarted), "retained QueryAttackState.AttackStarted"));
	ZENITH_ASSERT_TRUE(CombatSlotBool(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_IsAttacking), "retained QueryAttackState.IsAttacking"));
	ZENITH_ASSERT_EQ(CombatSlotInt(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_AttackType), "retained QueryAttackState.AttackType"), static_cast<int32_t>(Combat_AttackType::HEAVY));
	ZENITH_ASSERT_EQ(CombatSlotInt(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_ComboCount), "retained QueryAttackState.ComboCount"), 0);
	ZENITH_ASSERT_TRUE(CombatSlotBool(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_HitFrame), "retained QueryAttackState.HitFrame"));

	Combat_GameComponent::UnregisterEnemy(xEnemy.GetEntityID());
	Combat_DamageSystem::UnregisterEntity(xEnemy.GetEntityID());
	Combat_DamageSystem::UnregisterEntity(xPlayer.GetEntityID());
	xPlayerComponent.OnDestroy();
	Combat_GameComponent::RegisterPlayer(xOriginalPlayer);
	for (const Zenith_EntityID xID : axOriginalEnemies) Combat_GameComponent::RegisterEnemy(xID);
	g_xEngine.Input().ResetTransientForTest();
	g_xEngine.Pointers().ResetTransientForTest();
	xActions.ResetTransientForTest();
	if (bProfileWasOverridden)
	{
		xActions.SetProfileOverride(uSavedProfile);
	}
	else
	{
		xActions.ClearOverride();
	}
	(void)xPlayerCollider; // Presence, rather than a physics body, is PreTick's contract.
}

ZENITH_TEST(GraphPinTable, CombatGuardFailuresPreservePinSlotContracts)
{
	// QueryAttackState guards before any pin accessor. A direct, unbuilt failure
	// must therefore leave its OUTPUT slots UNSET rather than publishing zero.
	CombatNode_QueryAttackState xQuery;
	Zenith_GraphContext xEmptyContext;
	ZENITH_ASSERT_EQ(static_cast<int>(xQuery.Execute(xEmptyContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_NULL(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_AttackStarted));
	ZENITH_ASSERT_EQ(xQuery.GetBadAccessWarningCountForTest(), 0u);
	ZENITH_ASSERT_EQ(xQuery.GetFallbackUseCountForTest(CombatNode_QueryAttackState::uPIN_AttackStarted), 0u);

	// PlayerPreTick reads Dt after resolving the player but before the missing
	// collider makes Graph_PreTick fail. That accessor initializes typed output
	// storage, so State is a present INT32 zero on this fresh failed run.
	Zenith_TempScene xScene("CombatPreTickGuard");
	const Zenith_EntityID xOriginalPlayer = Combat_GameComponent::GetPlayerEntityID();
	Zenith_Entity xPlayer = xScene.CreateEntity("PlayerPreTickGuard");
	Combat_PlayerComponent& xPlayerComponent = xPlayer.AddComponent<Combat_PlayerComponent>();
	xPlayerComponent.OnAwake();
	CombatNode_PlayerPreTick xPreTick;
	xPreTick.m_strDtVar.clear(); xPreTick.m_strStateVar.clear();
	xPreTick.SetInputForTest(CombatNode_PlayerPreTick::uPIN_Dt, CombatFloat(0.5f));
	Zenith_GraphContext xContext;
	xContext.m_xSelf = xPlayer;
	ZENITH_ASSERT_EQ(static_cast<int>(xPreTick.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(CombatSlotInt(xPreTick.GetOutputForTest(CombatNode_PlayerPreTick::uPIN_State), "failed PlayerPreTick.State"), 0);
	ZENITH_ASSERT_EQ(xPreTick.GetFallbackUseCountForTest(CombatNode_PlayerPreTick::uPIN_Dt), 0u);
	ZENITH_ASSERT_EQ(xPreTick.GetBadAccessWarningCountForTest(), 0u);
	xPlayerComponent.OnDestroy();
	Combat_GameComponent::RegisterPlayer(xOriginalPlayer);
}

ZENITH_TEST(GraphPinTable, CombatInitialisedGuardFailuresStampTypedZero)
{
	// A bare direct node that returns before its first accessor remains UNSET.
	// A real graph instance builds each statically typed OUTPUT slot up-front,
	// so the identical guard failure exposes a present typed zero instead.
	const struct
	{
		const char* m_szType;
		u_int m_uOutput;
		Zenith_PropertyType m_eType;
		bool m_bQuery;
	} axCases[] =
	{
		{ "CombatQueryAttackState", CombatNode_QueryAttackState::uPIN_AttackStarted, PROPERTY_TYPE_BOOL, true },
		{ "CombatRegisterHits", CombatNode_RegisterHits::uPIN_HitCount, PROPERTY_TYPE_INT32, false },
		{ "CombatEnemyPreTick", CombatNode_EnemyPreTick::uPIN_State, PROPERTY_TYPE_INT32, false },
	};
	for (const auto& xCase : axCases)
	{
		Zenith_GraphDefinition xDefinition;
		const u_int uNode = xDefinition.AddNode(xCase.m_szType);
		ZENITH_ASSERT_NE(uNode, 0u, "%s was not registered for the real-graph fixture", xCase.m_szType);
		if (uNode == 0u) continue;
		Zenith_BehaviourGraph xGraph;
		const bool bInitialised = xGraph.InitialiseFromDefinition(xDefinition);
		ZENITH_ASSERT_TRUE(bInitialised);
		if (!bInitialised) continue;
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
		Zenith_GraphNode* pxNode = xGraph.FindNode(uNode);
		ZENITH_ASSERT_NOT_NULL(pxNode);
		if (pxNode == nullptr) continue;
		Zenith_GraphContext xContext;
		xContext.m_pxGraph = &xGraph;
		xContext.m_pxBlackboard = &xGraph.GetBlackboard();
		ZENITH_ASSERT_EQ(static_cast<int>(pxNode->Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		const Zenith_PropertyValue* pxOutput = pxNode->GetOutputForTest(xCase.m_uOutput);
		ZENITH_ASSERT_NOT_NULL(pxOutput, "%s's initialized typed output should be SET at its guard failure", xCase.m_szType);
		if (pxOutput != nullptr)
		{
			const Zenith_PropertyType eActualType = pxOutput->GetType();
			ZENITH_ASSERT_EQ(static_cast<int>(eActualType), static_cast<int>(xCase.m_eType));
			if (eActualType == PROPERTY_TYPE_BOOL) ZENITH_ASSERT_FALSE(pxOutput->GetBool());
			else if (eActualType == PROPERTY_TYPE_INT32) ZENITH_ASSERT_EQ(pxOutput->GetInt32(), 0);
		}
		if (xCase.m_bQuery)
		{
			const struct { u_int m_uPin; Zenith_PropertyType m_eType; } axRemainingQueryOutputs[] =
			{
				{ CombatNode_QueryAttackState::uPIN_IsAttacking, PROPERTY_TYPE_BOOL },
				{ CombatNode_QueryAttackState::uPIN_AttackType, PROPERTY_TYPE_INT32 },
				{ CombatNode_QueryAttackState::uPIN_ComboCount, PROPERTY_TYPE_INT32 },
				{ CombatNode_QueryAttackState::uPIN_HitFrame, PROPERTY_TYPE_BOOL },
			};
			for (const auto& xOutputCase : axRemainingQueryOutputs)
			{
				const Zenith_PropertyValue* pxQueryOutput = pxNode->GetOutputForTest(xOutputCase.m_uPin);
				ZENITH_ASSERT_NOT_NULL(pxQueryOutput);
				if (pxQueryOutput == nullptr) continue;
				const Zenith_PropertyType eActualType = pxQueryOutput->GetType();
				ZENITH_ASSERT_EQ(static_cast<int>(eActualType), static_cast<int>(xOutputCase.m_eType));
				if (eActualType == PROPERTY_TYPE_BOOL) ZENITH_ASSERT_FALSE(pxQueryOutput->GetBool());
				else if (eActualType == PROPERTY_TYPE_INT32) ZENITH_ASSERT_EQ(pxQueryOutput->GetInt32(), 0);
			}
		}
		ZENITH_ASSERT_EQ(pxNode->GetBadAccessWarningCountForTest(), 0u);
	}
}

ZENITH_TEST(GraphPinTable, CombatGuardReadUsesPermanentDefaultAfterPrerequisites)
{
	// One fresh unbound node: no self guard leaves State UNSET; a valid self then
	// reaches GetInput before the missing-collider guard and stamps typed zero.
	CombatNode_PlayerPreTick xPreTick;
	xPreTick.m_strDtVar.clear();
	Zenith_GraphContext xEmptyContext;
	ZENITH_ASSERT_EQ(static_cast<int>(xPreTick.Execute(xEmptyContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_NULL(xPreTick.GetOutputForTest(CombatNode_PlayerPreTick::uPIN_State));
	ZENITH_ASSERT_EQ(xPreTick.GetFallbackUseCountForTest(CombatNode_PlayerPreTick::uPIN_Dt), 0u);
	ZENITH_ASSERT_EQ(xPreTick.GetBadAccessWarningCountForTest(), 0u);

	Zenith_TempScene xScene("CombatGuardReadFallback");
	const Zenith_EntityID xOriginalPlayer = Combat_GameComponent::GetPlayerEntityID();
	Zenith_Entity xPlayer = xScene.CreateEntity("PlayerGuardReadFallback");
	Combat_PlayerComponent& xPlayerComponent = xPlayer.AddComponent<Combat_PlayerComponent>();
	xPlayerComponent.OnAwake();
	Zenith_GraphBlackboard xBlackboard;
	Zenith_GraphContext xContext;
	xContext.m_xSelf = xPlayer;
	xContext.m_pxBlackboard = &xBlackboard;
	ZENITH_ASSERT_EQ(static_cast<int>(xPreTick.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(CombatSlotInt(xPreTick.GetOutputForTest(CombatNode_PlayerPreTick::uPIN_State),
		"missing-collider PlayerPreTick.State"), 0);
	ZENITH_ASSERT_EQ(xPreTick.GetFallbackUseCountForTest(CombatNode_PlayerPreTick::uPIN_Dt), 0u);
	ZENITH_ASSERT_EQ(xPreTick.GetBadAccessWarningCountForTest(), 0u);
	xPlayerComponent.OnDestroy();
	Combat_GameComponent::RegisterPlayer(xOriginalPlayer);
}

ZENITH_TEST(GraphPinTable, CombatEnemyPreTickDtControlsCooldownGate)
{
	// Production shims establish the exact Idle -> Chasing -> Attacking ->
	// cooldown sequence. The node's two distinct Dt overrides must decide when
	// Chase may start the next attack, and the following PreTick snapshots it.
	Zenith_TempScene xScene("CombatEnemyPreTickDt");
	const Zenith_EntityID xOriginalPlayer = Combat_GameComponent::GetPlayerEntityID();
	Zenith_Entity xPlayer = xScene.CreateEntity("PlayerEnemyPreTickDt");
	xPlayer.AddComponent<Zenith_ColliderComponent>();
	Combat_PlayerComponent& xPlayerComponent = xPlayer.AddComponent<Combat_PlayerComponent>();
	xPlayerComponent.OnAwake();
	Combat_DamageSystem::RegisterEntity(xPlayer.GetEntityID(), 100.0f);
	Zenith_Entity xEnemy = xScene.CreateEntity("EnemyPreTickDt");
	xEnemy.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	Combat_DamageSystem::RegisterEntity(xEnemy.GetEntityID(), 50.0f);
	Combat_EnemyComponent& xEnemyComponent = xEnemy.AddComponent<Combat_EnemyComponent>();
	xEnemyComponent.OnAwake();
	const bool bEnemyReady = xEnemyComponent.Graph_PreTick(0.0f);
	ZENITH_ASSERT_TRUE(bEnemyReady);
	if (bEnemyReady)
	{
		xEnemyComponent.Graph_IdleTick();
		ZENITH_ASSERT_EQ(xEnemyComponent.Graph_GetStateInt(), static_cast<int>(Combat_EnemyState::CHASING));
		xEnemyComponent.Graph_ChaseTick(0.0f);
		ZENITH_ASSERT_EQ(xEnemyComponent.Graph_GetStateInt(), static_cast<int>(Combat_EnemyState::ATTACKING));
		xEnemyComponent.Graph_AttackTick(0.41f); // finishes and arms the exact 1.5 s cooldown
		ZENITH_ASSERT_EQ(xEnemyComponent.Graph_GetStateInt(), static_cast<int>(Combat_EnemyState::CHASING));
	}
	CombatNode_EnemyPreTick xPreTick;
	xPreTick.m_strDtVar.clear(); xPreTick.m_strStateVar.clear();
	Zenith_GraphContext xContext; xContext.m_xSelf = xEnemy;
	xPreTick.SetInputForTest(CombatNode_EnemyPreTick::uPIN_Dt, CombatFloat(0.0f));
	const bool bZeroDtPreTick = xPreTick.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS;
	ZENITH_ASSERT_TRUE(bZeroDtPreTick);
	if (bZeroDtPreTick)
	{
		ZENITH_ASSERT_EQ(CombatSlotInt(xPreTick.GetOutputForTest(CombatNode_EnemyPreTick::uPIN_State), "zero-Dt EnemyPreTick.State"), static_cast<int32_t>(Combat_EnemyState::CHASING));
		xEnemyComponent.Graph_ChaseTick(0.0f);
		ZENITH_ASSERT_EQ(xEnemyComponent.Graph_GetStateInt(), static_cast<int>(Combat_EnemyState::CHASING));
		xPreTick.SetInputForTest(CombatNode_EnemyPreTick::uPIN_Dt, CombatFloat(1.6f));
		const bool bExpiredCooldown = xPreTick.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS;
		ZENITH_ASSERT_TRUE(bExpiredCooldown);
		if (bExpiredCooldown)
		{
			xEnemyComponent.Graph_ChaseTick(0.0f);
			ZENITH_ASSERT_EQ(xEnemyComponent.Graph_GetStateInt(), static_cast<int>(Combat_EnemyState::ATTACKING));
			xPreTick.SetInputForTest(CombatNode_EnemyPreTick::uPIN_Dt, CombatFloat(0.0f));
			const bool bAttackSnapshot = xPreTick.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS;
			ZENITH_ASSERT_TRUE(bAttackSnapshot);
			if (bAttackSnapshot)
			{
				ZENITH_ASSERT_EQ(CombatSlotInt(xPreTick.GetOutputForTest(CombatNode_EnemyPreTick::uPIN_State), "attacking EnemyPreTick.State"), static_cast<int32_t>(Combat_EnemyState::ATTACKING));
			}
		}
	}
	ZENITH_ASSERT_EQ(xPreTick.GetFallbackUseCountForTest(CombatNode_EnemyPreTick::uPIN_Dt), 0u);
	ZENITH_ASSERT_EQ(xPreTick.GetBadAccessWarningCountForTest(), 0u);
	xEnemyComponent.OnDestroy();
	Combat_DamageSystem::UnregisterEntity(xEnemy.GetEntityID());
	Combat_DamageSystem::UnregisterEntity(xPlayer.GetEntityID());
	xPlayerComponent.OnDestroy();
	Combat_GameComponent::RegisterPlayer(xOriginalPlayer);
}

ZENITH_TEST(GraphPinTable, CombatPlayerPreTickDtExpiresComboWindow)
{
	// The dt pin is live only through UpdateTimers: a light swing ends, leaves
	// its combo window open, then the two distinct overrides preserve/expire it.
	Zenith_TempScene xScene("CombatPreTickDt");
	const Zenith_EntityID xOriginalPlayer = Combat_GameComponent::GetPlayerEntityID();
	Zenith_Entity xPlayer = xScene.CreateEntity("PlayerPreTickDt");
	xPlayer.AddComponent<Zenith_ColliderComponent>();
	Combat_PlayerComponent& xPlayerComponent = xPlayer.AddComponent<Combat_PlayerComponent>();
	xPlayerComponent.OnAwake();
	Zenith_InputActions& xActions = g_xEngine.Actions();
	const bool bOldOverride = xActions.IsProfileOverridden();
	const u_int8 uOldProfile = xActions.GetActiveProfile();
	g_xEngine.Input().ResetTransientForTest(); g_xEngine.Pointers().ResetTransientForTest(); xActions.ResetTransientForTest();
	xActions.SetProfileOverride(Combat_Bindings::uPROFILE_DESKTOP);
	g_xEngine.Input().DrainPendingPlatformEvents(); g_xEngine.Pointers().BeginFrame(1.0f);
	Zenith_InputEvent xPress; xPress.m_eType = INPUT_EVENT_MOUSE_PRESS; xPress.m_iCode = ZENITH_MOUSE_BUTTON_LEFT;
	g_xEngine.Input().AppendInjectedEvent(xPress); xActions.UpdateProfile(); xActions.FinalizeReservedUI(); xActions.FinalizeGameplay();
	const bool bPlayerReady = xPlayerComponent.Graph_PreTick(0.0f);
	ZENITH_ASSERT_TRUE(bPlayerReady);
	if (bPlayerReady)
	{
		xPlayerComponent.Graph_MovementTick(0.0f);
		ZENITH_ASSERT_EQ(static_cast<int>(xPlayerComponent.GetController().GetState()), static_cast<int>(Combat_PlayerState::LIGHT_ATTACK_1));
		Zenith_GraphContext xContext; xContext.m_xSelf = xPlayer;
		CombatNode_QueryAttackState xQuery;
		xQuery.m_strAttackStartedVar.clear(); xQuery.m_strIsAttackingVar.clear(); xQuery.m_strAttackTypeVar.clear();
		xQuery.m_strComboCountVar.clear(); xQuery.m_strHitFrameVar.clear();
		ZENITH_ASSERT_EQ(static_cast<int>(xQuery.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(CombatSlotInt(xQuery.GetOutputForTest(CombatNode_QueryAttackState::uPIN_ComboCount), "light QueryAttackState.ComboCount"), 1);
		xPlayerComponent.Graph_AttackTick(0.31f);
		CombatNode_PlayerPreTick xNode; xNode.m_strDtVar.clear(); xNode.m_strStateVar.clear();
		xNode.SetInputForTest(CombatNode_PlayerPreTick::uPIN_Dt, CombatFloat(0.0f));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(CombatSlotInt(xNode.GetOutputForTest(CombatNode_PlayerPreTick::uPIN_State), "zero Dt state"), static_cast<int32_t>(Combat_PlayerState::LIGHT_ATTACK_1));
		xNode.SetInputForTest(CombatNode_PlayerPreTick::uPIN_Dt, CombatFloat(0.6f));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(CombatSlotInt(xNode.GetOutputForTest(CombatNode_PlayerPreTick::uPIN_State), "expiry Dt state"), static_cast<int32_t>(Combat_PlayerState::IDLE));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(CombatNode_PlayerPreTick::uPIN_Dt), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
	xPlayerComponent.OnDestroy(); Combat_GameComponent::RegisterPlayer(xOriginalPlayer);
	g_xEngine.Input().ResetTransientForTest(); g_xEngine.Pointers().ResetTransientForTest(); xActions.ResetTransientForTest();
	if (bOldOverride) xActions.SetProfileOverride(uOldProfile); else xActions.ClearOverride();
}

#endif // ZENITH_TESTING
