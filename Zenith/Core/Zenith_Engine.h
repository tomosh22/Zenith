#pragma once

#include <type_traits>

// Forward decls -- keep this header light. Full subsystem headers are
// only included by Zenith_Engine.cpp where the accessor bodies live.
class FrameContext;
class Zenith_MultithreadingImpl;

// Zenith_Engine is the single owner of the engine's mutable runtime
// state. Phase 0 introduces the class and moves the bootstrap ordering
// from Zenith_Core::Zenith_Init / Zenith_Shutdown into
// Zenith_Engine::Initialise / Shutdown — subsystem APIs stay static
// for now; later phases migrate them onto Zenith_Engine as members.
//
// Construction model:
// - g_xEngine is constinit (zero static-init cost; nullptr-default
//   members are constant expressions).
// - Default ctor/dtor are trivial. All subsystem ownership / teardown
//   work runs explicitly in Initialise() / Shutdown(). NEVER add a
//   non-trivial destructor — process shutdown must not run static
//   destructors in undefined order vs. other globals.
// - Refactor plan + rationale:
//   ~/.claude/plans/the-zenith-engine-has-playful-canyon.md
class Zenith_Engine
{
public:
	Zenith_Engine() = default;
	~Zenith_Engine() = default;

	Zenith_Engine(const Zenith_Engine&) = delete;
	Zenith_Engine& operator=(const Zenith_Engine&) = delete;
	Zenith_Engine(Zenith_Engine&&) = delete;
	Zenith_Engine& operator=(Zenith_Engine&&) = delete;

	// Bootstrap. Replaces Zenith_Init / Zenith_Shutdown bodies; the
	// free Zenith_Core::Zenith_Init / Zenith_Shutdown are now thin
	// wrappers that forward here.
	void Initialise();
	void Shutdown();

	// Subsystem accessors. Bodies live in Zenith_Engine.cpp where the
	// full subsystem headers are visible; this header only forward-
	// declares the return types.
	FrameContext& Frame();
	Zenith_MultithreadingImpl& Threading();

private:
	// Subsystem members. Raw pointers to forward-declared types so the
	// default ctor/dtor stay trivial and the constinit global has zero
	// static-init cost. Each is allocated in Initialise() and deleted
	// in Shutdown().
	FrameContext*              m_pxFrame     = nullptr;
	Zenith_MultithreadingImpl* m_pxThreading = nullptr;
};

// Compile-time guard: enforce trivial destruction so the
// process-shutdown destructor-order fiasco can't reappear if a
// well-meaning member with a non-trivial dtor is added later.
static_assert(std::is_trivially_destructible_v<Zenith_Engine>,
	"Zenith_Engine must be trivially destructible. Subsystem cleanup "
	"belongs in Zenith_Engine::Shutdown(), not in member dtors that "
	"run at static-destruction time.");

extern Zenith_Engine g_xEngine;
