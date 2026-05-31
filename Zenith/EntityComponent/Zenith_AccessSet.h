#pragma once

// ============================================================================
// Zenith_AccessSet — WS12 parallel-simulation conflict model (header-only)
//
// This header turns the INERT per-component access-set metadata (the
// m_uReads / m_uWrites bitmasks populated at registration from each
// component's optional DeclareAccess — see Zenith_ComponentMeta.h) into the
// conservative eligibility decision the gated parallel-sim dispatcher uses.
//
// Nothing here runs unless the WS12 gate is ON
// (g_xEngine.Scenes().AreParallelSimEnabled()); with the gate OFF the
// chokepoint runs its existing serial loop verbatim and never calls into this
// header. The math is intentionally tiny and side-effect-free so it can stay
// header-only (no Sharpmake regen) and be unit-tested directly.
//
// CONSERVATISM IS THE WHOLE POINT. The default-OFF mechanism only ever runs
// ONE proven-safe parallel unit (collider-free / script-free Tween-only
// entities). Every ambiguity resolves to "serial". A wrong "eligible" verdict
// is a silent data race; a wrong "ineligible" verdict merely costs a little
// parallelism. We always err toward ineligible.
//
//   * A component type that declares NO access (m_uReads == 0 &&
//     m_uWrites == 0) is treated as uACCESS_UNKNOWN — "touches everything",
//     conflicts with everything — NOT "touches nothing". Un-annotated == we
//     don't know == serial. This keeps ~11/12 component types serial today.
//
//   * Only components that ACTUALLY RUN a per-frame hook (OnUpdate /
//     OnLateUpdate) contribute to an entity's aggregate mask, mirroring the
//     dispatch loops in Zenith_ComponentMetaRegistry::DispatchOnUpdate /
//     DispatchOnLateUpdate exactly. A component with no update hook performs no
//     per-frame work, so it cannot create a per-frame race.
//
// The collider / script guards (the hidden Tween->Jolt physics-write edge and
// the open-ended ScriptComponent surface) are applied by the CALLER, which has
// the concrete component headers; this header only does the mask arithmetic so
// it carries no extra include dependencies beyond the meta registry.
// ============================================================================

#include "EntityComponent/Zenith_ComponentMeta.h"

namespace Zenith_AccessSet
{
	// uACCESS_UNKNOWN = all reads + all writes set. An UNKNOWN component reads
	// and writes every domain, so ConflictsWith(UNKNOWN, anything-non-zero) is
	// always true and any entity whose aggregate mask contains the UNKNOWN bits
	// is forced serial. 0xFFFFFFFF deliberately includes the WRITES_PHYSICS bit
	// etc., so it can never be a subset of {READS_TRANSFORM|WRITES_TRANSFORM}.
	static constexpr u_int uACCESS_UNKNOWN = 0xFFFFFFFFu;

	// The ONLY domain bits a parallel-eligible entity may touch this wave:
	// Transform reads + Transform writes (the Tween output). Mirrors the raw bit
	// contract documented on Zenith_ComponentAccess / Zenith_TransformComponent:
	//     READS_TRANSFORM  = 1u << 0
	//     WRITES_TRANSFORM = 1u << 1
	static constexpr u_int uACCESS_TRANSFORM_ONLY =
		static_cast<u_int>(Zenith_ComponentAccess::READS_TRANSFORM) |
		static_cast<u_int>(Zenith_ComponentAccess::WRITES_TRANSFORM);

	// ConflictsWith relies on each domain's WRITE bit being its READ bit shifted
	// left by one (READS_TRANSFORM=1<<0 / WRITES_TRANSFORM=1<<1, etc.). Pin that
	// layout so a future enum edit can't silently break write-vs-read detection.
	static_assert(static_cast<u_int>(Zenith_ComponentAccess::WRITES_TRANSFORM) ==
	              (static_cast<u_int>(Zenith_ComponentAccess::READS_TRANSFORM) << 1),
	              "Zenith_AccessSet: WRITES_TRANSFORM must equal READS_TRANSFORM << 1");
	static_assert(static_cast<u_int>(Zenith_ComponentAccess::WRITES_PHYSICS) ==
	              (static_cast<u_int>(Zenith_ComponentAccess::READS_PHYSICS) << 1),
	              "Zenith_AccessSet: WRITES_PHYSICS must equal READS_PHYSICS << 1");

	// Conflict predicate over two access pairs. Two units conflict iff one writes
	// a domain the other reads or writes (write-write, write-read, read-write);
	// read-read never conflicts. Read bits and write bits occupy DIFFERENT
	// positions per domain (write == read << 1, pinned above), so to detect a
	// write-vs-read hazard on the SAME domain we align each side's write bits DOWN
	// onto their read positions (>> 1) before ANDing against the other's reads —
	// without the shift, (writes & reads) compares different domains and misses
	// every writer-vs-reader conflict. The future full scheduler reuses this; the
	// WS12 dispatcher needs only the single-entity eligibility test below (every
	// eligible entity is mutually disjoint by construction).
	inline bool ConflictsWith(u_int uReadsA, u_int uWritesA, u_int uReadsB, u_int uWritesB)
	{
		return ((uWritesA & uWritesB)        |   // write-write (same write bits)
		        ((uWritesA >> 1) & uReadsB)  |   // A writes a domain B reads
		        ((uWritesB >> 1) & uReadsA)) != 0u; // B writes a domain A reads
	}

	// The effective read mask of a registered component meta: its declared
	// reads, OR uACCESS_UNKNOWN when it declared NOTHING (both masks 0). The
	// "declared nothing" test must look at BOTH masks together — a component
	// that legitimately only writes (writes != 0, reads == 0) has declared
	// something and must NOT be promoted to UNKNOWN.
	inline u_int EffectiveReads(const Zenith_ComponentMeta& xMeta)
	{
		if (xMeta.m_uReads == 0u && xMeta.m_uWrites == 0u) return uACCESS_UNKNOWN;
		return xMeta.m_uReads;
	}

	inline u_int EffectiveWrites(const Zenith_ComponentMeta& xMeta)
	{
		if (xMeta.m_uReads == 0u && xMeta.m_uWrites == 0u) return uACCESS_UNKNOWN;
		return xMeta.m_uWrites;
	}

	// True iff the component meta runs a per-frame hook (OnUpdate or
	// OnLateUpdate). Only such components do per-frame work and can therefore
	// race. A component present on the entity but with no update hook (e.g. a
	// plain data component) contributes nothing to the aggregate mask — exactly
	// as it contributes nothing to the per-frame dispatch loops.
	inline bool RunsPerFrameHook(const Zenith_ComponentMeta& xMeta)
	{
		return xMeta.m_pfnOnUpdate != nullptr || xMeta.m_pfnOnLateUpdate != nullptr;
	}

	// Per-entity aggregate access mask = OR of the effective (reads|writes)
	// masks of every component the entity HAS that ALSO runs a per-frame hook.
	// Iterates the same registered-meta list, and uses the same
	// m_pfnHasComponent probe, as the dispatch loops — so what we reason about
	// is exactly what will execute per frame. The combined reads|writes is
	// sufficient for the subset eligibility test (we only ask "does this entity
	// touch anything outside Transform?"); the separated reads/writes are
	// available via ConflictsWith for the future pairwise scheduler.
	//
	// xEntity is taken by non-const ref because m_pfnHasComponent (the
	// type-erased ComponentHasWrapper) takes Zenith_Entity&. It does not mutate
	// the entity.
	inline u_int ComputeEntityUpdateAccessMask(const Zenith_ComponentMetaRegistry& xRegistry,
	                                            Zenith_Entity& xEntity)
	{
		u_int uMask = 0u;
		const std::vector<const Zenith_ComponentMeta*>& xMetas = xRegistry.GetAllMetasSorted();
		for (const Zenith_ComponentMeta* pxMeta : xMetas)
		{
			if (pxMeta == nullptr) continue;
			if (!RunsPerFrameHook(*pxMeta)) continue;
			if (pxMeta->m_pfnHasComponent == nullptr) continue;
			if (!pxMeta->m_pfnHasComponent(xEntity)) continue;

			uMask |= EffectiveReads(*pxMeta);
			uMask |= EffectiveWrites(*pxMeta);
		}
		return uMask;
	}

	// Mask-side eligibility test: the entity's aggregate update mask is a subset
	// of {READS_TRANSFORM|WRITES_TRANSFORM}. Any bit outside that set (UNKNOWN,
	// PHYSICS, ...) makes the entity ineligible. An entity with NO per-frame
	// hooks has an empty mask (0), which is trivially a subset — but such an
	// entity does no per-frame work anyway, so the dispatcher gains nothing by
	// parallelising it; the caller additionally requires the entity to be a
	// genuine Tween candidate (it has the collider/script guards). Note: empty
	// mask passes here by design; the collider/script guards + the "is actually
	// doing tween work" reality are the caller's complete gate.
	inline bool MaskIsParallelEligible(u_int uAggregateMask)
	{
		return (uAggregateMask & ~uACCESS_TRANSFORM_ONLY) == 0u;
	}
}
