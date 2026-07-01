#pragma once
#include "Core/Zenith_Engine.h"

#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_BehaviorTree.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"

#include "RenderTest/Components/RenderTest_TennisDecision.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"

#include <cstdint>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// The autonomous-tennis "brain" (order 135). Builds + SOLELY owns the behaviour
// tree, configures its agent's sight, exposes the decided shot + arm guard to the
// referee, and refreshes the pure TennisPlayerState each frame.
//
// Ownership (D1): Zenith_AIAgentComponent::SetBehaviorTree is a NON-owning borrow
// (its dtor never deletes the tree). The brain therefore owns the heap tree: it
// `new`s it, hands the raw pointer to the agent, keeps it in m_pxTree for delete +
// move-transfer, and nulls the agent's borrow before freeing.
//
// Referee<->brain signalling rides the blackboard (D8): the referee publishes
// Phase / BallEpoch / points / role / entities into this agent's blackboard each
// OnLateUpdate; the BT leaves + this component read them. That one-directional
// flow (referee -> blackboard -> leaves) is what keeps the brain/BT/referee
// headers acyclic — the brain never includes the referee.

// Tree built by RenderTest_BuildTennisTree (defined in RenderTest_TennisBTNodes.h,
// included by RenderTest.cpp). Forward-declared so the brain header stays free of
// the BT-leaf header (which itself includes THIS header to reach the brain).
Zenith_BehaviorTree* RenderTest_BuildTennisTree();

// Blackboard keys shared by the referee (writer) and the BT leaves (readers).
namespace RenderTest_TennisBB
{
	inline constexpr const char* k_szPhase          = "Phase";
	inline constexpr const char* k_szBallEpoch      = "BallEpoch";
	inline constexpr const char* k_szIsServer       = "IsServer";
	inline constexpr const char* k_szServeFromDeuce = "ServeFromDeuce";
	inline constexpr const char* k_szIsSecondServe  = "IsSecondServe";
	inline constexpr const char* k_szMyPoints       = "MyPoints";
	inline constexpr const char* k_szOppPoints      = "OppPoints";
	inline constexpr const char* k_szIsMyBall       = "IsMyBall";
	inline constexpr const char* k_szBallEntity     = "BallEntity";
	inline constexpr const char* k_szOppEntity      = "OppEntity";
	inline constexpr const char* k_szMySide         = "MySide";
	inline constexpr const char* k_szBallSpin       = "BallSpin";   // ball angular velocity (for intercept prediction)
	inline constexpr const char* k_szServeBallParked = "ServeBallParked";   // SERVING && serve not yet struck
}

class RenderTest_TennisAgentComponent
{
public:
	explicit RenderTest_TennisAgentComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	}

	RenderTest_TennisAgentComponent(const RenderTest_TennisAgentComponent&) = delete;
	RenderTest_TennisAgentComponent& operator=(const RenderTest_TennisAgentComponent&) = delete;

	// Move ctor: transfer the heap tree + null the source (its dtor becomes a
	// no-op). All other members are POD/by-value.
	RenderTest_TennisAgentComponent(RenderTest_TennisAgentComponent&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_xOpponentID(xOther.m_xOpponentID)
		, m_xBallID(xOther.m_xBallID)
		, m_pxTree(xOther.m_pxTree)
		, m_xDecidedShot(xOther.m_xDecidedShot)
		, m_xState(xOther.m_xState)
		, m_xRng(xOther.m_xRng)
		, m_uEpoch(xOther.m_uEpoch)
		, m_eSide(xOther.m_eSide)
		, m_bStarted(xOther.m_bStarted)
	{
		xOther.m_pxTree = nullptr;
	}

	// Move assign: release our OWN current tree first (never leak when assigning
	// into a live component), then take + null the source.
	RenderTest_TennisAgentComponent& operator=(RenderTest_TennisAgentComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			// Null THIS (old) entity's AIAgent behaviour-tree borrow BEFORE freeing the
			// tree it points at — assigning into a live brain would otherwise leave its
			// former entity's AIAgent dereferencing a freed tree.
			ClearTreeBorrow();
			DeleteTree();
			m_xParentEntity = xOther.m_xParentEntity;
			m_xOpponentID   = xOther.m_xOpponentID;
			m_xBallID       = xOther.m_xBallID;
			m_pxTree        = xOther.m_pxTree;
			m_xDecidedShot  = xOther.m_xDecidedShot;
			m_xState        = xOther.m_xState;
			m_xRng          = xOther.m_xRng;
			m_uEpoch        = xOther.m_uEpoch;
			m_eSide         = xOther.m_eSide;
			m_bStarted      = xOther.m_bStarted;
			xOther.m_pxTree = nullptr;
		}
		return *this;
	}

	~RenderTest_TennisAgentComponent()
	{
		// No-op after OnDestroy (which nulls m_pxTree). On the pure pool-teardown
		// path OnDestroy may not have run; delete iff non-null. Never reach back
		// into the AIAgent here — its component may already be gone.
		DeleteTree();
	}

	// ---- Lifecycle -------------------------------------------------------
	void OnStart()
	{
		// Read our side from the sibling body (it derived it from the v2 stream).
		if (RenderTest_TennisPlayerComponent* pxTennisPlayer = m_xParentEntity.TryGetComponent<RenderTest_TennisPlayerComponent>())
		{
			m_eSide = pxTennisPlayer->IsNearSide()
				? RenderTest_Tennis::TENNIS_SIDE_NEAR : RenderTest_Tennis::TENNIS_SIDE_FAR;
		}

		// Resolve the shared ball + the opponent NPC by name.
		if (Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_xParentEntity.GetEntityID()))
		{
			m_xBallID = pxSceneData->FindEntityByName("Tennis_Ball").GetEntityID();
			const char* szOpp = (m_eSide == RenderTest_Tennis::TENNIS_SIDE_NEAR) ? "Tennis_NPC_Far" : "Tennis_NPC_Near";
			m_xOpponentID = pxSceneData->FindEntityByName(szOpp).GetEntityID();
		}

		// Deterministic per-side RNG seed.
		m_xRng = RenderTest_Tennis::TennisRng(
			k_uDecisionSeed ^ (static_cast<uint32_t>(m_eSide) * 0x9E3779B9u));

		// Build + own the tree (always), then wire it onto the sibling agent when
		// present. Building unconditionally keeps ownership self-contained: a brain
		// with no agent still owns + frees its tree (exercised by the move tests).
		if (m_pxTree == nullptr)
			m_pxTree = RenderTest_BuildTennisTree();
		if (Zenith_AIAgentComponent* pxAgent = ResolveAgent())
		{
			pxAgent->SetBehaviorTree(m_pxTree);          // non-owning borrow
			pxAgent->SetUpdateInterval(0.08f);
			ConfigureSight(pxAgent->GetEntity().GetEntityID());
			SeedBlackboard(pxAgent->GetBlackboard());
		}
		m_bStarted = true;
	}

	void OnUpdate(float /*fDt*/)
	{
		RefreshPlayerState();
	}

	void OnDestroy()
	{
		// Null the agent's borrow BEFORE freeing so a later AIAgent abort/dtor can't
		// dereference a freed tree (pool dtor order across components is unspecified).
		ClearTreeBorrow();
		DeleteTree();   // delete + null; the dtor is then a no-op
	}

	// ---- Referee-facing (called from the referee's OnLateUpdate) ----------
	// Synchronous per-ball reset: clear the arm guard + stale decision, adopt the
	// new epoch. Because the referee runs in OnLateUpdate, both brains are reset
	// before the next frame's AIAgent tick — the BT can never act on a stale guard.
	void ResetForNewBall(u_int uEpoch)
	{
		m_uEpoch = uEpoch;
		m_xDecidedShot = RenderTest_Tennis::TennisShotDecision();   // armed=false, epoch=0
		if (Zenith_AIAgentComponent* pxAgent = ResolveAgent())
			pxAgent->GetBlackboard().SetInt(RenderTest_TennisBB::k_szBallEpoch, static_cast<int32_t>(uEpoch));
	}

	// The referee consumes the decided shot only when it is armed AND stamped with
	// the epoch it asks for (stale/unarmed -> rejected).
	bool TryGetDecidedShot(u_int uEpoch, RenderTest_Tennis::TennisShotDecision& xOut) const
	{
		if (m_xDecidedShot.m_bArmed && m_xDecidedShot.m_uEpoch == uEpoch)
		{
			xOut = m_xDecidedShot;
			return true;
		}
		return false;
	}

	const RenderTest_Tennis::TennisPlayerState& GetPlayerState() const { return m_xState; }

	// ---- BT-leaf-facing ---------------------------------------------------
	// Store a freshly-decided shot UNARMED (the decide leaves); arming is a
	// separate step gated on a confirmed stroke start.
	void SetDecidedShot(const RenderTest_Tennis::TennisShotDecision& xShot)
	{
		m_xDecidedShot = xShot;
		m_xDecidedShot.m_bArmed = false;
		m_xDecidedShot.m_uEpoch = 0u;
	}
	// Arm the stored decision, stamping it with the current ball epoch. Called by
	// the arm leaves ONLY after RequestSwing/RequestServe returned true.
	void ArmDecidedShot(u_int uEpoch)
	{
		m_xDecidedShot.m_bArmed = true;
		m_xDecidedShot.m_uEpoch = uEpoch;
	}
	const RenderTest_Tennis::TennisShotDecision& GetDecidedShot() const { return m_xDecidedShot; }
	bool IsArmed() const { return m_xDecidedShot.m_bArmed; }

	RenderTest_Tennis::TennisRng& Rng() { return m_xRng; }
	RenderTest_Tennis::TennisSide GetSide() const { return m_eSide; }
	Zenith_EntityID GetBallID() const { return m_xBallID; }
	Zenith_EntityID GetOpponentID() const { return m_xOpponentID; }
	Zenith_BehaviorTree* GetTree() const { return m_pxTree; }
	bool IsStarted() const { return m_bStarted; }

	// ---- Serialization (version-only; side/RNG/entities re-derived in OnStart) -
	void WriteToDataStream(Zenith_DataStream& xStream) const { const u_int uV = 1; xStream << uV; }
	void ReadFromDataStream(Zenith_DataStream& xStream) { u_int uV = 0; xStream >> uV; }

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Side: %s", m_eSide == RenderTest_Tennis::TENNIS_SIDE_NEAR ? "Near" : "Far");
		ImGui::Text("Epoch: %u  Armed: %s", m_uEpoch, m_xDecidedShot.m_bArmed ? "yes" : "no");
		ImGui::Text("Aim: %.1f %.1f %.1f", m_xDecidedShot.m_xAim.x, m_xDecidedShot.m_xAim.y, m_xDecidedShot.m_xAim.z);
		ImGui::Text("Balance: %.2f", m_xState.m_fBalance);
	}
#endif

private:
	static constexpr uint32_t k_uDecisionSeed = 0x1234567u;

	void DeleteTree()
	{
		if (m_pxTree != nullptr)
		{
			delete m_pxTree;   // Zenith_BehaviorTree dtor deletes its root subtree
			m_pxTree = nullptr;
		}
	}

	// Null the (current) entity's AIAgent behaviour-tree borrow so nobody
	// dereferences a tree we are about to free (used by OnDestroy AND move-assign).
	void ClearTreeBorrow()
	{
		if (Zenith_AIAgentComponent* pxAgent = ResolveAgent())
			pxAgent->SetBehaviorTree(nullptr);
	}

	Zenith_AIAgentComponent* ResolveAgent() const
	{
		if (Zenith_AIAgentComponent* pxAgent = m_xParentEntity.TryGetComponent<Zenith_AIAgentComponent>())
			return pxAgent;
		return nullptr;
	}

	void ConfigureSight(Zenith_EntityID xSelf) const
	{
		Zenith_SightConfig xCfg;
		xCfg.m_fMaxRange = 40.0f;
		// A generous forward cone. The far player faces -Z and the near player +Z; the
		// incoming ball always arrives from the net side (in front of the player), so a
		// forward cone contains it. (This was briefly set to 360 to work around an engine
		// bug where the perception forward was mis-derived via glm::eulerAngles().y — a
		// -Z facing decoded to a +Z forward, blinding the far player. That is now fixed at
		// the source in Zenith_PerceptionSystem, so a realistic cone works on both sides.)
		xCfg.m_fFOVAngle = 200.0f;
		xCfg.m_fPeripheralAngle = 260.0f;
		xCfg.m_bRequireLineOfSight = false;   // open court; net/racket would falsely occlude
		xCfg.m_fAwarenessGainRate = 8.0f;
		xCfg.m_fAwarenessDecayRate = 1.0f;
		Zenith_PerceptionSystem::SetSightConfig(xSelf, xCfg);
	}

	// Seed the blackboard so the FIRST BT tick (before the referee's first
	// OnLateUpdate) has valid entity handles + a safe WARMUP phase.
	void SeedBlackboard(Zenith_Blackboard& xBB) const
	{
		xBB.SetInt(RenderTest_TennisBB::k_szPhase, static_cast<int32_t>(RenderTest_Tennis::POINT_PHASE_WARMUP));
		xBB.SetInt(RenderTest_TennisBB::k_szBallEpoch, 0);
		xBB.SetInt(RenderTest_TennisBB::k_szMySide, static_cast<int32_t>(m_eSide));
		xBB.SetBool(RenderTest_TennisBB::k_szIsServer, false);
		xBB.SetBool(RenderTest_TennisBB::k_szServeBallParked, false);
		xBB.SetBool(RenderTest_TennisBB::k_szServeFromDeuce, true);
		xBB.SetBool(RenderTest_TennisBB::k_szIsSecondServe, false);
		xBB.SetBool(RenderTest_TennisBB::k_szIsMyBall, false);
		xBB.SetInt(RenderTest_TennisBB::k_szMyPoints, 0);
		xBB.SetInt(RenderTest_TennisBB::k_szOppPoints, 0);
		xBB.SetEntityID(RenderTest_TennisBB::k_szBallEntity, m_xBallID);
		xBB.SetEntityID(RenderTest_TennisBB::k_szOppEntity, m_xOpponentID);
	}

	void RefreshPlayerState()
	{
		const RenderTest_Tennis::TennisCourt xCourt = RenderTest_Tennis::DefaultCourt();
		m_xState.m_eMySide = m_eSide;
		m_xState.m_xMyPos = EntityPos(m_xParentEntity.GetEntityID());
		m_xState.m_xOppPos = EntityPos(m_xOpponentID);

		// Balance = how laterally centred I am (in control vs stretched wide). Use the
		// SAME Z as my current position so being deep behind the baseline (normal in a
		// baseline rally) does NOT read as "stretched" — only lateral displacement does.
		// (Measuring full distance-from-baseline-centre made every deep rally shot look
		// defensive, so the AI only ever sliced.)
		const Zenith_Maths::Vector3 xReady(xCourt.m_fCenterX, xCourt.m_fSurfaceY, m_xState.m_xMyPos.z);
		m_xState.m_fBalance = RenderTest_Tennis::ComputeBalance(m_xState.m_xMyPos, xReady, xCourt.m_fSinglesHalfWidth);

		if (Zenith_AIAgentComponent* pxAgent = ResolveAgent())
		{
			const Zenith_Blackboard& xBB = pxAgent->GetBlackboard();
			m_xState.m_iMyPoints  = xBB.GetInt(RenderTest_TennisBB::k_szMyPoints, 0);
			m_xState.m_iOppPoints = xBB.GetInt(RenderTest_TennisBB::k_szOppPoints, 0);
			m_xState.m_bIsServer  = xBB.GetBool(RenderTest_TennisBB::k_szIsServer, false);
		}
	}

	Zenith_Maths::Vector3 EntityPos(Zenith_EntityID xID) const
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		if (xID == INVALID_ENTITY_ID)
			return xPos;
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xID);
		if (Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>())
			pxTransform->GetPosition(xPos);
		return xPos;
	}

	Zenith_Entity m_xParentEntity;
	Zenith_EntityID m_xOpponentID = INVALID_ENTITY_ID;
	Zenith_EntityID m_xBallID = INVALID_ENTITY_ID;
	Zenith_BehaviorTree* m_pxTree = nullptr;   // SOLE owner

	RenderTest_Tennis::TennisShotDecision m_xDecidedShot;
	RenderTest_Tennis::TennisPlayerState  m_xState;
	RenderTest_Tennis::TennisRng          m_xRng;
	u_int m_uEpoch = 0u;
	RenderTest_Tennis::TennisSide m_eSide = RenderTest_Tennis::TENNIS_SIDE_NEAR;
	bool m_bStarted = false;
};
