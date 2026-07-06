#pragma once
#include "Core/Zenith_Engine.h"

#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"

#include "RenderTest/Components/RenderTest_TennisDecision.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"

#include <cstdint>
#include <cstring>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// The autonomous-tennis "brain" (order 135) - SYSTEMS SHIM since the W3
// behaviour-graph conversion. The decision body (the old behaviour tree's
// Selector over serve / rally / recover) lives in
// RenderTest_TennisBrain.bgraph on this entity's Zenith_GraphComponent,
// driven by the graph's own ON_UPDATE tick chain (an authored accumulator
// reproducing the retired AIAgent 0.08 s interval semantics exactly:
// accumulate-while-enabled, fire at >= 0.08, reset-to-zero). The shim keeps:
// the per-side deterministic TennisRng, the decided-shot storage + arm guard
// the referee consumes, the TennisPlayerState refresh, and the sight config.
// The old Zenith_BehaviorTree + RenderTest_TennisBTNodes.h are DELETED.
//
// Referee<->brain signalling rides the GRAPH blackboard (previously the
// AIAgent blackboard): the referee publishes Phase / BallEpoch / points /
// role / entities into this entity's graph blackboard each OnLateUpdate; the
// graph's gate/verb nodes and RefreshPlayerState read them. That
// one-directional flow (referee -> blackboard -> graph) keeps the headers
// acyclic - the brain never includes the referee.

// Blackboard keys shared by the referee (writer) and the graph (readers).
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
	static constexpr const char* kszGraphAsset = "game:Graphs/RenderTest_TennisBrain.bgraph";

	explicit RenderTest_TennisAgentComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	}

	RenderTest_TennisAgentComponent(const RenderTest_TennisAgentComponent&) = delete;
	RenderTest_TennisAgentComponent& operator=(const RenderTest_TennisAgentComponent&) = delete;

	// All members are POD/by-value since the heap behaviour tree left with the
	// graph conversion - default-style moves suffice (pools relocate instances).
	RenderTest_TennisAgentComponent(RenderTest_TennisAgentComponent&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_xOpponentID(xOther.m_xOpponentID)
		, m_xBallID(xOther.m_xBallID)
		, m_xDecidedShot(xOther.m_xDecidedShot)
		, m_xState(xOther.m_xState)
		, m_xRng(xOther.m_xRng)
		, m_uEpoch(xOther.m_uEpoch)
		, m_eSide(xOther.m_eSide)
		, m_bStarted(xOther.m_bStarted)
	{
	}

	RenderTest_TennisAgentComponent& operator=(RenderTest_TennisAgentComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity = xOther.m_xParentEntity;
			m_xOpponentID   = xOther.m_xOpponentID;
			m_xBallID       = xOther.m_xBallID;
			m_xDecidedShot  = xOther.m_xDecidedShot;
			m_xState        = xOther.m_xState;
			m_xRng          = xOther.m_xRng;
			m_uEpoch        = xOther.m_uEpoch;
			m_eSide         = xOther.m_eSide;
			m_bStarted      = xOther.m_bStarted;
		}
		return *this;
	}

	// ---- Graph blackboard plumbing (public: the referee's publish and the
	// ---- unit tests read/write the decision inputs through these) ---------
	Zenith_BehaviourGraph* FindTennisGraph() const
	{
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphs == nullptr) return nullptr;
		for (u_int u = 0; u < pxGraphs->GetGraphCount(); ++u)
		{
			if (std::strcmp(pxGraphs->GetGraphAssetPathAt(u), kszGraphAsset) == 0)
			{
				return pxGraphs->GetGraphAt(u);
			}
		}
		return nullptr;
	}
	void WriteBBInt(const char* szVar, int32_t iValue)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindTennisGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetInt32(iValue);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}
	void WriteBBBool(const char* szVar, bool bValue)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindTennisGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetBool(bValue);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}
	void WriteBBVector3(const char* szVar, const Zenith_Maths::Vector3& xValue3)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindTennisGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetVector3(xValue3);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}
	// Written ONLY for valid ids (parity with the retired AIAgent-blackboard
	// publish, which skipped invalid handles): an unpublished key reads back
	// as the INVALID sentinel default at the node side.
	void WriteBBEntity(const char* szVar, Zenith_EntityID xId)
	{
		if (xId == INVALID_ENTITY_ID) return;
		if (Zenith_BehaviourGraph* pxGraph = FindTennisGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetPackedEntityID(xId.GetPacked());
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
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

		// The AIAgent survives the graph conversion as perception registrar +
		// nav-agent host; its behaviour-tree borrow is simply never set.
		if (Zenith_AIAgentComponent* pxAgent = ResolveAgent())
		{
			ConfigureSight(pxAgent->GetEntity().GetEntityID());
		}
		SeedGraphBlackboard();
		m_bStarted = true;
	}

	void OnUpdate(float /*fDt*/)
	{
		RefreshPlayerState();
	}

	// ---- Referee-facing (called from the referee's OnLateUpdate) ----------
	// Synchronous per-ball reset: clear the arm guard + stale decision, adopt the
	// new epoch. Because the referee runs in OnLateUpdate, both brains are reset
	// before the next frame's graph tick - the graph can never act on a stale guard.
	void ResetForNewBall(u_int uEpoch)
	{
		m_uEpoch = uEpoch;
		m_xDecidedShot = RenderTest_Tennis::TennisShotDecision();   // armed=false, epoch=0
		WriteBBInt(RenderTest_TennisBB::k_szBallEpoch, static_cast<int32_t>(uEpoch));
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

	// ---- Graph-node-facing ------------------------------------------------
	// Store a freshly-decided shot UNARMED (the decide nodes); arming is a
	// separate step gated on a confirmed stroke start.
	void SetDecidedShot(const RenderTest_Tennis::TennisShotDecision& xShot)
	{
		m_xDecidedShot = xShot;
		m_xDecidedShot.m_bArmed = false;
		m_xDecidedShot.m_uEpoch = 0u;
	}
	// Arm the stored decision, stamping it with the current ball epoch. Called by
	// the arm nodes ONLY after RequestSwing/RequestServe returned true.
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
		// bug where the perception forward was mis-derived via glm::eulerAngles().y - a
		// -Z facing decoded to a +Z forward, blinding the far player. That is now fixed at
		// the source in Zenith_PerceptionSystem, so a realistic cone works on both sides.)
		xCfg.m_fFOVAngle = 200.0f;
		xCfg.m_fPeripheralAngle = 260.0f;
		xCfg.m_bRequireLineOfSight = false;   // open court; net/racket would falsely occlude
		xCfg.m_fAwarenessGainRate = 8.0f;
		xCfg.m_fAwarenessDecayRate = 1.0f;
		Zenith_PerceptionSystem::SetSightConfig(xSelf, xCfg);
	}

	// Seed the graph blackboard so ticks that beat the referee's first
	// OnLateUpdate publish see valid entity handles + a safe WARMUP phase
	// (declared graph-variable defaults cover the scalars; the entity handles
	// are runtime-resolved and can only be seeded here).
	void SeedGraphBlackboard()
	{
		WriteBBInt(RenderTest_TennisBB::k_szPhase, static_cast<int32_t>(RenderTest_Tennis::POINT_PHASE_WARMUP));
		WriteBBInt(RenderTest_TennisBB::k_szBallEpoch, 0);
		WriteBBInt(RenderTest_TennisBB::k_szMySide, static_cast<int32_t>(m_eSide));
		WriteBBBool(RenderTest_TennisBB::k_szIsServer, false);
		WriteBBBool(RenderTest_TennisBB::k_szServeBallParked, false);
		WriteBBBool(RenderTest_TennisBB::k_szServeFromDeuce, true);
		WriteBBBool(RenderTest_TennisBB::k_szIsSecondServe, false);
		WriteBBBool(RenderTest_TennisBB::k_szIsMyBall, false);
		WriteBBInt(RenderTest_TennisBB::k_szMyPoints, 0);
		WriteBBInt(RenderTest_TennisBB::k_szOppPoints, 0);
		WriteBBEntity(RenderTest_TennisBB::k_szBallEntity, m_xBallID);
		WriteBBEntity(RenderTest_TennisBB::k_szOppEntity, m_xOpponentID);
	}

	void RefreshPlayerState()
	{
		const RenderTest_Tennis::TennisCourt xCourt = RenderTest_Tennis::DefaultCourt();
		m_xState.m_eMySide = m_eSide;
		m_xState.m_xMyPos = EntityPos(m_xParentEntity.GetEntityID());
		m_xState.m_xOppPos = EntityPos(m_xOpponentID);

		// Balance = how laterally centred I am (in control vs stretched wide). Use the
		// SAME Z as my current position so being deep behind the baseline (normal in a
		// baseline rally) does NOT read as "stretched" - only lateral displacement does.
		// (Measuring full distance-from-baseline-centre made every deep rally shot look
		// defensive, so the AI only ever sliced.)
		const Zenith_Maths::Vector3 xReady(xCourt.m_fCenterX, xCourt.m_fSurfaceY, m_xState.m_xMyPos.z);
		m_xState.m_fBalance = RenderTest_Tennis::ComputeBalance(m_xState.m_xMyPos, xReady, xCourt.m_fSinglesHalfWidth);

		if (Zenith_BehaviourGraph* pxGraph = FindTennisGraph())
		{
			const Zenith_GraphBlackboard& xBB = pxGraph->GetBlackboard();
			m_xState.m_iMyPoints  = xBB.GetInt32(RenderTest_TennisBB::k_szMyPoints, 0);
			m_xState.m_iOppPoints = xBB.GetInt32(RenderTest_TennisBB::k_szOppPoints, 0);
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

	RenderTest_Tennis::TennisShotDecision m_xDecidedShot;
	RenderTest_Tennis::TennisPlayerState  m_xState;
	RenderTest_Tennis::TennisRng          m_xRng;
	u_int m_uEpoch = 0u;
	RenderTest_Tennis::TennisSide m_eSide = RenderTest_Tennis::TENNIS_SIDE_NEAR;
	bool m_bStarted = false;
};
