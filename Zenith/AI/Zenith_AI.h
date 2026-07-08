#pragma once

// Optional engine-driven tick for the AI manager systems (Perception, Squad,
// TacticalPoint).
//
// By DEFAULT these managers are driven by GAME code: each game ticks them from
// its own component in a game-specific order relative to its per-agent AI logic
// (e.g. a game ticks perception/squad/tactical BEFORE its enemy-AI step, so
// the agents act on fresh perception). That ordering is intentional and is why
// the engine does not tick them unconditionally.
//
// A game that has no such ordering constraint can instead opt into a single
// engine-driven tick: call Zenith_AI::SetEngineTickEnabled(true) once at init,
// and the main loop will call Zenith_AI::Update(dt) each frame after the scene
// update (and only while game logic is running). Do NOT also tick the managers
// from game code when this is enabled — that would double-update them.
namespace Zenith_AI
{
	// Tick perception -> squad coordination -> tactical-point scoring, in that
	// canonical order. Safe to call directly; the main loop calls it when the
	// engine tick is enabled.
	void Update(float fDt);

	// Opt-in toggle (default false). When true, Zenith_Core::Zenith_MainLoop ticks
	// the AI managers via Update() each game-logic frame.
	void SetEngineTickEnabled(bool bEnabled);
	bool IsEngineTickEnabled();
}
