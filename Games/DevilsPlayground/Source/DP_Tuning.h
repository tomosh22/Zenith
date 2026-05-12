#pragma once

// ============================================================================
// DP_Tuning — runtime accessor for Devil's Playground tuning constants.
//
// Loads Games/DevilsPlayground/Config/Tuning.json once at boot and exposes
// every leaf number/bool via flat-dotted keys (e.g.
// "possession.life_timer_default_s"). Keys whose component starts with '_'
// (json comments / metadata) are skipped during the recursive flatten.
//
// Asserts on miss (key not in JSON) and on type mismatch (querying bool
// against a number, or vice versa). The Get<T>() accessors are explicit
// specializations — only float / int / bool are supported.
// ============================================================================
namespace DP_Tuning
{
	// Parse Config/Tuning.json into the in-process cache. Idempotent —
	// repeated calls after the first are no-ops.
	void Initialize();

	// Drop the cache. Idempotent — safe to call without a prior Initialize().
	void Shutdown();

	// Read a flat-dotted key. Asserts on miss (key not in JSON) or
	// type mismatch (querying bool against a number, etc.).
	template<typename T> T Get(const char* szDottedKey);

	// Explicit specializations — defined in DP_Tuning.cpp.
	template<> float Get<float>(const char* szDottedKey);
	template<> int   Get<int>  (const char* szDottedKey);
	template<> bool  Get<bool> (const char* szDottedKey);
}
