#pragma once
#include "Core/Zenith_Engine.h"

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Physics/Zenith_Physics.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "UI/Zenith_UICanvas.h"
#include "Zenith_OS_Include.h"
#include "RenderTest/RenderTest_Tennis.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"
#include "RenderTest/Components/RenderTest_GameplayState.h"

#include <cmath>
#include <string>
#include <algorithm>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// Tennis match orchestrator (singleton on the "Tennis_Match" entity).
//
// Owns the physics ball and drives the two NPCs through a rule-correct match:
// serve -> rally -> point -> game -> short set -> restart. The rally is a
// deterministic volley exchange — each shot is a physics launch (velocity +
// gravity) aimed at the opponent's hit point, so the ball is genuinely
// physics-driven while the timing stays controllable enough for the arm-IK to
// meet it. Point outcomes are pre-decided (server-biased) so every game/set
// completes and the restart loop is always reached.
//
// World-anchored score: the current score is CPU-projected from a world anchor
// above the net to screen and drawn as MSDF UI text each frame.
class RenderTest_TennisMatchComponent
{
public:
	RenderTest_TennisMatchComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	}

	void OnStart()
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_xParentEntity.GetEntityID());
		if (pxSceneData)
		{
			m_xBall    = pxSceneData->FindEntityByName("Tennis_Ball");
			m_xNpc[0]  = pxSceneData->FindEntityByName("Tennis_NPC_Near");
			m_xNpc[1]  = pxSceneData->FindEntityByName("Tennis_NPC_Far");
		}
		Zenith_PhysicsBodyID xBody;
		if (TryBallBody(xBody))
		{
			g_xEngine.Physics().SetRestitution(xBody, k_fBallRestitution);
			g_xEngine.Physics().SetFriction(xBody, k_fBallFriction);
		}
		ResetMatch();

		// IK-showcase overrides the normal match (see UpdateShowcase).
		if (RenderTest_GameplayState::s_bTennisIkShowcase)
		{
			m_eState = State::Showcase;
			const char* aszStroke[3] = { "serve", "forehand", "backhand" };
			Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] IK SHOWCASE: %s",
				aszStroke[glm::clamp(RenderTest_GameplayState::s_iTennisShowcaseStroke, 0, 2)]);
		}
	}

	void OnUpdate(float fDt)
	{
		switch (m_eState)
		{
		case State::Warmup:    UpdateWarmup(fDt);    break;
		case State::Serving:   UpdateServing(fDt);   break;
		case State::Rally:     UpdateRally(fDt);     break;
		case State::PointOver: UpdatePointOver(fDt); break;
		case State::MatchOver: UpdateMatchOver(fDt); break;
		case State::Showcase:  UpdateShowcase(fDt);  break;
		}
		SubmitScoreText();
	}

	void WriteToDataStream(Zenith_DataStream& xStream) const { const u_int uV = 1; xStream << uV; }
	void ReadFromDataStream(Zenith_DataStream& xStream) { u_int uV = 0; xStream >> uV; }
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("State: %d", static_cast<int>(m_eState));
		ImGui::Text("Points: %u-%u  Games: %u-%u", m_auPoints[0], m_auPoints[1], m_auGames[0], m_auGames[1]);
		ImGui::Text("Server: %d  shot: %d/%d  winner: %d", m_iServer, m_iShotCount, m_iMinRally, m_iPointWinner);
	}
#endif

private:
	enum class State { Warmup, Serving, Rally, PointOver, MatchOver, Showcase };

	// ---- Helpers ---------------------------------------------------------
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

	Zenith_Maths::Vector3 BallVel() const
	{
		Zenith_PhysicsBodyID xBody;
		if (TryBallBody(xBody))
			return g_xEngine.Physics().GetLinearVelocity(xBody);
		return Zenith_Maths::Vector3(0.0f);
	}

	RenderTest_TennisPlayerComponent* Npc(int i) const
	{
		if (m_xNpc[i].IsValid() && m_xNpc[i].HasComponent<RenderTest_TennisPlayerComponent>())
			return &m_xNpc[i].GetComponent<RenderTest_TennisPlayerComponent>();
		return nullptr;
	}

	// Baseline Z (the player's hit line) for side i (0=near, 1=far).
	static float BaselineZ(int i)
	{
		return (i == 0) ? RenderTest_Tennis::fBASELINE_NEAR_Z : RenderTest_Tennis::fBASELINE_FAR_Z;
	}

	// Deterministic LCG so the whole match replays identically.
	uint32_t Rng() { m_uRng = m_uRng * 1664525u + 1013904223u; return m_uRng; }
	float Rng01() { return static_cast<float>((Rng() >> 8) & 0xFFFFFF) / 16777216.0f; }

	// Ballistic launch velocity from xFrom to xTarget over time fT (gravity -Y).
	static Zenith_Maths::Vector3 ComputeArc(const Zenith_Maths::Vector3& xFrom,
		const Zenith_Maths::Vector3& xTarget, float fT)
	{
		constexpr float fG = 9.81f;
		const Zenith_Maths::Vector3 d = xTarget - xFrom;
		return Zenith_Maths::Vector3(d.x / fT, d.y / fT + 0.5f * fG * fT, d.z / fT);
	}

	// A hit point near side i's baseline at racket height, offset in X for footwork.
	Zenith_Maths::Vector3 HitPoint(int i, float fX) const
	{
		using namespace RenderTest_Tennis;
		// Slightly in front of the baseline (toward the net) so the player reaches forward.
		const float fZ = BaselineZ(i) + (i == 0 ? 1.2f : -1.2f);
		return Zenith_Maths::Vector3(fX, fSURFACE_Y + 1.0f, fZ);
	}

	// ---- Match / scoring -------------------------------------------------
	void ResetMatch()
	{
		m_auPoints[0] = m_auPoints[1] = 0;
		m_auGames[0]  = m_auGames[1]  = 0;
		m_iServer = 0;
		m_eState = State::Warmup;
		m_fTimer = 1.5f;
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] === New match ===");
	}

	void StartPoint()
	{
		// Server-biased so service games are usually held (a real server's edge) —
		// matches then trade serve and stay competitive with the occasional break,
		// rather than one side bagel-ing the set. The RNG carries across matches
		// (ResetMatch doesn't reseed) so successive sets vary.
		m_iPointWinner = (Rng01() < 0.64f) ? m_iServer : (1 - m_iServer);
		m_iMinRally    = 1 + static_cast<int>(Rng() % 5u);   // 1..5 shots before the loser errs
		m_iShotCount   = 0;
		m_iHitter      = m_iServer;
		m_bSwingCommanded = false;
		m_fShotTimer   = k_fShotWatchdog;   // per-shot watchdog (see UpdateServing/UpdateRally)

		// Park the ball at the server's serve position (a respawn — physics path),
		// floating with gravity off until contact.
		using namespace RenderTest_Tennis;
		const float fServerZ = BaselineZ(m_iServer) + (m_iServer == 0 ? 0.4f : -0.4f);
		// Toss sits above the server's shoulder (NPC origin fSURFACE_Y+1.05, rig
		// shoulder ~+1.10 above) so the serve reaches UP into the ball instead of
		// collapsing the arm-IK target onto the shoulder.
		const Zenith_Maths::Vector3 xServePos(fCOURT_CX, fSURFACE_Y + 3.0f, fServerZ);
		Zenith_PhysicsBodyID xBody;
		if (TryBallBody(xBody))
		{
			g_xEngine.Physics().SetGravityEnabled(xBody, false);
			g_xEngine.Physics().SetLinearVelocity(xBody, Zenith_Maths::Vector3(0.0f));
			g_xEngine.Physics().TeleportBody(xBody, xServePos);
		}

		// Aim the serve at the receiver's hit point (centre-ish, slight variation).
		const int iReceiver = 1 - m_iServer;
		m_xCurrentAim = HitPoint(iReceiver, fCOURT_CX + (Rng01() - 0.5f) * 4.0f);
		if (RenderTest_TennisPlayerComponent* pxServer = Npc(m_iServer))
			pxServer->RequestServe(m_xCurrentAim);

		m_eState = State::Serving;
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] Point start: server=%d winner=%d minRally=%d",
			m_iServer, m_iPointWinner, m_iMinRally);
	}

	void UpdateWarmup(float fDt)
	{
		// Keep the players in position; hold serve until warmup elapses.
		for (int i = 0; i < 2; i++)
			if (RenderTest_TennisPlayerComponent* p = Npc(i)) p->SetFootworkTargetX(RenderTest_Tennis::fCOURT_CX);
		m_fTimer -= fDt;
		if (m_fTimer <= 0.0f)
			StartPoint();
	}

	void UpdateServing(float fDt)
	{
		// Watchdog: if the serve contact never fires (e.g. a missed animation
		// event), resolve the point so the match never stalls.
		if (Watchdog(fDt))
			return;

		RenderTest_TennisPlayerComponent* pxServer = Npc(m_iServer);
		if (!pxServer)
			return;
		if (pxServer->ConsumeContact())
		{
			m_fShotTimer = k_fShotWatchdog;
			// Serve struck -> first shot toward the receiver. The serve counts as
			// shot 1 (the server's shot).
			LaunchFromBall(m_xCurrentAim, /*isError*/ ShotIsError(m_iServer, 1));
			m_iShotCount = 1;
			m_iHitter = 1 - m_iServer;
			m_bSwingCommanded = false;
			if (ShotIsError(m_iServer, 1))
			{
				m_eState = State::PointOver; m_fTimer = 1.4f;   // serve fault
			}
			else
			{
				m_eState = State::Rally;
			}
		}
	}

	void UpdateRally(float fDt)
	{
		// Watchdog: a netted/stalled ball can leave the hitter's plane unreachable
		// forever (BallApproachingHitter never true), so the swing/ConsumeContact
		// never fire. Force the point to resolve to keep the match progressing.
		if (Watchdog(fDt))
			return;

		RenderTest_TennisPlayerComponent* pxHitter = Npc(m_iHitter);
		if (!pxHitter)
			return;

		const Zenith_Maths::Vector3 xBall = BallPos();

		// Footwork: receiver tracks the ball's X.
		pxHitter->SetFootworkTargetX(xBall.x);

		// Command the swing when the ball is ~contact-lead seconds from crossing
		// the hitter's baseline plane, and the hitter is back in the ready state.
		if (!m_bSwingCommanded && pxHitter->IsReady() && BallApproachingHitter(m_iHitter, xBall))
		{
			const int iNext = m_iHitter; // this hitter is about to hit; pick the aim for the RETURN
			const int iOpp = 1 - iNext;
			const bool bErr = ShotIsError(m_iHitter, m_iShotCount + 1);
			m_xCurrentAim = bErr
				? NetTarget()                                  // error: into the net
				: HitPoint(iOpp, RenderTest_Tennis::fCOURT_CX + (Rng01() - 0.5f) * 6.0f);
			pxHitter->RequestSwing(m_xCurrentAim, xBall.x);
			m_bSwingCommanded = true;
		}

		if (pxHitter->ConsumeContact())
		{
			m_iShotCount++;
			const bool bErr = ShotIsError(m_iHitter, m_iShotCount);
			LaunchFromBall(m_xCurrentAim, bErr);
			if (bErr)
			{
				m_eState = State::PointOver; m_fTimer = 1.4f;
			}
			else
			{
				m_iHitter = 1 - m_iHitter;
				m_bSwingCommanded = false;
				m_fShotTimer = k_fShotWatchdog;   // fresh shot, fresh watchdog
			}
		}
	}

	// Per-shot watchdog: returns true (and resolves the point) when a shot has
	// taken too long — i.e. an expected contact event never arrived because the
	// ball netted/stalled/went wide. The point goes to its pre-decided winner, so
	// the match always progresses and the restart loop is always reached.
	bool Watchdog(float fDt)
	{
		m_fShotTimer -= fDt;
		if (m_fShotTimer > 0.0f)
			return false;
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] shot watchdog fired -> resolving point to %d", m_iPointWinner);
		m_eState = State::PointOver;
		m_fTimer = 1.0f;
		return true;
	}

	void UpdatePointOver(float fDt)
	{
		m_fTimer -= fDt;
		if (m_fTimer > 0.0f)
			return;
		AwardPoint(m_iPointWinner);
	}

	void UpdateMatchOver(float fDt)
	{
		m_fTimer -= fDt;
		if (m_fTimer <= 0.0f)
			ResetMatch();
	}

	// IK showcase: the near player repeatedly performs one chosen stroke against a
	// ball frozen at the stroke's contact point, so the arm-IK visibly places the
	// racket on the ball. No scoring — purely for inspecting the IK.
	void UpdateShowcase(float)
	{
		RenderTest_TennisPlayerComponent* pxNpc = Npc(0);   // near player
		if (!pxNpc)
			return;
		pxNpc->SetFootworkTargetX(RenderTest_Tennis::fCOURT_CX);   // hold position
		pxNpc->SetIKShowcaseHold(true);   // pin the racket head on the ball every frame

		Zenith_Maths::Vector3 xNpcPos(0.0f);
		if (m_xNpc[0].IsValid() && m_xNpc[0].HasComponent<Zenith_TransformComponent>())
			m_xNpc[0].GetComponent<Zenith_TransformComponent>().GetPosition(xNpcPos);

		// Contact point for the chosen stroke, relative to the player (faces +Z).
		// Forehand is on the +X side, backhand crosses to -X, serve is overhead.
		const int iStroke = RenderTest_GameplayState::s_iTennisShowcaseStroke;
		Zenith_Maths::Vector3 xContact;
		switch (iStroke)
		{
		case 1:  xContact = xNpcPos + Zenith_Maths::Vector3( 0.45f, 0.45f, 0.60f); break;  // forehand
		case 2:  xContact = xNpcPos + Zenith_Maths::Vector3(-0.30f, 0.50f, 0.55f); break;  // backhand
		// Serve contact is well above the shoulder (rig shoulder ~model Y 1.10) so the
		// arm extends UP to it; a lower toss collapses the IK target onto the shoulder.
		default: xContact = xNpcPos + Zenith_Maths::Vector3( 0.25f, 1.95f, 0.15f); break;  // serve
		}

		// Freeze the ball at the contact point (stable IK target, stays visible at
		// the racket). The player's arm-IK reads this live position during the swing.
		Zenith_PhysicsBodyID xBody;
		if (TryBallBody(xBody))
		{
			g_xEngine.Physics().SetGravityEnabled(xBody, false);
			g_xEngine.Physics().SetLinearVelocity(xBody, Zenith_Maths::Vector3(0.0f));
			g_xEngine.Physics().TeleportBody(xBody, xContact);
		}

		// Re-swing whenever the player is back to ready; aim across the net so the
		// end-effector IK squares the racket face forward.
		if (pxNpc->IsReady())
		{
			const Zenith_Maths::Vector3 xAim = HitPoint(1, RenderTest_Tennis::fCOURT_CX);
			if (iStroke == 0)
				pxNpc->RequestServe(xAim);
			else
				pxNpc->RequestSwing(xAim, xContact.x);
		}
		pxNpc->ConsumeContact();   // swallow contact so the ball isn't launched
	}

	// True when side iShooter's shot number iShot should be the deciding error
	// (the loser's first shot at or after the pre-decided rally length).
	bool ShotIsError(int iShooter, int iShot) const
	{
		const int iLoser = 1 - m_iPointWinner;
		return (iShooter == iLoser) && (iShot >= m_iMinRally);
	}

	// Net target for an error shot: short, into the net plane.
	Zenith_Maths::Vector3 NetTarget() const
	{
		using namespace RenderTest_Tennis;
		return Zenith_Maths::Vector3(fCOURT_CX, fSURFACE_Y + 0.2f, fCOURT_CZ);
	}

	// Launch the ball from its current position toward xTarget (a real "hit":
	// velocity change at the ball's position, not a teleport). Re-enables gravity
	// (the serve had it off).
	void LaunchFromBall(const Zenith_Maths::Vector3& xTarget, bool /*bIsError*/)
	{
		Zenith_PhysicsBodyID xBody;
		if (!TryBallBody(xBody))
			return;
		const Zenith_Maths::Vector3 xFrom = BallPos();
		const float fDist = glm::length(xTarget - xFrom);
		const float fT = glm::clamp(fDist / 14.0f, 0.8f, 1.7f);
		g_xEngine.Physics().SetGravityEnabled(xBody, true);
		g_xEngine.Physics().SetLinearVelocity(xBody, ComputeArc(xFrom, xTarget, fT));
	}

	// The ball will cross side i's baseline plane within the contact-lead window
	// and is moving toward that side.
	bool BallApproachingHitter(int i, const Zenith_Maths::Vector3& xBall) const
	{
		const float fPlaneZ = BaselineZ(i) + (i == 0 ? 1.2f : -1.2f);
		const float fVz = BallVel().z;
		const float fToward = (fPlaneZ - xBall.z);           // signed distance to the plane
		if (std::fabs(fVz) < 0.5f)
			return false;
		const float fTime = fToward / fVz;                   // time to reach the plane
		return fTime > 0.0f && fTime < k_fContactLead;
	}

	void AwardPoint(int iWinner)
	{
		m_auPoints[iWinner]++;
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] Point -> %d  (%u-%u)", iWinner, m_auPoints[0], m_auPoints[1]);

		// Game won? >=4 points and a 2-point lead (deuce/advantage), with a hard
		// deuce cap so a game always resolves.
		const uint32_t a = m_auPoints[0], b = m_auPoints[1];
		// Sudden-death only once a leader exists, so the cap never awards a tied game.
		const bool bDeuceCap = (a + b) >= 12 && a != b;
		const bool bGame = (a >= 4 || b >= 4) && (std::max(a, b) - std::min(a, b) >= 2 || bDeuceCap);
		if (bGame)
		{
			const int iGameWinner = (a > b) ? 0 : 1;
			m_auGames[iGameWinner]++;
			m_auPoints[0] = m_auPoints[1] = 0;
			m_iServer = 1 - m_iServer;   // alternate serve each game
			Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] Game -> %d  (games %u-%u)",
				iGameWinner, m_auGames[0], m_auGames[1]);

			// Short set: first to 4 games, win by 2; hard cap at 6 so it always ends.
			const uint32_t ga = m_auGames[0], gb = m_auGames[1];
			const bool bSet = (ga >= 4 || gb >= 4) && (std::max(ga, gb) - std::min(ga, gb) >= 2 || std::max(ga, gb) >= 6);
			if (bSet)
			{
				const int iSetWinner = (ga > gb) ? 0 : 1;
				Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[Tennis] === Match over: side %d wins the set %u-%u ===",
					iSetWinner, ga, gb);
				m_eState = State::MatchOver;
				m_fTimer = 3.0f;
				return;
			}
		}

		// Next point.
		StartPoint();
	}

	// ---- Score display ---------------------------------------------------
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

	// CPU-project a world anchor above the net to screen and draw the score.
	void SubmitScoreText()
	{
		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (!pxCanvas)
			return;

		using namespace RenderTest_Tennis;
		const Zenith_Maths::Vector3 xAnchor(fCOURT_CX, fSURFACE_Y + 4.5f, fCOURT_CZ);
		const Zenith_Maths::Matrix4 xVP = g_xEngine.FluxGraphics().GetViewProjMatrix();
		const Zenith_Maths::Vector4 xClip = xVP * Zenith_Maths::Vector4(xAnchor, 1.0f);
		if (xClip.w <= 0.0001f)
			return;   // behind the camera

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

	static constexpr float k_fBallRestitution = 0.75f;
	static constexpr float k_fBallFriction    = 0.25f;
	static constexpr float k_fContactLead     = 0.40f;   // s before plane-cross to start the swing
	static constexpr float k_fShotWatchdog    = 3.5f;    // s before a stuck shot is force-resolved

	Zenith_Entity m_xParentEntity;
	Zenith_Entity m_xBall;
	Zenith_Entity m_xNpc[2];

	State m_eState = State::Warmup;
	float m_fTimer = 0.0f;

	uint32_t m_auPoints[2] = { 0, 0 };
	uint32_t m_auGames[2]  = { 0, 0 };
	int  m_iServer = 0;

	int  m_iPointWinner = 0;
	int  m_iMinRally = 1;
	int  m_iShotCount = 0;
	int  m_iHitter = 0;
	float m_fShotTimer = 0.0f;
	bool m_bSwingCommanded = false;
	Zenith_Maths::Vector3 m_xCurrentAim = Zenith_Maths::Vector3(0.0f);

	uint32_t m_uRng = 0x2468ACEu;
};
