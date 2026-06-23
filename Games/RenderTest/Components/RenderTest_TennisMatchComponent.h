#pragma once
#include "Core/Zenith_Engine.h"

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Physics/Zenith_Physics.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "UI/Zenith_UICanvas.h"
#include "Zenith_OS_Include.h"

#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include "RenderTest/RenderTest_Tennis.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"
#include "RenderTest/Components/RenderTest_TennisAgentComponent.h"
#include "RenderTest/Components/RenderTest_TennisTelemetry.h"
#include "RenderTest/Components/RenderTest_GameplayState.h"

#include "Telemetry/Zenith_Telemetry.h"

#include <cmath>
#include <string>
#include <algorithm>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// Tennis REFEREE / scorekeeper (singleton on the "Tennis_Match" entity, order 130).
//
// The match is no longer scripted: the two NPCs are autonomous AI agents (AIAgent
// + behaviour tree + brain). This component is a PASSIVE referee — it owns the
// ball physics layer (Magnus + spin-aware bounce override), resolves points from
// real physics events (contact / bounce / net-crossing), keeps score, and parks
// the agents between points. All per-frame work runs in OnLateUpdate, AFTER every
// entity's OnUpdate, so the referee sees each NPC's post-update state (the body's
// anim-contact flag set @120 and the brain's decided shot armed @135) the same
// frame (cross-entity OnUpdate vs OnLateUpdate ordering — see the plan's C-EXEC).
//
// Referee -> brain/leaf signalling rides each NPC's blackboard (published here);
// brain -> referee is direct (TryGetDecidedShot / ResetForNewBall). That keeps the
// headers acyclic (the referee includes the brain; the brain never includes the
// referee).
class RenderTest_TennisMatchComponent
{
public:
	explicit RenderTest_TennisMatchComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	}

	RenderTest_TennisMatchComponent(const RenderTest_TennisMatchComponent&) = delete;
	RenderTest_TennisMatchComponent& operator=(const RenderTest_TennisMatchComponent&) = delete;

	// Move semantics: transfer the heap navmesh + nav agents, null the source. The
	// nav agents live on the heap so their addresses (handed to the AIAgents via
	// SetNavMeshAgent) survive a referee relocation. (There is only one referee in
	// the scene, but the pool still move-constructs and the tests force it.)
	RenderTest_TennisMatchComponent(RenderTest_TennisMatchComponent&& xOther) noexcept
	{
		MoveFrom(xOther);
	}
	RenderTest_TennisMatchComponent& operator=(RenderTest_TennisMatchComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			// Null THIS referee's (old) NPCs' nav-agent borrows BEFORE freeing the
			// agents they point at — assigning into a live referee would otherwise
			// leave its former NPCs dereferencing freed nav agents.
			ClearNavBorrows();
			DeleteNav();
			MoveFrom(xOther);
		}
		return *this;
	}

	~RenderTest_TennisMatchComponent()
	{
		DeleteNav();   // null-guarded; no-op after OnDestroy
	}

	// ---- Lifecycle -------------------------------------------------------
	void OnStart()
	{
		m_xCourt = RenderTest_Tennis::DefaultCourt();

		if (Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_xParentEntity.GetEntityID()))
		{
			m_xBall   = pxSceneData->FindEntityByName("Tennis_Ball");
			m_xNpc[0] = pxSceneData->FindEntityByName("Tennis_NPC_Near");
			m_xNpc[1] = pxSceneData->FindEntityByName("Tennis_NPC_Far");
		}

		// Defensive: the referee's OnStart must run AFTER both NPCs' OnStart (the
		// authoring keeps Tennis_Match last). Log if an NPC is missing its AI parts
		// — a reorder would silently break the autonomous match.
		for (int i = 0; i < 2; ++i)
		{
			if (!m_xNpc[i].IsValid() || !m_xNpc[i].HasComponent<Zenith_AIAgentComponent>()
				|| !m_xNpc[i].HasComponent<RenderTest_TennisAgentComponent>())
			{
				Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
					"[Tennis] NPC %d missing AIAgent/brain at referee OnStart — autonomous match disabled for it", i);
			}
		}

		// Ball physics: near-zero Jolt restitution/friction so the referee's
		// BounceVelocity owns the bounce (the model restitution is applied in
		// BounceVelocity, not by Jolt).
		Zenith_PhysicsBodyID xBody;
		if (TryBallBody(xBody))
		{
			g_xEngine.Physics().SetRestitution(xBody, k_fJoltRestitution);
			g_xEngine.Physics().SetFriction(xBody, k_fJoltFriction);
		}

		BuildNavMeshAndAgents();
		WirePerceptionTargets();

		m_xJitterRng = RenderTest_Tennis::TennisRng(k_uJitterSeed);

		BeginTelemetry();   // before ResetMatch, so its MatchStart event is recorded

		ResetMatch();

		// IK-showcase overrides the live match (see UpdateShowcase).
		if (RenderTest_GameplayState::s_bTennisIkShowcase)
		{
			m_ePhase = RenderTest_Tennis::POINT_PHASE_SHOWCASE;
			const char* aszStroke[3] = { "serve", "forehand", "backhand" };
			Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] IK SHOWCASE: %s",
				aszStroke[glm::clamp(RenderTest_GameplayState::s_iTennisShowcaseStroke, 0, 2)]);
		}
	}

	// All per-frame referee work runs here (after every entity's OnUpdate).
	void OnLateUpdate(float fDt)
	{
		// Telemetry frame tick FIRST so events fired this frame stamp the new index.
		if (m_bTelemetryActive)
			Zenith_Telemetry::GetRecorder().NextFrame();

		// Perception is game-driven; the referee is its single ticker.
		Zenith_PerceptionSystem::Update(fDt);

		ApplyPhaseAgentState();

		// Per-agent nav path-failure detection (the agents' nav ran in their OnUpdate
		// this frame): hand a stuck agent to footwork for the rest of the ball.
		if (m_ePhase == RenderTest_Tennis::POINT_PHASE_SERVING || m_ePhase == RenderTest_Tennis::POINT_PHASE_LIVE)
			CheckNavFallback();

		// Hold the un-struck serve parked at its independent toss point above the server
		// (see ServeTossPos) — the racket must reach UP to it, so the proximity gate
		// stays meaningful (it is NOT glued to the racket).
		if (m_ePhase == RenderTest_Tennis::POINT_PHASE_SERVING && m_iLastHitter < 0)
			TossBallAboveServer();

		const bool bBallLive =
			(m_ePhase == RenderTest_Tennis::POINT_PHASE_LIVE)
			|| (m_ePhase == RenderTest_Tennis::POINT_PHASE_SERVING && m_iLastHitter >= 0);

		if (bBallLive)
		{
			ApplySpinAndMagnus(fDt);
			LatchNetCrossing();
			DetectAndHandleBounce();
			SettleCheck();   // a ball that has died on a side = not returned -> resolve cleanly
		}

		HandleContacts();
		TickWatchdogs(fDt);
		TickPhaseTimers(fDt);

		// Re-apply agent enable/park AFTER any mid-frame phase transition (a bounce/
		// contact/timeout this frame), so a just-disabled agent doesn't coast one
		// extra frame on its last nav velocity before the next frame parks it.
		ApplyPhaseAgentState();

		// Stash this frame's sample as next frame's pre-impact vector.
		Zenith_PhysicsBodyID xBody;
		if (bBallLive && TryBallBody(xBody))
		{
			m_xPrevBallPos = BallPos();
			m_xPrevBallVel = g_xEngine.Physics().GetLinearVelocity(xBody);
			m_bPrevSampleValid = true;
		}
		else
		{
			m_bPrevSampleValid = false;
		}

		PublishBlackboards();

		if (m_ePhase == RenderTest_Tennis::POINT_PHASE_SHOWCASE)
			UpdateShowcase(fDt);

		// Telemetry per-frame sample (gated to the recorder's sample period = 10 Hz) +
		// a periodic disk snapshot (~every 600 frames) so a windowed run that is killed
		// rather than exited cleanly still leaves a complete, valid file on disk.
		if (m_bTelemetryActive)
		{
			if (Zenith_Telemetry::GetRecorder().ShouldSampleThisFrame())
				SampleTelemetry();
			if ((Zenith_Telemetry::GetRecorder().GetFrameIdx() % 600u) == 0u)
				FlushTelemetrySnapshot();
		}

		SubmitScoreText();
	}

	void OnDestroy()
	{
		EndTelemetry();      // flush the recording to disk (no-op if not recording)

		ClearNavBorrows();   // null the AIAgents' borrows before freeing the agents
		DeleteNav();

		// Undo only OUR perception registrations (the AIAgents own their own agent
		// registration). Never call Perception::Reset — it clears everything.
		UnregisterTarget(m_xBall);
		UnregisterTarget(m_xNpc[0]);
		UnregisterTarget(m_xNpc[1]);
	}

	// ---- Getters (for the unit tests + diagnostics) ----------------------
	RenderTest_Tennis::PointPhase GetPhase() const { return m_ePhase; }
	RenderTest_Tennis::ServeAttempt GetServeAttempt() const { return m_eServeAttempt; }
	int GetServerSide() const { return m_iServer; }
	uint32_t GetBallEpoch() const { return m_uBallEpoch; }
	uint32_t GetSidePoints(int iSide) const { return m_auPoints[iSide & 1]; }
	bool GetServeFromDeuceCourt() const
	{
		return RenderTest_Tennis::ServeCourtIsDeuce(
			static_cast<int>(m_auPoints[m_iServer]),
			static_cast<int>(m_auPoints[RenderTest_Tennis::OtherSideIndex(m_iServer)]));
	}
	// Heap-ownership accessors (diagnostics + relocation tests).
	const Zenith_NavMesh* GetNavMesh() const { return m_pxNavMesh; }
	const Zenith_NavMeshAgent* GetNavAgent(int i) const { return m_apxNavAgents[i & 1]; }
	bool IsNavMeshValid() const { return m_bNavMeshValid; }
	bool IsNavFallback(int i) const { return m_abNavFallback[i & 1]; }   // P2: agent on footwork after a path failure
	// Diagnostics + the independent-toss invariant test (P1): where the un-struck serve
	// is parked — a fixed point above the server's body, NOT the racket sweet spot (which
	// would make the contact gate tautological).
	Zenith_Maths::Vector3 GetServeTossPos() const { return ServeTossPos(); }
	int GetPendingWinner() const { return m_iPendingWinner; }   // side awarded the in-flight point

	// The non-physics half of the contact dispatch: map the gate outcome to its
	// referee effect and report whether the caller should LAUNCH. Out-of-range = a
	// genuine miss -> the OPPONENT scores; in-range-unarmed = discard (ball stays
	// live, no point); in-range-armed = launch. Public + called by HandleEligibleContact
	// so the dispatch wiring (esp. "miss -> the OTHER side", which the human reviewer's
	// gate-bypass class of bug lived in) is unit-testable without a physics body.
	bool ResolveContactOutcome(int iStriker, bool bInRange, bool bHasArmedShot)
	{
		switch (RenderTest_Tennis::ClassifyContactOutcome(bInRange, bHasArmedShot))
		{
		case RenderTest_Tennis::CONTACT_OUTCOME_OPPONENT_POINT:
			ResolvePoint(RenderTest_Tennis::OtherSideIndex(iStriker),
				RenderTest_TennisTelemetry::PointReason::OutOfRangeMiss);   // genuine miss
			return false;
		case RenderTest_Tennis::CONTACT_OUTCOME_DISCARD:
			return false;   // stale/unarmed: consumed + discarded
		case RenderTest_Tennis::CONTACT_OUTCOME_LAUNCH:
			return true;    // in-range + armed: caller launches
		}
		return false;
	}

	// ---- Serialization (version-only; all state is rebuilt at OnStart) ----
	void WriteToDataStream(Zenith_DataStream& xStream) const { const u_int uV = 2; xStream << uV; }
	void ReadFromDataStream(Zenith_DataStream& xStream) { u_int uV = 0; xStream >> uV; }

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Phase: %d  Serve: %d", static_cast<int>(m_ePhase), static_cast<int>(m_eServeAttempt));
		ImGui::Text("Points: %u-%u  Games: %u-%u", m_auPoints[0], m_auPoints[1], m_auGames[0], m_auGames[1]);
		ImGui::Text("Server: %d  Receiver: %d  LastHitter: %d", m_iServer, m_iExpectedReceiver, m_iLastHitter);
		ImGui::Text("Epoch: %u  Bounce#: %d  CrossedNet: %s  NavOK: %s",
			m_uBallEpoch, m_iBounceCountSinceHit, m_bCrossedNetSinceHit ? "yes" : "no", m_bNavMeshValid ? "yes" : "no");
	}
#endif

private:
	// ======================================================================
	// Resolution helpers
	// ======================================================================
	bool TryBallBody(Zenith_PhysicsBodyID& xOut) const
	{
		if (!m_xBall.IsValid() || !m_xBall.HasComponent<Zenith_ColliderComponent>())
			return false;
		Zenith_ColliderComponent& xCol = m_xBall.GetComponent<Zenith_ColliderComponent>();
		if (!xCol.HasValidBody())
			return false;
		xOut = xCol.GetBodyID();
		return true;
	}

	Zenith_Maths::Vector3 BallPos() const
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		if (m_xBall.IsValid() && m_xBall.HasComponent<Zenith_TransformComponent>())
			m_xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		return xPos;
	}

	RenderTest_TennisPlayerComponent* Npc(int i) const
	{
		if (m_xNpc[i].IsValid() && m_xNpc[i].HasComponent<RenderTest_TennisPlayerComponent>())
			return &m_xNpc[i].GetComponent<RenderTest_TennisPlayerComponent>();
		return nullptr;
	}
	Zenith_AIAgentComponent* AIAgent(int i) const
	{
		if (m_xNpc[i].IsValid() && m_xNpc[i].HasComponent<Zenith_AIAgentComponent>())
			return &m_xNpc[i].GetComponent<Zenith_AIAgentComponent>();
		return nullptr;
	}
	RenderTest_TennisAgentComponent* Brain(int i) const
	{
		if (m_xNpc[i].IsValid() && m_xNpc[i].HasComponent<RenderTest_TennisAgentComponent>())
			return &m_xNpc[i].GetComponent<RenderTest_TennisAgentComponent>();
		return nullptr;
	}

	Zenith_Maths::Vector3 NpcPos(int i) const
	{
		Zenith_Maths::Vector3 xPos(m_xCourt.m_fCenterX, m_xCourt.m_fSurfaceY, m_xCourt.BaselineZ(i));
		if (m_xNpc[i].IsValid() && m_xNpc[i].HasComponent<Zenith_TransformComponent>())
			m_xNpc[i].GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		return xPos;
	}

	RenderTest_Tennis::TennisSide ServerSide() const { return static_cast<RenderTest_Tennis::TennisSide>(m_iServer); }

	// ======================================================================
	// Ownership cleanup / move
	// ======================================================================
	// Null the (current) NPCs' AIAgent nav-agent borrows so nobody dereferences a
	// nav agent we are about to free (used by OnDestroy AND move-assignment).
	void ClearNavBorrows()
	{
		for (int i = 0; i < 2; ++i)
		{
			if (Zenith_AIAgentComponent* pxAgent = AIAgent(i))
				pxAgent->SetNavMeshAgent(nullptr);
		}
	}

	void DeleteNav()
	{
		for (int i = 0; i < 2; ++i)
		{
			delete m_apxNavAgents[i];   // delete agents first (they borrow the navmesh)
			m_apxNavAgents[i] = nullptr;
		}
		delete m_pxNavMesh;
		m_pxNavMesh = nullptr;
	}

	void MoveFrom(RenderTest_TennisMatchComponent& xOther)
	{
		m_xParentEntity = xOther.m_xParentEntity;
		m_xBall = xOther.m_xBall;
		m_xNpc[0] = xOther.m_xNpc[0];
		m_xNpc[1] = xOther.m_xNpc[1];
		m_xCourt = xOther.m_xCourt;

		m_ePhase = xOther.m_ePhase;
		m_eServeAttempt = xOther.m_eServeAttempt;
		m_iServer = xOther.m_iServer;
		m_iLastHitter = xOther.m_iLastHitter;
		m_iExpectedReceiver = xOther.m_iExpectedReceiver;
		m_uBallEpoch = xOther.m_uBallEpoch;
		m_iBounceCountSinceHit = xOther.m_iBounceCountSinceHit;
		m_iSettleFrames = xOther.m_iSettleFrames;
		m_iRallyShots = xOther.m_iRallyShots;
		m_iPendingWinner = xOther.m_iPendingWinner;

		m_bPrevSampleValid = xOther.m_bPrevSampleValid;
		m_bCrossedNetSinceHit = xOther.m_bCrossedNetSinceHit;
		m_bNavMeshValid = xOther.m_bNavMeshValid;
		// Telemetry ownership transfers to this instance; null the source so only
		// one referee flushes/ends the (global) recording.
		m_bTelemetryActive = xOther.m_bTelemetryActive;
		xOther.m_bTelemetryActive = false;
		for (int i = 0; i < 2; ++i)
		{
			m_aiNavStuckFrames[i] = xOther.m_aiNavStuckFrames[i];
			m_abNavFallback[i] = xOther.m_abNavFallback[i];
			m_axNavLastPos[i] = xOther.m_axNavLastPos[i];
		}

		m_xPrevBallPos = xOther.m_xPrevBallPos;
		m_xPrevBallVel = xOther.m_xPrevBallVel;
		m_xBallAngVel = xOther.m_xBallAngVel;
		m_xJitterRng = xOther.m_xJitterRng;

		m_fTimer = xOther.m_fTimer;
		m_fServeTimer = xOther.m_fServeTimer;
		m_fShotTimer = xOther.m_fShotTimer;
		m_fPointTimer = xOther.m_fPointTimer;

		m_auPoints[0] = xOther.m_auPoints[0];
		m_auPoints[1] = xOther.m_auPoints[1];
		m_auGames[0] = xOther.m_auGames[0];
		m_auGames[1] = xOther.m_auGames[1];

		// Transfer heap pointers + null the source (its dtor becomes a no-op).
		m_pxNavMesh = xOther.m_pxNavMesh;
		m_apxNavAgents[0] = xOther.m_apxNavAgents[0];
		m_apxNavAgents[1] = xOther.m_apxNavAgents[1];
		xOther.m_pxNavMesh = nullptr;
		xOther.m_apxNavAgents[0] = nullptr;
		xOther.m_apxNavAgents[1] = nullptr;
	}

	// ======================================================================
	// Navmesh + perception wiring (OnStart)
	// ======================================================================
	void BuildNavMeshAndAgents()
	{
		using namespace RenderTest_Tennis;
		const float fX0 = m_xCourt.m_fCenterX - m_xCourt.m_fSlabHalfWidth;
		const float fX1 = m_xCourt.m_fCenterX + m_xCourt.m_fSlabHalfWidth;
		const float fZ0 = m_xCourt.m_fNetZ - m_xCourt.m_fSlabHalfLength;
		const float fZ1 = m_xCourt.m_fNetZ + m_xCourt.m_fSlabHalfLength;
		const float fY  = m_xCourt.m_fSurfaceY;

		// Verts: 0=BL, 1=BR, 2=TR, 3=TL. Triangles {0,3,2},{0,2,1} give an UPWARD
		// (+Y) face normal (CCW from above) so the generator marks the slab walkable
		// — a downward winding fails the slope test and yields an empty navmesh.
		Zenith_Vector<Zenith_Maths::Vector3> axVerts;
		axVerts.PushBack(Zenith_Maths::Vector3(fX0, fY, fZ0));
		axVerts.PushBack(Zenith_Maths::Vector3(fX1, fY, fZ0));
		axVerts.PushBack(Zenith_Maths::Vector3(fX1, fY, fZ1));
		axVerts.PushBack(Zenith_Maths::Vector3(fX0, fY, fZ1));
		Zenith_Vector<uint32_t> axIdx;
		axIdx.PushBack(0); axIdx.PushBack(3); axIdx.PushBack(2);
		axIdx.PushBack(0); axIdx.PushBack(2); axIdx.PushBack(1);

		NavMeshGenerationConfig xCfg;
		m_pxNavMesh = Zenith_NavMeshGenerator::GenerateFromGeometry(axVerts, axIdx, xCfg);
		m_bNavMeshValid = (m_pxNavMesh != nullptr) && (m_pxNavMesh->GetPolygonCount() > 0);
		if (!m_bNavMeshValid)
		{
			Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
				"[Tennis] navmesh build produced no polygons — agents fall back to footwork");
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] navmesh built: %u polys",
				m_pxNavMesh->GetPolygonCount());
		}

		for (int i = 0; i < 2; ++i)
		{
			m_apxNavAgents[i] = new Zenith_NavMeshAgent();
			m_apxNavAgents[i]->SetNavMesh(m_pxNavMesh);
			m_apxNavAgents[i]->SetMoveSpeed(k_fRunSpeed);
			m_apxNavAgents[i]->SetStoppingDistance(0.3f);
			m_apxNavAgents[i]->SetTurnSpeed(0.0f);   // body re-asserts net-facing itself
			if (Zenith_AIAgentComponent* pxAgent = AIAgent(i))
				pxAgent->SetNavMeshAgent(m_apxNavAgents[i]);   // non-owning borrow
		}
	}

	void WirePerceptionTargets()
	{
		// The AIAgents already self-registered as perception AGENTS in OnAwake, and
		// each brain set its own sight config. The referee owns the TARGET set.
		if (m_xBall.IsValid())
			Zenith_PerceptionSystem::RegisterTarget(m_xBall.GetEntityID(), /*hostile*/ false);
		for (int i = 0; i < 2; ++i)
			if (m_xNpc[i].IsValid())
				Zenith_PerceptionSystem::RegisterTarget(m_xNpc[i].GetEntityID(), /*hostile*/ true);
	}

	void UnregisterTarget(const Zenith_Entity& xEnt)
	{
		if (xEnt.IsValid())
			Zenith_PerceptionSystem::UnregisterTarget(xEnt.GetEntityID());
	}

	// ======================================================================
	// Match / phase state machine
	// ======================================================================
	void ResetMatch()
	{
		m_auPoints[0] = m_auPoints[1] = 0;
		m_auGames[0] = m_auGames[1] = 0;
		m_iServer = 0;
		m_fTimer = 1.5f;
		m_ePhase = RenderTest_Tennis::POINT_PHASE_WARMUP;
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] === New match (autonomous AI) ===");
		RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::MatchStart,
			Zenith_EntityID{}, Zenith_EntityID{}, m_iServer, static_cast<int32_t>(k_uJitterSeed));
	}

	void StartPoint()
	{
		m_eServeAttempt = RenderTest_Tennis::SERVE_ATTEMPT_FIRST;
		m_iRallyShots = 0;
		m_iLastHitter = -1;
		m_iExpectedReceiver = RenderTest_Tennis::OtherSideIndex(m_iServer);
		m_ePhase = RenderTest_Tennis::POINT_PHASE_SERVING;
		StartServe();
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] Point start: server=%d  (%u-%u)",
			m_iServer, m_auPoints[0], m_auPoints[1]);
		RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::PointStart,
			NpcEntity(m_iServer), NpcEntity(m_iExpectedReceiver),
			m_iServer, static_cast<int32_t>(m_eServeAttempt), GetServeFromDeuceCourt() ? 1 : 0,
			(static_cast<int32_t>(m_auPoints[0]) << 8) | static_cast<int32_t>(m_auPoints[1]));
	}

	// Park the ball above the server's serve stance (gravity off) for the current
	// attempt + advance the ball epoch (resets both brains' arm guards).
	void StartServe()
	{
		m_iLastHitter = -1;
		Zenith_PhysicsBodyID xBody;
		if (TryBallBody(xBody))
		{
			g_xEngine.Physics().SetGravityEnabled(xBody, false);
			g_xEngine.Physics().SetLinearVelocity(xBody, Zenith_Maths::Vector3(0.0f));
			g_xEngine.Physics().SetAngularVelocity(xBody, Zenith_Maths::Vector3(0.0f));
			g_xEngine.Physics().TeleportBody(xBody, ServeTossPos());
		}
		m_xBallAngVel = Zenith_Maths::Vector3(0.0f);
		m_fServeTimer = k_fServeWatchdog;
		AdvanceBallEpoch();
	}

	// The serve toss is an INDEPENDENT world point above (and slightly in front of) the
	// server's body — deliberately NOT the racket sweet spot. Gluing the ball to the
	// racket makes the contact proximity gate tautological (zero distance by
	// construction), so a mistimed/misposed serve could never miss. Here the ball is
	// parked at a fixed point and the serve swing + arm-IK must actually bring the racket
	// up to it; the gate then measures the REAL racket-to-ball distance (a server that
	// fails to reach its own toss loses the point). Height matches the racket's
	// serve-contact reach; the forward offset (toward the net) matches where the swing
	// meets the ball. Independent of the racket pose -> the gate is meaningful.
	Zenith_Maths::Vector3 ServeTossPos() const
	{
		Zenith_Maths::Vector3 xPos = NpcPos(m_iServer);
		const float fForward = (m_iServer == 0) ? 1.0f : -1.0f;   // near faces +Z, far faces -Z
		xPos.y  = m_xCourt.m_fSurfaceY + k_fServeTossHeight;
		xPos.z += fForward * k_fServeTossForward;
		return xPos;
	}

	void TossBallAboveServer()
	{
		Zenith_PhysicsBodyID xBody;
		if (!TryBallBody(xBody))
			return;
		g_xEngine.Physics().SetGravityEnabled(xBody, false);
		g_xEngine.Physics().SetLinearVelocity(xBody, Zenith_Maths::Vector3(0.0f));
		g_xEngine.Physics().TeleportBody(xBody, ServeTossPos());
	}

	// The ONLY mutator of the ball epoch. Resets per-ball physics state + both
	// brains' arm guards synchronously (race-free: the referee runs in
	// OnLateUpdate, so both brains are reset before the next AIAgent tick).
	void AdvanceBallEpoch()
	{
		++m_uBallEpoch;
		m_iBounceCountSinceHit = 0;
		m_iSettleFrames = 0;
		m_bPrevSampleValid = false;
		m_bCrossedNetSinceHit = false;
		for (int i = 0; i < 2; ++i)
		{
			// Fresh ball: re-arm nav for both agents (clear the stall fallback) and
			// re-baseline the goalward-progress tracker at the agent's current position.
			m_aiNavStuckFrames[i] = 0;
			m_abNavFallback[i] = false;
			m_axNavLastPos[i] = NpcPos(i);
			if (RenderTest_TennisAgentComponent* pxBrain = Brain(i))
				pxBrain->ResetForNewBall(m_uBallEpoch);
		}
	}

	// Per-agent stall fallback (P2): hand an agent that isn't making headway to its
	// body's X-slide footwork for the rest of the ball. Based on the PLAYER's displacement
	// projected toward the goal (ProgressTowardDestination), NOT path-presence, raw motion,
	// or change-in-distance: SetDestination only QUEUES pathfinding (a failed path leaves
	// the body still), but a SUCCESSFUL path can ALSO stall — the dynamic body wedged
	// against geometry/the other player can keep HasPath() true AND jitter sideways while
	// never advancing. And since the BT rewrites the destination every tick, a goal
	// sliding toward a stationary agent would fake distance-reduction "progress" — so we
	// measure how far the PLAYER actually moved goalward. A stationary/blocked agent scores
	// ~0 regardless of where the goal moves. After k_uNavStuckFrames consecutive
	// non-advancing play frames toward an unreached destination, fall back.
	void CheckNavFallback()
	{
		for (int i = 0; i < 2; ++i)
		{
			if (m_abNavFallback[i] || !m_apxNavAgents[i])
				continue;
			const Zenith_Maths::Vector3 xPos = NpcPos(i);
			const float fProgress = RenderTest_Tennis::ProgressTowardDestination(
				m_axNavLastPos[i], xPos, m_apxNavAgents[i]->GetDestination());
			m_axNavLastPos[i] = xPos;
			const bool bStuck = RenderTest_Tennis::IsNavStalled(
				m_apxNavAgents[i]->HasReachedDestination(), fProgress, k_fNavProgressEps);
			m_aiNavStuckFrames[i] = bStuck ? (m_aiNavStuckFrames[i] + 1) : 0;
			if (m_aiNavStuckFrames[i] >= k_uNavStuckFrames)
			{
				m_abNavFallback[i] = true;   // ApplyPhaseAgentState will flip this agent to footwork
				m_apxNavAgents[i]->Stop();
				Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] agent %d nav stalled (player not advancing to goal) -> X-slide footwork fallback for this ball", i);
				RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::NavFallback,
					NpcEntity(i), Zenith_EntityID{}, i);
			}
		}
	}

	// Enable + nav-drive the agents only while the ball is in play; otherwise park
	// them (disabled + nav stopped + body XZ velocity zeroed + footwork hold). A
	// disabled agent's last nav velocity would otherwise persist (its OnUpdate
	// early-returns before the nav update).
	void ApplyPhaseAgentState()
	{
		const bool bPlay = (m_ePhase == RenderTest_Tennis::POINT_PHASE_SERVING)
			|| (m_ePhase == RenderTest_Tennis::POINT_PHASE_LIVE);
		const bool bShowcase = (m_ePhase == RenderTest_Tennis::POINT_PHASE_SHOWCASE);
		for (int i = 0; i < 2; ++i)
		{
			if (Zenith_AIAgentComponent* pxAgent = AIAgent(i))
				pxAgent->SetEnabled(bPlay);
			if (RenderTest_TennisPlayerComponent* pxBody = Npc(i))
			{
				// Nav owns this agent's XZ only while playing AND the navmesh built AND
				// this agent hasn't fallen back to footwork after a path failure (P2);
				// otherwise the body's own X-slide drives it to the leaf-set target.
				const bool bExternal = bPlay && m_bNavMeshValid && !m_abNavFallback[i];
				pxBody->SetExternalMovementDriven(bExternal);
				if (!bPlay)
				{
					if (m_apxNavAgents[i])
						m_apxNavAgents[i]->Stop();
					pxBody->ParkBody();
					if (!bShowcase)
						pxBody->SetFootworkTargetX(m_xCourt.m_fCenterX);   // settle at centre
				}
			}
		}
	}

	void ResolvePoint(int iWinner,
		RenderTest_TennisTelemetry::PointReason eReason = RenderTest_TennisTelemetry::PointReason::Unknown)
	{
		// Idempotent: once the point is resolved, ignore any further resolution this
		// ball. Per-frame the referee runs DetectAndHandleBounce() then SettleCheck()
		// back-to-back; a single frame can satisfy both a bounce-out and an off-slab
		// settle, and without this guard the second call would overwrite the (correct)
		// first winner. The first resolver wins; counters reset on the next StartPoint.
		if (m_ePhase == RenderTest_Tennis::POINT_PHASE_POINT_OVER)
			return;

		m_iPendingWinner = iWinner & 1;
		m_ePhase = RenderTest_Tennis::POINT_PHASE_POINT_OVER;
		m_fTimer = 1.3f;
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] Point resolved -> %d", m_iPendingWinner);

		RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::PointResolved,
			NpcEntity(m_iPendingWinner), NpcEntity(RenderTest_Tennis::OtherSideIndex(m_iPendingWinner)),
			m_iPendingWinner, static_cast<int32_t>(eReason), m_iRallyShots, m_iBounceCountSinceHit,
			m_bCrossedNetSinceHit ? 1.0f : 0.0f, static_cast<float>(m_iLastHitter), 0.0f, 0.0f,
			RenderTest_TennisTelemetry::PointReasonToString(static_cast<int32_t>(eReason)));
	}

	Zenith_EntityID NpcEntity(int i) const
	{
		return (i >= 0 && i < 2 && m_xNpc[i & 1].IsValid()) ? m_xNpc[i & 1].GetEntityID() : Zenith_EntityID{};
	}

	// ======================================================================
	// Telemetry (Zenith_Telemetry): per-frame samples + gameplay events -> disk.
	// Gated to the real "RenderTest" game scene + the --rendertest-tennis-telemetry
	// flag, so the unit-test fixtures (additive test scenes) never record.
	// ======================================================================
	void BeginTelemetry()
	{
		m_bTelemetryActive = false;
		if (!RenderTest_GameplayState::s_bTennisTelemetry)
			return;
		// Record ONLY the real "RenderTest" game scene (build index 0, loaded single) —
		// never the unit-test fixtures, which build referees in additive test scenes.
		const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(g_xEngine.Scenes().GetActiveScene());
		if (xInfo.m_strName != "RenderTest")
			return;

		Zenith_Telemetry::Header xH;
		xH.strSceneName        = xInfo.m_strName;
		xH.uSeed               = k_uJitterSeed;
		xH.fFixedDt            = 1.0f / 60.0f;
		xH.uSamplePeriodFrames = 6;   // 10 Hz
		xH.strPersonalityName  = "autonomous-ai";
		Zenith_Telemetry::GetRecorder().Begin(xH);
		m_bTelemetryActive = true;
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] telemetry recording -> %s.ztlm",
			RenderTest_GameplayState::s_strTennisTelemetryPath.c_str());
	}

	void EndTelemetry()
	{
		if (!m_bTelemetryActive)
			return;
		m_bTelemetryActive = false;
		Zenith_Telemetry::Recorder& xRec = Zenith_Telemetry::GetRecorder();
		if (!xRec.IsRecording())
			return;

		const std::string strBase = RenderTest_GameplayState::s_strTennisTelemetryPath;
		const std::string strBin  = strBase + ".ztlm";
		const std::string strJson = strBase + ".json";
		xRec.End(strBin.c_str(), strJson.c_str(), &RenderTest_TennisTelemetry::EventTypeToString);

		// CSV sidecars for spreadsheet / scripted analysis (re-read the binary).
		Zenith_Telemetry::Reader xReader;
		if (xReader.LoadFromFile(strBin.c_str()))
		{
			const std::string strFramesCsv = strBase + "_frames.csv";
			const std::string strEventsCsv = strBase + "_events.csv";
			xReader.ExportCsv(strFramesCsv.c_str(), strEventsCsv.c_str(),
				&RenderTest_TennisTelemetry::EventTypeToString);
		}
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] telemetry flushed: %s (+ .json, _frames.csv, _events.csv)", strBin.c_str());
	}

	// Periodic checkpoint: write the full accumulated recording to disk WITHOUT ending
	// (recording continues). Keeps the on-disk .ztlm/.json current for a run that may be
	// killed rather than exited cleanly.
	void FlushTelemetrySnapshot()
	{
		if (!m_bTelemetryActive)
			return;
		const std::string strBase = RenderTest_GameplayState::s_strTennisTelemetryPath;
		const std::string strBin  = strBase + ".ztlm";
		const std::string strJson = strBase + ".json";
		Zenith_Telemetry::GetRecorder().FlushSnapshot(strBin.c_str(), strJson.c_str(),
			&RenderTest_TennisTelemetry::EventTypeToString);
	}

	void SampleTelemetry()
	{
		using namespace RenderTest_TennisTelemetry;
		const bool bPlay = (m_ePhase == RenderTest_Tennis::POINT_PHASE_SERVING)
			|| (m_ePhase == RenderTest_Tennis::POINT_PHASE_LIVE);
		const bool bBallLive = (m_ePhase == RenderTest_Tennis::POINT_PHASE_LIVE)
			|| (m_ePhase == RenderTest_Tennis::POINT_PHASE_SERVING && m_iLastHitter >= 0);
		const Zenith_Maths::Vector3 xBallPos = BallPos();

		Zenith_Telemetry::FrameSample xF;

		// --- the two players ---
		for (int i = 0; i < 2; ++i)
		{
			Zenith_Telemetry::EntitySnapshot xS;
			xS.xId  = NpcEntity(i);
			xS.xPos = NpcPos(i);
			if (RenderTest_TennisPlayerComponent* pxBody = Npc(i))
				xS.xForward = Zenith_Maths::Vector3(0.0f, 0.0f, pxBody->IsFacingPositiveZ() ? 1.0f : -1.0f);
			uint32_t uFlags = PlayerFlags::IsPlayer;
			if (i == m_iServer)                  uFlags |= PlayerFlags::IsServer;
			if (i == 0)                          uFlags |= PlayerFlags::IsNearSide;
			if (m_abNavFallback[i])              uFlags |= PlayerFlags::NavFallback;
			if (bPlay && m_bNavMeshValid && !m_abNavFallback[i]) uFlags |= PlayerFlags::NavExternal;
			if (m_ePhase == RenderTest_Tennis::POINT_PHASE_LIVE && i == m_iExpectedReceiver)
				uFlags |= PlayerFlags::IsReceiver;
			xS.uStateFlags     = uFlags;
			xS.fSecondaryFloat = RenderTest_Tennis::HorizDist(xS.xPos, xBallPos);   // distance to the ball
			xF.axEntities.PushBack(xS);
		}

		// --- the ball (also carries global match state for per-frame replay) ---
		{
			Zenith_Telemetry::EntitySnapshot xS;
			xS.xId  = m_xBall.IsValid() ? m_xBall.GetEntityID() : Zenith_EntityID{};
			xS.xPos = xBallPos;
			Zenith_PhysicsBodyID xBody;
			Zenith_Maths::Vector3 xVel(0.0f);
			if (TryBallBody(xBody))
				xVel = g_xEngine.Physics().GetLinearVelocity(xBody);
			const float fSpeed = Zenith_Maths::Length(xVel);
			xS.xForward        = (fSpeed > 1e-3f) ? (xVel / fSpeed) : Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
			xS.fSecondaryFloat = fSpeed;
			uint32_t uFlags = BallFlags::IsBall;
			if (bBallLive)                                       uFlags |= BallFlags::InFlight;
			if (Zenith_Maths::Length(m_xBallAngVel) > 1.0f)      uFlags |= BallFlags::HasSpin;
			xS.uStateFlags  = uFlags;
			xS.uAIIntent    = static_cast<uint8_t>(m_ePhase);                       // phase per frame
			xS.uHeldItemTag = static_cast<uint8_t>((m_iServer & 0x0F) | ((m_iExpectedReceiver & 0x0F) << 4));
			xS.xAITargetPos = m_xBallAngVel;                                        // spin vector
			xF.axEntities.PushBack(xS);
		}

		Zenith_Telemetry::GetRecorder().RecordFrame(xF);
	}

	void TickPhaseTimers(float fDt)
	{
		switch (m_ePhase)
		{
		case RenderTest_Tennis::POINT_PHASE_WARMUP:
			m_fTimer -= fDt;
			if (m_fTimer <= 0.0f)
				StartPoint();
			break;
		case RenderTest_Tennis::POINT_PHASE_POINT_OVER:
			m_fTimer -= fDt;
			if (m_fTimer <= 0.0f)
				AwardPoint(m_iPendingWinner);
			break;
		case RenderTest_Tennis::POINT_PHASE_MATCH_OVER:
			m_fTimer -= fDt;
			if (m_fTimer <= 0.0f)
				ResetMatch();
			break;
		default:
			break;
		}
	}

	// Deterministic termination so the match always restarts.
	void TickWatchdogs(float fDt)
	{
		if (m_ePhase == RenderTest_Tennis::POINT_PHASE_SERVING)
		{
			// Serve never struck, or a struck serve stalled in the net before
			// bouncing -> fault (second attempt -> double fault).
			m_fServeTimer -= fDt;
			if (m_fServeTimer <= 0.0f)
				HandleServeFault();
			return;
		}
		if (m_ePhase == RenderTest_Tennis::POINT_PHASE_LIVE)
		{
			m_fShotTimer -= fDt;
			m_fPointTimer -= fDt;
			const bool bStall = (m_fShotTimer <= 0.0f) || (m_fPointTimer <= 0.0f) || (m_iRallyShots >= k_iMaxRallyShots);
			if (bStall)
			{
				const int iWinner = RenderTest_Tennis::ResolveStallWinner(m_iLastHitter, m_bCrossedNetSinceHit);
				ResolvePoint(iWinner, RenderTest_TennisTelemetry::PointReason::RallyTimeout);
			}
		}
	}

	void HandleServeFault()
	{
		// A fault: the serve was never struck within the watchdog, or a struck serve
		// stalled in the net before bouncing.
		RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::Fault,
			NpcEntity(m_iServer), Zenith_EntityID{}, m_iServer, static_cast<int32_t>(m_eServeAttempt));
		if (m_eServeAttempt == RenderTest_Tennis::SERVE_ATTEMPT_FIRST)
		{
			m_eServeAttempt = RenderTest_Tennis::SERVE_ATTEMPT_SECOND;
			StartServe();
		}
		else
		{
			// Watchdog double fault — the second serve was never properly struck (or
			// stalled in the net before bouncing). Distinct from an in-box double fault
			// (struck + bounced but landed out), which ClassifyServe reports as DoubleFault.
			RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::DoubleFault,
				NpcEntity(m_iServer), NpcEntity(m_iExpectedReceiver), m_iServer, m_iExpectedReceiver);
			ResolvePoint(m_iExpectedReceiver, RenderTest_TennisTelemetry::PointReason::ServeTimeout);
		}
	}

	// ======================================================================
	// Per-frame physics (spin + bounce + net)
	// ======================================================================
	void ApplySpinAndMagnus(float fDt)
	{
		Zenith_PhysicsBodyID xBody;
		if (!TryBallBody(xBody))
			return;
		const Zenith_Maths::Vector3 xVel = g_xEngine.Physics().GetLinearVelocity(xBody);
		const Zenith_Maths::Vector3 xDV = RenderTest_Tennis::MagnusDeltaV(xVel, m_xBallAngVel, k_fMagnusK, fDt);
		g_xEngine.Physics().SetLinearVelocity(xBody, xVel + xDV);

		m_xBallAngVel = RenderTest_Tennis::ApplySpinDecay(m_xBallAngVel, k_fSpinDecay, fDt);
		g_xEngine.Physics().SetAngularVelocity(xBody, m_xBallAngVel);   // visible spin
	}

	void LatchNetCrossing()
	{
		if (!m_bPrevSampleValid)
			return;
		const RenderTest_Tennis::TennisNetCrossing xX =
			RenderTest_Tennis::NetCrossingClearance(m_xPrevBallPos, BallPos(), m_xCourt.m_fNetZ);
		if (xX.m_bCrossed)
		{
			const bool bLegal = xX.m_fHeightAtCross > m_xCourt.m_fSurfaceY + m_xCourt.m_fNetHeight;
			if (bLegal)
				m_bCrossedNetSinceHit = true;   // latched; cleared only by AdvanceBallEpoch
			RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::NetCross,
				Zenith_EntityID{}, Zenith_EntityID{}, bLegal ? 1 : 0, 0, 0, 0,
				xX.m_fHeightAtCross - (m_xCourt.m_fSurfaceY + m_xCourt.m_fNetHeight));
		}
	}

	void DetectAndHandleBounce()
	{
		Zenith_PhysicsBodyID xBody;
		if (!TryBallBody(xBody))
			return;
		const Zenith_Maths::Vector3 xCur = BallPos();
		const float fFloor = m_xCourt.m_fSurfaceY + m_xCourt.m_fBallRadius;
		// Fire when a DESCENDING ball enters the floor zone. The band tolerates Jolt
		// resting/depenetrating the ball slightly ABOVE the true floor before we sample
		// it: Jolt steps before this OnLateUpdate, so with near-zero Jolt restitution a
		// landing ball is parked at ~fFloor+skin. The old strict "xCur.y <= fFloor" test
		// then missed that crossing and the ball just rolled (the reported "doesn't
		// bounce" bug). prevPos above the band + cur inside it = a real landing this frame.
		const float fBand = m_xCourt.m_fBallRadius;
		if (!m_bPrevSampleValid || m_xPrevBallVel.y >= 0.0f
			|| m_xPrevBallPos.y <= fFloor + fBand || xCur.y > fFloor + fBand)
			return;

		// Authoritative spin-aware bounce from the stored PRE-impact velocity.
		const Zenith_Maths::Vector3 xBounce = RenderTest_Tennis::BounceVelocity(
			m_xPrevBallVel, m_xBallAngVel, k_fBounceRestitution, k_fBounceFriction, k_fTopspinKick, k_fSliceSkid);
		g_xEngine.Physics().SetLinearVelocity(xBody, xBounce);
		++m_iBounceCountSinceHit;

		const RenderTest_Tennis::TennisSide eLandSide = m_xCourt.SideOfZ(xCur.z);
		const bool bServeBounce = (m_ePhase == RenderTest_Tennis::POINT_PHASE_SERVING && m_iBounceCountSinceHit == 1);
		const bool bInBounds = RenderTest_Tennis::IsInBounds(m_xCourt, xCur, eLandSide);
		RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::Bounce,
			Zenith_EntityID{}, Zenith_EntityID{},
			static_cast<int32_t>(eLandSide), m_iBounceCountSinceHit, bInBounds ? 1 : 0, bServeBounce ? 1 : 0,
			xCur.x, xCur.z, m_xPrevBallVel.y);
		ClassifyBounce(xCur, eLandSide);
	}

	void ClassifyBounce(const Zenith_Maths::Vector3& xLand, RenderTest_Tennis::TennisSide eLandSide)
	{
		using namespace RenderTest_Tennis;
		if (m_ePhase == POINT_PHASE_SERVING && m_iBounceCountSinceHit == 1)
		{
			const bool bSecond = (m_eServeAttempt == SERVE_ATTEMPT_SECOND);
			const ServeResult eRes = ClassifyServe(m_xCourt, xLand, ServerSide(),
				GetServeFromDeuceCourt(), bSecond, m_bCrossedNetSinceHit);
			if (eRes == SERVE_RESULT_GOOD)
			{
				m_ePhase = POINT_PHASE_LIVE;
				m_fPointTimer = k_fPointTimeout;
				m_fShotTimer = k_fShotWatchdog;
			}
			else if (eRes == SERVE_RESULT_FAULT)
			{
				RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::Fault,
					NpcEntity(m_iServer), Zenith_EntityID{}, m_iServer, static_cast<int32_t>(m_eServeAttempt));
				m_eServeAttempt = SERVE_ATTEMPT_SECOND;
				StartServe();
			}
			else
			{
				RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::DoubleFault,
					NpcEntity(m_iServer), NpcEntity(m_iExpectedReceiver), m_iServer, m_iExpectedReceiver);
				ResolvePoint(m_iExpectedReceiver, RenderTest_TennisTelemetry::PointReason::DoubleFault);
			}
			return;
		}

		// LIVE rally bounce (m_iLastHitter is the striker).
		if (m_iLastHitter < 0)
			return;
		const PointOutcome eOut = ClassifyBounceOutcome(m_xCourt, xLand, eLandSide,
			static_cast<TennisSide>(m_iLastHitter), m_iBounceCountSinceHit, m_bCrossedNetSinceHit);
		if (eOut == POINT_OUTCOME_HITTER_WINS)
		{
			// bounceIdx>=2: the opponent let it bounce twice. If only the serve has been
			// struck it's an unreturned serve (ace-like); otherwise a rally winner.
			const RenderTest_TennisTelemetry::PointReason eReason = (m_iRallyShots <= 1)
				? RenderTest_TennisTelemetry::PointReason::ServeUnreturned
				: RenderTest_TennisTelemetry::PointReason::DoubleBounce;
			ResolvePoint(m_iLastHitter, eReason);
		}
		else if (eOut == POINT_OUTCOME_HITTER_LOSES)
		{
			// Failed to cross the net legally -> into-net/own-side; else cleared the net
			// but landed out of bounds.
			const RenderTest_TennisTelemetry::PointReason eReason = m_bCrossedNetSinceHit
				? RenderTest_TennisTelemetry::PointReason::LandedOut
				: RenderTest_TennisTelemetry::PointReason::IntoNetOrOwnSide;
			ResolvePoint(OtherSideIndex(m_iLastHitter), eReason);
		}
		// CONTINUE -> rally lives on
	}

	// Resolve a live ball that has left play, so points end cleanly instead of fizzling
	// to the shot-watchdog (telemetry: 58% RallyTimeout). Two cases:
	//  (1) OFF THE SLAB — the floating court has little run-back room, so a deep shot the
	//      opponent doesn't return bounces once then carries off the far edge into the
	//      void (telemetry diag: ball at z>slab-edge, descending, never bounces again).
	//      If it bounced in first, the opponent failed to return a good ball -> hitter's
	//      point; if it never bounced (overshot), it's the hitter's error.
	//  (2) DEAD LOW — a gently-paced shot bounces weakly and rolls along the surface
	//      without a detectable 2nd bounce; once it stays below a returnable height for a
	//      sustained window it was not returned.
	void SettleCheck()
	{
		using namespace RenderTest_Tennis;
		// Already resolved this frame (e.g. by DetectAndHandleBounce) — don't double-resolve.
		if (m_ePhase == POINT_PHASE_POINT_OVER)
			return;
		Zenith_PhysicsBodyID xBody;
		if (m_iLastHitter < 0 || !TryBallBody(xBody))
		{
			m_iSettleFrames = 0;
			return;
		}
		const Zenith_Maths::Vector3 xPos = BallPos();

		// (1) Out of play — left the floating slab (XZ) or fell below it (Y).
		const bool bOffSlab =
			   (std::fabs(xPos.z - m_xCourt.m_fNetZ)    > m_xCourt.m_fSlabHalfLength)
			|| (std::fabs(xPos.x - m_xCourt.m_fCenterX) > m_xCourt.m_fSlabHalfWidth)
			|| (xPos.y < m_xCourt.m_fSurfaceY - 1.0f);
		if (bOffSlab)
		{
			const int iBallSide = static_cast<int>(m_xCourt.SideOfZ(xPos.z));
			const TennisSettleResolution xR =
				ResolveOffSlabSettle(m_iBounceCountSinceHit, iBallSide, m_iLastHitter, m_iRallyShots);
			ResolvePoint(xR.m_iWinnerSide, SettleCauseToReason(xR.m_eCause));
			return;
		}

		// (2) Dead low on the slab.
		if (m_iBounceCountSinceHit < 1)
		{
			m_iSettleFrames = 0;
			return;
		}
		const bool bDead = (xPos.y <= m_xCourt.m_fSurfaceY + k_fDeadHeight);
		m_iSettleFrames = bDead ? (m_iSettleFrames + 1) : 0;
		if (m_iSettleFrames < k_uSettleFrames)
			return;

		const int iSettleSide = static_cast<int>(m_xCourt.SideOfZ(xPos.z));
		const TennisSettleResolution xR = ResolveDeadLowSettle(iSettleSide, m_iLastHitter, m_iRallyShots);
		ResolvePoint(xR.m_iWinnerSide, SettleCauseToReason(xR.m_eCause));
	}

	// Map a neutral SettleCheck cause (pure, unit-tested in the decision header) to the
	// telemetry PointReason.
	static RenderTest_TennisTelemetry::PointReason SettleCauseToReason(RenderTest_Tennis::TennisSettleCause eCause)
	{
		using PR = RenderTest_TennisTelemetry::PointReason;
		switch (eCause)
		{
		case RenderTest_Tennis::TENNIS_SETTLE_SERVE_UNRETURNED:     return PR::ServeUnreturned;
		case RenderTest_Tennis::TENNIS_SETTLE_DOUBLE_BOUNCE:        return PR::DoubleBounce;
		case RenderTest_Tennis::TENNIS_SETTLE_LANDED_OUT:           return PR::LandedOut;
		case RenderTest_Tennis::TENNIS_SETTLE_INTO_NET_OR_OWN_SIDE: return PR::IntoNetOrOwnSide;
		}
		return PR::Unknown;
	}

	// ======================================================================
	// Contact (sole consumer; eligibility + epoch + proximity gated)
	// ======================================================================
	int EligibleStriker() const
	{
		return RenderTest_Tennis::ComputeEligibleStriker(m_ePhase, m_iLastHitter, m_iServer, m_iExpectedReceiver);
	}

	void HandleContacts()
	{
		const int iElig = EligibleStriker();
		for (int i = 0; i < 2; ++i)
		{
			RenderTest_TennisPlayerComponent* pxBody = Npc(i);
			if (!pxBody)
				continue;
			const bool bContact = pxBody->ConsumeContact();   // referee is the sole consumer
			if (!bContact || i != iElig)
				continue;   // no contact, or an ineligible/wrong-player swing: discard
			HandleEligibleContact(i, pxBody);
		}
	}

	void HandleEligibleContact(int iStriker, RenderTest_TennisPlayerComponent* pxBody)
	{
		using namespace RenderTest_Tennis;
		Zenith_PhysicsBodyID xBody;
		if (!TryBallBody(xBody))
			return;
		const Zenith_Maths::Vector3 xBallPos = BallPos();

		// Proximity gate for EVERY contact (serve, forehand, backhand): the ball must
		// be within racket reach of the POSED sweet spot, else it's a genuine miss ->
		// opponent point. (No "magical" launch regardless of racket position.) The serve
		// connects because the ball parks at an INDEPENDENT toss point (see ServeTossPos)
		// and UpdateArmIK drives the racket head onto that ball each frame, so the gate
		// measures the real racket-vs-ball distance — NOT a zero-by-construction
		// toss==sweet-spot (which is exactly the tautology the independent toss removed).
		// Serve returnability comes from the reduced serve pace, not from serves missing.
		// The launch/opponent-point/discard branching is the pure ClassifyContactOutcome.
		const Zenith_Maths::Vector3 xSweet = pxBody->GetRacketSweetSpotPos();
		const bool bInRange = IsWithinContactRange(xBallPos, xSweet, k_fContactRadius);

		RenderTest_TennisAgentComponent* pxBrain = Brain(iStriker);
		TennisShotDecision xShot;
		const bool bArmed = pxBrain && pxBrain->TryGetDecidedShot(m_uBallEpoch, xShot);

		// Contact telemetry: every eligible swing (serve or groundstroke), whether it
		// connects or whiffs. Carries the DECIDED shot intent (pre-jitter) so analytics
		// see shot selection + the in-range outcome.
		const bool bServingContact = (m_ePhase == POINT_PHASE_SERVING);
		RenderTest_TennisTelemetry::Emit(
			bServingContact ? RenderTest_TennisTelemetry::EventType::ServeStruck
			                : RenderTest_TennisTelemetry::EventType::Contact,
			NpcEntity(iStriker), Zenith_EntityID{},
			bArmed ? static_cast<int32_t>(xShot.m_eType) : -1, iStriker, bInRange ? 1 : 0,
			static_cast<int32_t>(m_eServeAttempt),
			bArmed ? xShot.m_fPace : 0.0f,
			bArmed ? xShot.m_xAim.x : xBallPos.x,
			bArmed ? xShot.m_xAim.z : xBallPos.z,
			bArmed ? Zenith_Maths::Length(xShot.m_xSpinAngVel) : 0.0f);

		// Dispatch the gate outcome (resolves a miss to the opponent / discards a stale
		// swing / signals launch). Routed through ResolveContactOutcome so the wiring —
		// especially "miss -> the OTHER side scores" — is unit-tested without a physics
		// body (the launch itself stays physics + windowed-verified). xShot is valid iff
		// it returns true (LAUNCH => bInRange && bArmed).
		if (!ResolveContactOutcome(iStriker, bInRange, bArmed))
			return;

		// Per-hit randomness scaled by shot risk + how stretched the striker is.
		const float fBalance = pxBrain->GetPlayerState().m_fBalance;
		const float fDifficulty = glm::clamp(
			xShot.m_fRisk * k_fRiskWeight + (1.0f - fBalance) * k_fStretchWeight, 0.0f, 1.0f);
		const Zenith_Maths::Vector3 xAim2 = JitterAim(xShot.m_xAim, xShot.m_fRisk, fDifficulty, m_xJitterRng);
		const float fPace2 = JitterPace(xShot.m_fPace, xShot.m_fRisk, fDifficulty, m_xJitterRng);

		float fFlightT = 0.0f;
		const Zenith_Maths::Vector3 xV0 = ComputeLaunchVelocity(xBallPos, xAim2, xShot.m_xSpinAngVel, fPace2, fFlightT);
		g_xEngine.Physics().SetGravityEnabled(xBody, true);
		g_xEngine.Physics().SetLinearVelocity(xBody, xV0);
		g_xEngine.Physics().SetAngularVelocity(xBody, xShot.m_xSpinAngVel);
		m_xBallAngVel = xShot.m_xSpinAngVel;

		m_iLastHitter = iStriker;
		m_iExpectedReceiver = OtherSideIndex(iStriker);
		++m_iRallyShots;
		m_fShotTimer = k_fShotWatchdog;
		AdvanceBallEpoch();
	}

	// ======================================================================
	// Blackboard publish (referee -> brain/leaves)
	// ======================================================================
	void PublishBlackboards()
	{
		for (int i = 0; i < 2; ++i)
		{
			Zenith_AIAgentComponent* pxAgent = AIAgent(i);
			if (!pxAgent)
				continue;
			Zenith_Blackboard& xBB = pxAgent->GetBlackboard();
			using namespace RenderTest_TennisBB;
			xBB.SetInt(k_szPhase, static_cast<int32_t>(m_ePhase));
			xBB.SetInt(k_szBallEpoch, static_cast<int32_t>(m_uBallEpoch));
			xBB.SetInt(k_szMySide, i);
			xBB.SetBool(k_szIsServer, i == m_iServer);
			xBB.SetBool(k_szServeFromDeuce, GetServeFromDeuceCourt());
			xBB.SetBool(k_szIsSecondServe, m_eServeAttempt == RenderTest_Tennis::SERVE_ATTEMPT_SECOND);
			xBB.SetInt(k_szMyPoints, static_cast<int32_t>(m_auPoints[i]));
			xBB.SetInt(k_szOppPoints, static_cast<int32_t>(m_auPoints[RenderTest_Tennis::OtherSideIndex(i)]));
			xBB.SetBool(k_szIsMyBall,
				m_ePhase == RenderTest_Tennis::POINT_PHASE_LIVE && i == m_iExpectedReceiver);
			// The serve is only armable while the ball is parked above the server
			// (not yet struck). Once struck the ball is in flight and the serve
			// branch must NOT re-fire (phase stays SERVING until the bounce).
			xBB.SetBool(k_szServeBallParked,
				m_ePhase == RenderTest_Tennis::POINT_PHASE_SERVING && m_iLastHitter < 0);
			xBB.SetVector3(k_szBallSpin, m_xBallAngVel);
			if (m_xBall.IsValid())
				xBB.SetEntityID(k_szBallEntity, m_xBall.GetEntityID());
			if (m_xNpc[RenderTest_Tennis::OtherSideIndex(i)].IsValid())
				xBB.SetEntityID(k_szOppEntity, m_xNpc[RenderTest_Tennis::OtherSideIndex(i)].GetEntityID());
		}
	}

	// ======================================================================
	// Scoring (kept from the scripted version; now driven by physics events)
	// ======================================================================
	void AwardPoint(int iWinner)
	{
		m_auPoints[iWinner]++;
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] Point -> %d  (%u-%u)", iWinner, m_auPoints[0], m_auPoints[1]);

		const uint32_t a = m_auPoints[0], b = m_auPoints[1];
		const bool bDeuceCap = (a + b) >= 12 && a != b;
		const bool bGame = (a >= 4 || b >= 4) && (std::max(a, b) - std::min(a, b) >= 2 || bDeuceCap);
		if (bGame)
		{
			const int iGameWinner = (a > b) ? 0 : 1;
			m_auGames[iGameWinner]++;
			m_auPoints[0] = m_auPoints[1] = 0;
			m_iServer = 1 - m_iServer;
			Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] Game -> %d  (games %u-%u)",
				iGameWinner, m_auGames[0], m_auGames[1]);
			RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::GameWon,
				NpcEntity(iGameWinner), Zenith_EntityID{}, iGameWinner,
				static_cast<int32_t>(m_auGames[0]), static_cast<int32_t>(m_auGames[1]));

			const uint32_t ga = m_auGames[0], gb = m_auGames[1];
			const bool bSet = (ga >= 4 || gb >= 4) && (std::max(ga, gb) - std::min(ga, gb) >= 2 || std::max(ga, gb) >= 6);
			if (bSet)
			{
				const int iSetWinner = (ga > gb) ? 0 : 1;
				Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] === Match over: side %d wins %u-%u ===",
					iSetWinner, ga, gb);
				RenderTest_TennisTelemetry::Emit(RenderTest_TennisTelemetry::EventType::MatchOver,
					NpcEntity(iSetWinner), Zenith_EntityID{}, iSetWinner,
					static_cast<int32_t>(ga), static_cast<int32_t>(gb));
				m_ePhase = RenderTest_Tennis::POINT_PHASE_MATCH_OVER;
				m_fTimer = 3.0f;
				return;
			}
		}
		StartPoint();
	}

	static std::string PointStr(uint32_t p) { static const char* k[4] = { "0","15","30","40" }; return p < 4 ? k[p] : "40"; }

	std::string ScoreString() const
	{
		const uint32_t a = m_auPoints[0], b = m_auPoints[1];
		std::string strPts;
		if (a >= 3 && b >= 3)
		{
			if (a == b) strPts = "Deuce";
			else        strPts = (a > b) ? "Ad Near" : "Ad Far";
		}
		else
		{
			strPts = PointStr(a) + " - " + PointStr(b);
		}
		return strPts + "    Games " + std::to_string(m_auGames[0]) + "-" + std::to_string(m_auGames[1]);
	}

	void SubmitScoreText()
	{
		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (!pxCanvas)
			return;
		const Zenith_Maths::Vector3 xAnchor(m_xCourt.m_fCenterX, m_xCourt.m_fSurfaceY + 4.5f, m_xCourt.m_fNetZ);
		const Zenith_Maths::Matrix4 xVP = g_xEngine.FluxGraphics().GetViewProjMatrix();
		const Zenith_Maths::Vector4 xClip = xVP * Zenith_Maths::Vector4(xAnchor, 1.0f);
		if (xClip.w <= 0.0001f)
			return;
		int iW = 1280, iH = 720;
		if (Zenith_Window::GetInstance())
			Zenith_Window::GetInstance()->GetSize(iW, iH);
		const float fNdcX = xClip.x / xClip.w;
		const float fNdcY = xClip.y / xClip.w;
		const Zenith_Maths::Vector2 xPx(
			(fNdcX * 0.5f + 0.5f) * static_cast<float>(iW),
			(fNdcY * 0.5f + 0.5f) * static_cast<float>(iH));
		const float fSize = glm::clamp(900.0f / xClip.w, 18.0f, 64.0f);
		pxCanvas->SubmitText(ScoreString(), xPx, fSize, Zenith_Maths::Vector4(1.0f, 1.0f, 0.95f, 1.0f));
	}

	// ======================================================================
	// IK showcase (near NPC repeats one stroke against a frozen ball)
	// ======================================================================
	void UpdateShowcase(float)
	{
		RenderTest_TennisPlayerComponent* pxNpc = Npc(0);
		if (!pxNpc)
			return;
		pxNpc->SetFootworkTargetX(m_xCourt.m_fCenterX);
		pxNpc->SetIKShowcaseHold(true);

		const Zenith_Maths::Vector3 xNpcPos = NpcPos(0);
		const int iStroke = RenderTest_GameplayState::s_iTennisShowcaseStroke;
		Zenith_Maths::Vector3 xContact;
		switch (iStroke)
		{
		case 1:  xContact = xNpcPos + Zenith_Maths::Vector3( 0.45f, 0.45f, 0.60f); break;
		case 2:  xContact = xNpcPos + Zenith_Maths::Vector3(-0.30f, 0.50f, 0.55f); break;
		default: xContact = xNpcPos + Zenith_Maths::Vector3( 0.25f, 1.95f, 0.15f); break;
		}
		Zenith_PhysicsBodyID xBody;
		if (TryBallBody(xBody))
		{
			g_xEngine.Physics().SetGravityEnabled(xBody, false);
			g_xEngine.Physics().SetLinearVelocity(xBody, Zenith_Maths::Vector3(0.0f));
			g_xEngine.Physics().TeleportBody(xBody, xContact);
		}
		if (pxNpc->IsReady())
		{
			const Zenith_Maths::Vector3 xAim(m_xCourt.m_fCenterX, m_xCourt.m_fSurfaceY + 1.0f, m_xCourt.BaselineZ(1));
			if (iStroke == 0)
				pxNpc->RequestServe(xAim);
			else
				pxNpc->RequestSwing(xAim, xContact.x);
		}
		pxNpc->ConsumeContact();   // swallow so the ball isn't launched
	}

	// ======================================================================
	// Tuning
	// ======================================================================
	static constexpr float k_fJoltRestitution = 0.02f;   // Jolt barely bounces; BounceVelocity owns it
	static constexpr float k_fJoltFriction    = 0.02f;
	static constexpr float k_fBounceRestitution = 0.70f; // tennis-ball bounce model
	static constexpr float k_fBounceFriction    = 0.25f;
	static constexpr float k_fTopspinKick       = 0.35f;
	static constexpr float k_fSliceSkid         = 0.30f;
	static constexpr float k_fMagnusK           = 0.012f;
	static constexpr float k_fSpinDecay         = 0.25f;
	// Contact proximity-gate tolerance. DEVIATES from the plan's 0.40 (idealized
	// racket-head size): the testbed's arm-IK + ball placement carry real slop, so at
	// 0.40 legitimate returns land just outside the gate and miss, collapsing rallies.
	// 0.80 keeps the gate meaningful (a clearly out-of-range swing >0.8 m still misses ->
	// opponent point) while tolerating that slop so a player who runs the ball down
	// reliably connects and rallies sustain past one or two shots. For serves the ball
	// parks at an independent toss point and the arm-IK reliably drives the racket onto
	// it (returnability is tuned via serve pace, not by serves missing the gate).
	static constexpr float k_fContactRadius     = 0.80f;
	static constexpr float k_fRiskWeight        = 0.60f;
	static constexpr float k_fStretchWeight     = 0.40f;
	static constexpr float k_fRunSpeed          = 6.0f;
	static constexpr float k_fServeWatchdog     = 4.0f;
	static constexpr float k_fShotWatchdog      = 3.5f;
	static constexpr float k_fPointTimeout      = 20.0f;
	static constexpr int   k_iMaxRallyShots     = 30;
	static constexpr float k_fDeadHeight        = 0.35f;  // a bounced ball staying below surface+this is rolling/dying (m)
	static constexpr int   k_uSettleFrames      = 15;     // consecutive dead frames before resolving (not-returned)
	static constexpr uint32_t k_uJitterSeed     = 0x2468ACEu;
	static constexpr int   k_uNavStuckFrames    = 30;     // consecutive no-progress play frames before footwork fallback
	static constexpr float k_fNavProgressEps    = 0.01f;  // min per-frame goalward DISPLACEMENT (player motion projected on the goal direction; NOT distance-reduction) counted as progress (m)
	// Independent serve toss above the server (NOT the racket): height ~ the racket's
	// serve-contact reach, small forward offset toward the net where the swing meets it.
	static constexpr float k_fServeTossHeight   = 1.0f;
	static constexpr float k_fServeTossForward  = 0.25f;

	// ======================================================================
	// State
	// ======================================================================
	Zenith_Entity m_xParentEntity;
	Zenith_Entity m_xBall;
	Zenith_Entity m_xNpc[2];
	RenderTest_Tennis::TennisCourt m_xCourt;

	RenderTest_Tennis::PointPhase   m_ePhase = RenderTest_Tennis::POINT_PHASE_WARMUP;
	RenderTest_Tennis::ServeAttempt m_eServeAttempt = RenderTest_Tennis::SERVE_ATTEMPT_FIRST;
	int m_iServer = 0;
	int m_iLastHitter = -1;
	int m_iExpectedReceiver = 1;
	int m_iPendingWinner = 0;
	int m_iBounceCountSinceHit = 0;
	int m_iSettleFrames = 0;        // consecutive frames the live ball has been "dead" on the surface
	int m_iRallyShots = 0;
	uint32_t m_uBallEpoch = 0u;

	bool m_bPrevSampleValid = false;
	bool m_bCrossedNetSinceHit = false;
	bool m_bNavMeshValid = false;
	bool m_bTelemetryActive = false;   // recording this run (NOT serialized); transferred on move

	int  m_aiNavStuckFrames[2] = { 0, 0 };       // P2: consecutive non-advancing play frames
	bool m_abNavFallback[2]    = { false, false }; // P2: this agent switched to footwork this ball
	Zenith_Maths::Vector3 m_axNavLastPos[2] = {   // P2: prev-frame body pos (for goalward-progress)
		Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f) };

	Zenith_Maths::Vector3 m_xPrevBallPos = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xPrevBallVel = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xBallAngVel  = Zenith_Maths::Vector3(0.0f);
	RenderTest_Tennis::TennisRng m_xJitterRng;

	float m_fTimer = 0.0f;
	float m_fServeTimer = 0.0f;
	float m_fShotTimer = 0.0f;
	float m_fPointTimer = 0.0f;

	uint32_t m_auPoints[2] = { 0, 0 };
	uint32_t m_auGames[2]  = { 0, 0 };

	Zenith_NavMesh* m_pxNavMesh = nullptr;
	Zenith_NavMeshAgent* m_apxNavAgents[2] = { nullptr, nullptr };
};
