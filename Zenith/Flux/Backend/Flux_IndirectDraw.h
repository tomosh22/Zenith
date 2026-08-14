#pragma once

// ============================================================================
// Flux_IndirectDraw.h — backend-neutral indirect-count capability & policy.
//
// A dependency-light leaf (only <cstdint>/<cstddef>) owned by Flux so the
// engine, every backend concept, the command-recorder surface, tests and the
// CLI converter can all name these types without re-entering the Flux.h or
// Vulkan/D3D12/Null header cycles. Vulkan stays out of this header on
// purpose: the per-backend negotiation (core 1.2 vs VK_KHR_draw_indirect_count,
// advertised vs enabled vs entry-point-resolved) is its own concern and is
// surfaced here only as the USABLE semantic booleans a backend reports.
//
// The contract this header pins, in order of load-bearing-ness:
//
//   1. The "zero-padded to max" promise. A caller that passes
//      ZERO_PADDED_TO_MAX guarantees every record in [0, uMaxDrawCount) is
//      a valid zero/no-op every frame, so a backend without native count may
//      legally execute the full range with fixed indexed-indirect draws even
//      when the live prefix is shorter. A REQUIRE_NATIVE caller refuses that
//      fallback; the backend must fail closed rather than silently drop the
//      draw or replay a stale tail.
//
//   2. Native count is selected only when its preconditions hold: usable
//      entry point, request inside the reported native limit, and override
//      permitting it. multiDrawIndirect does not gate the counted command.
//      An over-limit request is NEVER split across the same global count
//      buffer — each split batch would re-read the full count and over-
//      draw — so the selector chooses padded fixed batches when the caller
//      permits them.
//
//   3. The padding tiers honour Vulkan rules: PADDED_MULTI uses batches no
//      larger than maxDrawIndirectCount (and no larger than 1 when multi-
//      draw is unavailable, regardless of the raw physical-device limit);
//      PADDED_SINGLE uses drawCount == 1 per call.
//
//   4. The CLI override is boot-time immutable. AUTO is the shipping default
//      and may lower the tier; NATIVE/PADDED/SINGLE are test assertions —
//      they fail closed when the requested tier cannot legally run, rather
//      than silently choosing a different one.
//
// Terrain is the primary (and currently only) caller. The persistent-per-
// terrain indirect buffer is cleared to zero every frame by a dedicated GPU
// reset pass before culling writes the live prefix, so terrain passes
// ZERO_PADDED_TO_MAX. See Docs/design/TerrainIndirectCountFallback.md.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <limits>

// ----------------------------------------------------------------------------
// Caller policy for a semantic counted-indirect draw.
// ----------------------------------------------------------------------------
enum class Flux_IndirectCountFallback : uint8_t
{
	// The caller refuses a fixed-draw fallback; native count is mandatory. The
	// backend must fail closed (assert + emit no draw) when its usable native
	// count preconditions do not hold, rather than silently executing a fixed
	// range. Used by callers that cannot prove a zero/no-op tail.
	REQUIRE_NATIVE,

	// The caller guarantees that every record in [0, uMaxDrawCount) is a
	// valid zero/no-op every frame, so a fixed indexed-indirect draw over the
	// full range is valid even when the count buffer's value is smaller. The
	// backend must NEVER call a null function pointer and must never replay a
	// stale tail — the caller's invariant is the only thing that makes the
	// padding legal.
	ZERO_PADDED_TO_MAX,
};

// ----------------------------------------------------------------------------
// Effective backend execution mode for one semantic count request.
// ----------------------------------------------------------------------------
enum class Flux_IndirectExecutionMode : uint8_t
{
	// One native vkCmdDrawIndexedIndirectCount[KHR] call — the fast path on
	// devices that support it and when the request fits the reported native
	// limit. This mode does not require multiDrawIndirect.
	NATIVE_COUNT,

	// vkCmdDrawIndexedIndirect in batches no larger than the device's legal
	// multi-draw per-call limit. Each batch covers a contiguous slice of the
	// padded [0, uMaxDrawCount) range exactly once.
	PADDED_MULTI,

	// One vkCmdDrawIndexedIndirect call per record (drawCount == 1), looping
	// over the padded [0, uMaxDrawCount) range. The legal tier when multi-
	// draw is unavailable or when the CLI override forces it.
	PADDED_SINGLE,

	// No valid execution path. The backend must emit no API command; a
	// REQUIRE_NATIVE caller whose native preconditions failed or an explicit
	// NATIVE override on unsupported hardware lands here. Recorder code MUST
	// treat this as a hard failure (assert/log) and never fall through to any
	// draw call.
	FAILED_CLOSED,
};

// ----------------------------------------------------------------------------
// Stable, copyable record of the SEMANTIC graphics capability a backend reports.
//
// These are graphics semantics, not Vulkan API names. Each backend keeps
// advertised (device-enumerated), enabled (requested at device creation), and
// usable (enabled && entry point resolved && not downgraded by a broken ICD)
// state DISTINCT in its own negotiation/log layer; this struct reports the
// USABLE semantic booleans only. The override may change the effective
// execution mode but never falsifies these fields.
// ----------------------------------------------------------------------------
struct Flux_IndirectDrawCapabilities
{
	// vkCmdDrawIndexedIndirectCount[KHR] is the enabled route AND its entry
	// point resolved to a non-null function pointer. False on Vulkan 1.0
	// hardware, on drivers that advertise the feature but yield a null proc
	// address, or on the Null/D3D12 no-op backends (which never execute draws).
	bool m_bNativeIndexedIndirectCount = false;

	// multiDrawIndirect enabled at device creation. This gates only fixed
	// vkCmdDraw[Indexed]Indirect calls; native counted draws are independent.
	// When false, padded fallback therefore uses PADDED_SINGLE.
	bool m_bMultiDrawIndirect = false;

	// drawIndirectFirstInstance enabled. Terrain's firstInstance carries the
	// stable chunk index that the vertex shader consumes via
	// SV_StartInstanceLocation; this is a documented terrain minimum.
	bool m_bIndirectFirstInstance = false;

	// shaderDrawParameters enabled. The terrain vertex shader reads
	// SV_StartInstanceLocation; this is a documented terrain minimum.
	bool m_bShaderDrawParameters = false;

	// Raw VkPhysicalDeviceLimits::maxDrawIndirectCount. The native selector uses
	// this reported limit independently of multiDrawIndirect. Fixed emission
	// MUST call Flux_ResolveFixedDrawPerCallLimit(), which clamps to one when
	// multiDrawIndirect is disabled.
	uint32_t m_uMaxDrawIndirectCount = 1;
};

// ----------------------------------------------------------------------------
// Boot-time immutable CLI override. Parsed by Core (Zenith_CommandLine) and
// converted to this enum by the backend at device initialisation time.
// ----------------------------------------------------------------------------
enum class Flux_IndirectDrawOverride : uint8_t
{
	// Shipping default. May choose NATIVE_COUNT when its preconditions hold,
	// otherwise falls back to the legal padded tier the caller's policy
	// allows.
	AUTO,

	// Test assertion: requests NATIVE_COUNT. Fails closed if any native
	// precondition is missing. NEVER silently downgrades to padded.
	NATIVE,

	// Forbids native count. Selects PADDED_MULTI when multi-draw is
	// available, otherwise PADDED_SINGLE. Fails closed if the caller's policy
	// is REQUIRE_NATIVE (the override cannot violate the caller's promise).
	PADDED,

	// Forces PADDED_SINGLE regardless of multi-draw availability. Intended
	// for diagnostics/CI. Fails closed if the caller's policy is
	// REQUIRE_NATIVE.
	SINGLE,
};

// ----------------------------------------------------------------------------
// One fixed indexed-indirect API call covers one (offset, drawCount) slice of
// the padded [0, uMaxDrawCount) range. The batch planner produces these.
// ----------------------------------------------------------------------------
struct Flux_IndirectDrawBatch
{
	uint64_t m_ulArgumentsOffsetBytes = 0;
	uint32_t m_uDrawCount = 0;
};

// ----------------------------------------------------------------------------
// Pure mode selector.
//
//   Receives usable semantic booleans (the backend reports them with
//   advertised/enabled/usable already resolved) and so cannot distinguish
//   Vulkan 1.2 core from the KHR route — that is the backend's own concern.
//
//   Returns FAILED_CLOSED when no legal execution path exists:
//     - REQUIRE_NATIVE + native unavailable;
//     - NATIVE override with any failed native precondition;
//     - PADDED/SINGLE override with REQUIRE_NATIVE policy (the override
//       cannot violate the caller's promise);
//     - PADDED/SINGLE override on a backend that reports no padded fallback
//       (impossible for the current surface because PADDED_SINGLE needs no
//       optional feature, but the contract is what it is).
//
//   The recorder MUST NOT emit any API command when FAILED_CLOSED is returned;
//   a hard assert/log is the only legal response.
// ----------------------------------------------------------------------------
inline Flux_IndirectExecutionMode Flux_SelectIndirectExecutionMode(
	const Flux_IndirectDrawCapabilities& xCaps,
	uint32_t uMaxDrawCount,
	Flux_IndirectCountFallback eFallback,
	Flux_IndirectDrawOverride eOverride)
{
	// Native preconditions. The engine treats maxDrawIndirectCount as the
	// request's safe native upper bound because the count producer may write any
	// value through uMaxDrawCount. multiDrawIndirect gates only fixed indirect
	// commands and must not influence this decision.
	const bool bNativeFitsLimit = uMaxDrawCount <= xCaps.m_uMaxDrawIndirectCount;
	const bool bOverrideAllowsNative = (eOverride == Flux_IndirectDrawOverride::AUTO) ||
	                                   (eOverride == Flux_IndirectDrawOverride::NATIVE);
	const bool bNativePreconditions =
		xCaps.m_bNativeIndexedIndirectCount &&
		bNativeFitsLimit &&
		bOverrideAllowsNative;

	// An explicit NATIVE override is a TEST ASSERTION, not a preference: any
	// failed precondition fails closed rather than silently choosing padded.
	if (eOverride == Flux_IndirectDrawOverride::NATIVE)
	{
		return bNativePreconditions ? Flux_IndirectExecutionMode::NATIVE_COUNT
		                            : Flux_IndirectExecutionMode::FAILED_CLOSED;
	}

	// PADDED forbids native count; SINGLE forces one-record calls. Both fail
	// closed if the caller's policy refuses the fallback (the override cannot
	// downgrade a REQUIRE_NATIVE caller's promise).
	if (eOverride == Flux_IndirectDrawOverride::PADDED)
	{
		if (eFallback != Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX)
			return Flux_IndirectExecutionMode::FAILED_CLOSED;
		return xCaps.m_bMultiDrawIndirect ? Flux_IndirectExecutionMode::PADDED_MULTI
		                                  : Flux_IndirectExecutionMode::PADDED_SINGLE;
	}
	if (eOverride == Flux_IndirectDrawOverride::SINGLE)
	{
		if (eFallback != Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX)
			return Flux_IndirectExecutionMode::FAILED_CLOSED;
		return Flux_IndirectExecutionMode::PADDED_SINGLE;
	}

	// AUTO: choose the legal tier. NATIVE_COUNT wins when available; else
	// PADDED_MULTI when policy allows pad and multi-draw is on; else
	// PADDED_SINGLE when policy allows pad; else FAILED_CLOSED.
	if (bNativePreconditions)
		return Flux_IndirectExecutionMode::NATIVE_COUNT;

	if (eFallback == Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX)
	{
		if (xCaps.m_bMultiDrawIndirect)
			return Flux_IndirectExecutionMode::PADDED_MULTI;
		return Flux_IndirectExecutionMode::PADDED_SINGLE;
	}

	// REQUIRE_NATIVE without a usable native path.
	return Flux_IndirectExecutionMode::FAILED_CLOSED;
}

// ----------------------------------------------------------------------------
// A small stateful iterator over a fixed-batch plan. Header-only, no heap.
//
// Reset() seeds a plan; Next() emits one batch per call until the plan is
// exhausted (returns false). The plan covers [0, uRequestedDrawCount) records
// exactly once; uPerCallLimit is the legal per-call drawCount (1 for
// PADDED_SINGLE; maxDrawIndirectCount-clamped value for PADDED_MULTI). The
// cursor advances by m_uStrideBytes * batchDrawCount. Reset preflights the
// complete plan so no prefix can be emitted before an overflow is discovered.
//
// uRequestedDrawCount == 0 produces an empty plan (the first Next() returns
// false). An illegal uPerCallLimit of 0 also produces an empty plan because
// no batch the planner could emit would satisfy the ">= 1 draw per batch"
// contract.
//
// uArgumentsOffsetBytes is the caller's starting offset into the argument
// buffer (terrain passes 0). Batches accumulate stride*drawCount from there.
// ----------------------------------------------------------------------------
struct Flux_IndirectDrawBatchPlan
{
	bool Reset(uint32_t uRequestedDrawCount,
		uint64_t ulArgumentsOffsetBytes,
		uint32_t uStrideBytes,
		uint32_t uPerCallLimit)
	{
		m_uStrideBytes       = uStrideBytes;
		m_uPerCallLimit      = uPerCallLimit;
		m_uRemainingRecords  = 0;
		m_ulNextOffsetBytes  = ulArgumentsOffsetBytes;
		m_bOverflowed         = false;

		// A zero request is a valid empty plan. A zero limit or stride cannot
		// describe any non-empty legal plan.
		if (uRequestedDrawCount == 0u)
		{
			return true;
		}
		if (uPerCallLimit == 0u || (uRequestedDrawCount > 1u && uStrideBytes == 0u))
		{
			m_bOverflowed = true;
			return false;
		}

		// Preflight the complete plan before the recorder can emit its first
		// API command. Every batch starts at one record in the requested range,
		// so the last possible start offset must fit VkDeviceSize. (The caller
		// separately validates the final record's byte range against the buffer.)
		const uint64_t ulLastRecordIndex = static_cast<uint64_t>(uRequestedDrawCount - 1u);
		if (uRequestedDrawCount > 1u &&
			ulLastRecordIndex >
			(std::numeric_limits<uint64_t>::max() - ulArgumentsOffsetBytes) /
			static_cast<uint64_t>(uStrideBytes))
		{
			m_bOverflowed = true;
			return false;
		}
		m_uRemainingRecords = uRequestedDrawCount;
		return true;
	}

	// Emit the next preflighted batch. Returns false on exhaustion; an invalid
	// plan was already rejected atomically by Reset(), which leaves no records
	// available for emission.
	bool Next(Flux_IndirectDrawBatch& xOutBatch)
	{
		if (m_uRemainingRecords == 0u || m_uPerCallLimit == 0u)
			return false;

		const uint32_t uBatch = (m_uRemainingRecords < m_uPerCallLimit)
			? m_uRemainingRecords
			: m_uPerCallLimit;

		// Reset preflighted every batch start. Advance in VkDeviceSize-width
		// arithmetic; after the final batch the cursor is not used again, so a
		// request whose conceptual one-past-end is 2^64 remains representable
		// as long as its final record start and accessed bytes were valid.
		const uint64_t ulAdvance = static_cast<uint64_t>(uBatch) * static_cast<uint64_t>(m_uStrideBytes);

		xOutBatch.m_ulArgumentsOffsetBytes = m_ulNextOffsetBytes;
		xOutBatch.m_uDrawCount = uBatch;

		m_uRemainingRecords -= uBatch;
		if (m_uRemainingRecords > 0u)
		{
			m_ulNextOffsetBytes += ulAdvance;
		}
		return true;
	}

	// True iff Reset() rejected a non-empty plan because its parameters were
	// invalid or a batch start would overflow uint64_t. Diagnostic only.
	bool HasOverflowed() const { return m_bOverflowed; }

	uint32_t m_uStrideBytes       = 0;
	uint32_t m_uPerCallLimit      = 0;
	uint32_t m_uRemainingRecords  = 0;
	uint64_t m_ulNextOffsetBytes  = 0;
	bool     m_bOverflowed        = false;
};

// ----------------------------------------------------------------------------
// Resolve the EFFECTIVE per-call fixed-draw limit for the PADDED_MULTI tier
// given usable semantic capabilities. When multi-draw is enabled, the raw
// maxDrawIndirectCount is the fixed per-call limit; when it is disabled only
// one record per fixed call is legal. This clamp is deliberately isolated
// from native counted-mode selection. A forced SINGLE counted draw must not turn unrelated ordinary
// fixed draws into single-record loops — only the PADDED_MULTI caller passes
// its resolved limit to the batch planner; PADDED_SINGLE callers always pass 1.
//
// Public so the recorder can use the same clamp for both counted-emulated
// and ordinary fixed indexed-indirect draws (the latter's hardening is phased
// in separately per the design doc, but the clamp lives here so the two paths
// cannot disagree).
// ----------------------------------------------------------------------------
inline uint32_t Flux_ResolveFixedDrawPerCallLimit(const Flux_IndirectDrawCapabilities& xCaps)
{
	return xCaps.m_bMultiDrawIndirect ? xCaps.m_uMaxDrawIndirectCount : 1u;
}

// ----------------------------------------------------------------------------
// The 20-byte indexed-indirect-command ABI: five 32-bit words. Defined here as
// a stable POD so C++ allocation sizing, the Slang shared include, allocation
// seeding in Zenith_TerrainComponent, and the per-backend recorders all name
// the same contract. The struct is intentionally trivially-copyable; the GPU
// reads it as a raw byte sequence.
//
//   word 0: indexCount    (uint32_t)
//   word 1: instanceCount (uint32_t)
//   word 2: firstIndex    (uint32_t)
//   word 3: vertexOffset  (int32_t — signed, per VkDrawIndexedIndirectCommand)
//   word 4: firstInstance (uint32_t)
// ----------------------------------------------------------------------------
struct Flux_IndirectDrawIndexedCommand
{
	uint32_t m_uIndexCount;
	uint32_t m_uInstanceCount;
	uint32_t m_uFirstIndex;
	int32_t  m_iVertexOffset;
	uint32_t m_uFirstInstance;
};
static_assert(sizeof(Flux_IndirectDrawIndexedCommand) == 20u,
	"Flux_IndirectDrawIndexedCommand must be exactly five 32-bit words / 20 bytes — "
	"it is the cross-API indirect-argument ABI pinned by tests, allocation sizing, "
	"the Slang shared include and every backend recorder.");
static_assert(alignof(Flux_IndirectDrawIndexedCommand) == alignof(uint32_t),
	"Flux_IndirectDrawIndexedCommand must have uint32 alignment so a packed array of "
	"records has no implicit padding between entries — the GPU reads a flat stride.");

// Named word/byte/record constants derived from the ABI. The Slang shared include
// re-declares the same constants on its side; a static_assert in
// Flux_Terrain.cpp pins both halves to the same values.
inline constexpr uint32_t uFLUX_INDIRECT_DRAW_INDEXED_WORD_COUNT = 5u;
inline constexpr uint32_t uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE = sizeof(Flux_IndirectDrawIndexedCommand);
static_assert(uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE ==
		uFLUX_INDIRECT_DRAW_INDEXED_WORD_COUNT * sizeof(uint32_t),
	"Flux_IndirectDrawIndexedCommand stride must be exactly word_count * sizeof(uint32_t)");

// Zero-initialise every word of an indirect command to the legal no-op. Used
// by allocation seeding (CPU one-shot at buffer creation) and by tests; the
// GPU reset pass writes its own zeroed records from the shader.
inline void Flux_ZeroIndirectDrawIndexedCommand(Flux_IndirectDrawIndexedCommand& xOut)
{
	xOut.m_uIndexCount    = 0u;
	xOut.m_uInstanceCount = 0u;
	xOut.m_uFirstIndex    = 0u;
	xOut.m_iVertexOffset  = 0;
	xOut.m_uFirstInstance = 0u;
}
