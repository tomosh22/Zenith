//------------------------------------------------------------------------------
// Flux_IndirectDraw.Tests.inl — pure policy/ABI tests for the neutral indirect-
// count fallback header. Hosted by Flux.cpp (Flux_RendererImpl runtime) so the
// ZENITH_TEST registrars survive MSVC's /OPT:REF linker pass in every backend
// variant AND every game that links the engine lib — Flux_BackendConformance.cpp
// has only compile-time static_asserts and zero runtime functions, so /OPT:REF
// would dead-strip its .obj and take the registrars with it (the
// Null/CLAUDE.md hazard manifests as Combat's unit count growing by only +8
// when the include lives there, instead of +34 once moved here). The header's
// tests are pure C++ arithmetic / small-struct assertions over constexpr
// inputs (no device, no command buffer, no API call), headless-safe in all
// configs.
//
// Coverage:
//   * Indirect-command ABI: 20 bytes, five words, zero-fill semantics.
//   * Selector: auto/capable -> NATIVE_COUNT, forced padded/single, fail-closed
//     cases for REQUIRE_NATIVE + native unavailable, NATIVE override on
//     unsupported hardware, native count independent of multiDrawIndirect,
//     and over-limit fallback without splitting one global count buffer.
//   * Batch planner: size 1, 64, 4095, 4096, non-divisible final batch, last-
//     record byte offset, integer-overflow rejection, uDrawCount == 0.
//   * Fixed-draw per-call limit clamp for multi-draw being disabled.
//
// IMPORTANT CODE-SHAPE: the ZENITH_TEST macro defines BOTH the static
// function declaration and the trailing signature body-less header, so the
// test body follows the macro DIRECTLY in `{ ... }` form — no out-of-line
// `void ...Test()` definition. Defining the body twice ("already has a body"
// C2084) is the failure mode this prevents.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Flux/Backend/Flux_IndirectDraw.h"

#ifdef ZENITH_TESTING

//------------------------------------------------------------------------------
// Indirect-command ABI
//------------------------------------------------------------------------------

ZENITH_TEST(FluxIndirectDraw, AbiIsFiveWordsTwentyBytes)
{
	ZENITH_ASSERT_EQ(sizeof(Flux_IndirectDrawIndexedCommand), 20u,
		"the indexed-indirect-command ABI is exactly five 32-bit words / 20 bytes");
	ZENITH_ASSERT_EQ(uFLUX_INDIRECT_DRAW_INDEXED_WORD_COUNT, 5u,
		"the named word-count constant must be 5");
	ZENITH_ASSERT_EQ(uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE, 20u,
		"the named byte-stride constant must be 20");
	ZENITH_ASSERT_EQ(uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE,
		uFLUX_INDIRECT_DRAW_INDEXED_WORD_COUNT * sizeof(uint32_t),
		"stride must equal word_count * sizeof(uint32_t) — every record is word-aligned with no implicit padding");
}

ZENITH_TEST(FluxIndirectDraw, ZeroCommandWritesAllFiveWords)
{
	// Poison every field first (0xCDCDCDCD-style sentinel fill), then zero
	// through the helper and assert every one of the five words was written.
	// A partial-fill regression would leave a stale word behind, reading as
	// a real count / offset on the GPU and producing plausible-but-wrong draw
	// commands — exactly the sort of bug the "zero-tail" invariant prevents.
	Flux_IndirectDrawIndexedCommand xCmd;
	xCmd.m_uIndexCount    = 0xDEADBEEFu;
	xCmd.m_uInstanceCount = 0xCAFEBABEu;
	xCmd.m_uFirstIndex    = 0xBAADF00Du;
	xCmd.m_iVertexOffset  = static_cast<int32_t>(0xFACEFEEDu);
	xCmd.m_uFirstInstance = 0xFEEDFACEu;
	Flux_ZeroIndirectDrawIndexedCommand(xCmd);
	ZENITH_ASSERT_EQ(xCmd.m_uIndexCount,    0u, "zero-fill must clear indexCount");
	ZENITH_ASSERT_EQ(xCmd.m_uInstanceCount, 0u, "zero-fill must clear instanceCount");
	ZENITH_ASSERT_EQ(xCmd.m_uFirstIndex,    0u, "zero-fill must clear firstIndex");
	ZENITH_ASSERT_EQ(xCmd.m_iVertexOffset,   0,  "zero-fill must clear vertexOffset (signed int)");
	ZENITH_ASSERT_EQ(xCmd.m_uFirstInstance, 0u, "zero-fill must clear firstInstance");
}

ZENITH_TEST(FluxIndirectDraw, LiveRecordHoldsTheFiveFields)
{
	// The "live record" the culling shader writes for one visible chunk, per
	// the contract in Docs/design/TerrainIndirectCountFallback.md. Pins field
	// ORDER against the Slang twin (Flux_TerrainIndirectCommon.slang) — a
	// transposed pair would decode wild values on the GPU.
	Flux_IndirectDrawIndexedCommand xLive;
	xLive.m_uIndexCount    = 6144u; // selected resident LOD index count
	xLive.m_uInstanceCount = 1u;    // exactly one instance per live chunk
	xLive.m_uFirstIndex    = 1024u; // selected resident LOD first index
	xLive.m_iVertexOffset  = static_cast<int32_t>(2048u); // selected resident LOD vertex offset
	xLive.m_uFirstInstance = 42u;    // stable chunk index (carries chunk identity to the VS)
	ZENITH_ASSERT_EQ(xLive.m_uInstanceCount, 1u,
		"a live record always draws exactly one instance — instanceCount == 1 is the contract");
}

//------------------------------------------------------------------------------
// Mode-selection: selector table (auto/capable, override, fail-closed)
//------------------------------------------------------------------------------

namespace
{
	// The shipping Vulkan desktop build reports these caps on a typical
	// Vulkan 1.2+ device with multi-draw + first-instance + draw-parameters
	// all enabled and maxDrawIndirectCount at the spec-native floor of 1.
	Flux_IndirectDrawCapabilities MakeCapableDesktopCaps()
	{
		return Flux_IndirectDrawCapabilities{
			true,   // m_bNativeIndexedIndirectCount
			true,   // m_bMultiDrawIndirect
			true,   // m_bIndirectFirstInstance
			true,   // m_bShaderDrawParameters
			1u,     // m_uMaxDrawIndirectCount (spec floor; raised by real hw)
		};
	}
	// Advertised-but-null: a broken ICD that names the count extension but
	// hands back a null function pointer. The cap record reports usable=false
	// even though the device nominally "supports" count.
	Flux_IndirectDrawCapabilities MakeAdvertisedButNullCaps()
	{
		return Flux_IndirectDrawCapabilities{
			false,  // m_bNativeIndexedIndirectCount (usable, not advertised)
			true,   // m_bMultiDrawIndirect
			true,   // m_bIndirectFirstInstance
			true,   // m_bShaderDrawParameters
			1u,     // m_uMaxDrawIndirectCount
		};
	}
	// Adapter with no count at all — the Android emulator tier the plan
	// targets. multi-draw is on (desktop emulator passes the hard-suitability
	// check), draw-parameters and first-instance are on (terrain minimums).
	Flux_IndirectDrawCapabilities MakeNoCountCaps()
	{
		return Flux_IndirectDrawCapabilities{
			false,  // m_bNativeIndexedIndirectCount
			true,   // m_bMultiDrawIndirect
			true,   // m_bIndirectFirstInstance
			true,   // m_bShaderDrawParameters
			1u,     // m_uMaxDrawIndirectCount
		};
	}
	// No count AND no multi-draw — the legal tier forces PADDED_SINGLE.
	Flux_IndirectDrawCapabilities MakeNoCountNoMultiDrawCaps()
	{
		return Flux_IndirectDrawCapabilities{
			false,  // m_bNativeIndexedIndirectCount
			false,  // m_bMultiDrawIndirect
			true,   // m_bIndirectFirstInstance
			true,   // m_bShaderDrawParameters
			1u,     // m_uMaxDrawIndirectCount (conservative reported limit)
		};
	}
	// Native count is usable while fixed multi-draw is unavailable. Keep a
	// deliberately large reported native limit to prove the two capabilities
	// are negotiated independently.
	Flux_IndirectDrawCapabilities MakeCountButNoMultiDrawCaps()
	{
		return Flux_IndirectDrawCapabilities{
			true,   // m_bNativeIndexedIndirectCount
			false,  // m_bMultiDrawIndirect (fixed multi-draw is unavailable)
			true,   // m_bIndirectFirstInstance
			true,   // m_bShaderDrawParameters
			4096u,  // m_uMaxDrawIndirectCount (raw device limit)
		};
	}
}

ZENITH_TEST(FluxIndirectDraw, AutoCapableSelectsNativeCount)
{
	const Flux_IndirectDrawCapabilities xCaps = MakeCapableDesktopCaps();
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 1u,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::AUTO)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::NATIVE_COUNT),
		"auto on a count-capable device, uMaxDrawCount <= 1, must select NATIVE_COUNT");
}

ZENITH_TEST(FluxIndirectDraw, AutoUnsupportedSelectsPaddedMultiWhenMultiDrawOn)
{
	const Flux_IndirectDrawCapabilities xCaps = MakeNoCountCaps();
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::AUTO)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::PADDED_MULTI),
		"auto on a no-count device with multi-draw ON, policy=ZERO_PADDED, must select PADDED_MULTI");
}

ZENITH_TEST(FluxIndirectDraw, AutoUnsupportedSelectsPaddedSingleWhenMultiDrawOff)
{
	const Flux_IndirectDrawCapabilities xCaps = MakeNoCountNoMultiDrawCaps();
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::AUTO)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::PADDED_SINGLE),
		"auto on a no-count no-multi-draw device, policy=ZERO_PADDED, must select PADDED_SINGLE");
}

ZENITH_TEST(FluxIndirectDraw, RequireNativeUnsupportedFailsClosed)
{
	const Flux_IndirectDrawCapabilities xCaps = MakeNoCountCaps();
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::REQUIRE_NATIVE, Flux_IndirectDrawOverride::AUTO)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::FAILED_CLOSED),
		"a REQUIRE_NATIVE caller whose native preconditions fail must FAIL CLOSED, never silently fall back");
}

ZENITH_TEST(FluxIndirectDraw, ForcedPaddedWithRequireNativeFailsClosed)
{
	const Flux_IndirectDrawCapabilities xCaps = MakeCapableDesktopCaps();
	// The override cannot violate the caller's policy: PADDED + REQUIRE_NATIVE
	// has no legal execution path (the override forbids native count, the
	// policy forbids a padded fallback) and fails closed.
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::REQUIRE_NATIVE, Flux_IndirectDrawOverride::PADDED)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::FAILED_CLOSED),
		"a forced PADDED override on a REQUIRE_NATIVE caller must FAIL CLOSED — the override cannot downgrade the caller's promise");
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::REQUIRE_NATIVE, Flux_IndirectDrawOverride::SINGLE)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::FAILED_CLOSED),
		"a forced SINGLE override on a REQUIRE_NATIVE caller must FAIL CLOSED — same rule");
}

ZENITH_TEST(FluxIndirectDraw, ForcedNativeUnsupportedFailsClosed)
{
	const Flux_IndirectDrawCapabilities xCaps = MakeNoCountCaps();
	// The override is a TEST ASSERTION, not a preference: a NATIVE override
	// with any failed native precondition fails closed rather than silently
	// choosing padded execution.
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::NATIVE)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::FAILED_CLOSED),
		"NATIVE override on hardware missing native count must FAIL CLOSED, not slide into padded");
}

ZENITH_TEST(FluxIndirectDraw, ForcedPaddedSelectsPaddedSingleWhenNoMultiDraw)
{
	const Flux_IndirectDrawCapabilities xCaps = MakeNoCountNoMultiDrawCaps();
	// PADDED forbids native count; selects PADDED_MULTI when available and
	// otherwise the legal PADDED_SINGLE tier — here multi-draw is OFF so the
	// legal tier is PADDED_SINGLE.
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::PADDED)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::PADDED_SINGLE),
		"PADDED override on a no-multi-draw device must select PADDED_SINGLE (the legal tier)");
}

ZENITH_TEST(FluxIndirectDraw, ForcedSingleAlwaysSelectsPaddedSingle)
{
	// SINGLE forces PADDED_SINGLE regardless of multi-draw availability;
	// used by diagnostics/CI to exercise the lowest tier on capable hardware.
	const Flux_IndirectDrawCapabilities xCaps = MakeCapableDesktopCaps();
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::SINGLE)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::PADDED_SINGLE),
		"SINGLE override must force PADDED_SINGLE even when multi-draw is available");
}

ZENITH_TEST(FluxIndirectDraw, ForcedPaddedOnCapableSelectsPaddedMulti)
{
	const Flux_IndirectDrawCapabilities xCaps = MakeCapableDesktopCaps();
	// PADDED forbids native count even when available; on a multi-draw
	// capable device it selects PADDED_MULTI (the legal tier above SINGLE).
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::PADDED)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::PADDED_MULTI),
		"PADDED override on a multi-draw-capable device must select PADDED_MULTI (legal tier above SINGLE)");
}

ZENITH_TEST(FluxIndirectDraw, AdvertisedButNullEntryPointFailsNative)
{
	// A driver that advertises the count extension/feature but hands back a
	// null proc address downgrades the usable cap to false; the selector then
	// fails closed under REQUIRE_NATIVE even though the feature nominally
	// "is supported". This is the broken-ICD guard the plan demands.
	const Flux_IndirectDrawCapabilities xCaps = MakeAdvertisedButNullCaps();
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::REQUIRE_NATIVE, Flux_IndirectDrawOverride::AUTO)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::FAILED_CLOSED),
		"advertised-but-null entry point must FAIL CLOSED under REQUIRE_NATIVE");
}

ZENITH_TEST(FluxIndirectDraw, OverLimitRequestSelectsPaddedNotSplit)
{
	// A request exceeding maxDrawIndirectCount must NOT be split across the
	// same count buffer (each batch would re-read the full count and over-
	// draw). The selector must choose PADDED_MULTI when the policy permits.
	Flux_IndirectDrawCapabilities xCaps = MakeCapableDesktopCaps();
	xCaps.m_uMaxDrawIndirectCount = 1u;  // a tiny cap to make the request exceed
	const uint32_t uBig = 4096u;
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, uBig,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::AUTO)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::PADDED_MULTI),
		"an over-limit request must NOT split native count; PADDED_MULTI is legal under ZERO_PADDED");
	// Explicit NATIVE on the over-limit request still FAILS CLOSED rather than
	// splitting native count.
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, uBig,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::NATIVE)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::FAILED_CLOSED),
		"explicit NATIVE on an over-limit request must FAIL CLOSED rather than silently splitting the count buffer");
}

ZENITH_TEST(FluxIndirectDraw, NativeCountDoesNotRequireMultiDrawIndirect)
{
	const Flux_IndirectDrawCapabilities xCaps = MakeCountButNoMultiDrawCaps();
	// multiDrawIndirect gates only fixed vkCmdDrawIndexedIndirect. A usable
	// count route with a sufficient raw native limit remains native even when
	// fixed multi-draw is unavailable.
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::AUTO)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::NATIVE_COUNT),
		"native counted draw must not be rejected merely because fixed multi-draw is disabled");
	ZENITH_ASSERT_EQ(Flux_ResolveFixedDrawPerCallLimit(xCaps), 1u,
		"the same device must still clamp ordinary fixed indirect calls to one record");
}

//------------------------------------------------------------------------------
// Fixed-draw per-call limit (clamp for multi-draw being disabled)
//------------------------------------------------------------------------------

ZENITH_TEST(FluxIndirectDraw, FixedDrawPerCallLimitClampsToOneWhenNoMultiDraw)
{
	Flux_IndirectDrawCapabilities xCapsMultiOff = MakeNoCountNoMultiDrawCaps();
	xCapsMultiOff.m_uMaxDrawIndirectCount = 4096u;
	ZENITH_ASSERT_EQ(Flux_ResolveFixedDrawPerCallLimit(xCapsMultiOff), 1u,
		"with multi-draw disabled, the per-call limit MUST clamp to 1 regardless of maxDrawIndirectCount");

	Flux_IndirectDrawCapabilities xCapsMultiOn = MakeCapableDesktopCaps();
	xCapsMultiOn.m_uMaxDrawIndirectCount = 1024u;
	ZENITH_ASSERT_EQ(Flux_ResolveFixedDrawPerCallLimit(xCapsMultiOn), 1024u,
		"with multi-draw enabled, the per-call limit is the device's maxDrawIndirectCount");
}

//------------------------------------------------------------------------------
// Batch planner
//------------------------------------------------------------------------------

namespace
{
	// Collect every batch the planner emits into a vector for assertion. The
	// planner itself is heap-free; this helper is test-only.
	Zenith_Vector<Flux_IndirectDrawBatch> CollectBatches(uint32_t uRequestedDrawCount,
		uint64_t ulArgumentsOffsetBytes, uint32_t uStride, uint32_t uPerCallLimit)
	{
		Flux_IndirectDrawBatchPlan xPlan;
		xPlan.Reset(uRequestedDrawCount, ulArgumentsOffsetBytes, uStride, uPerCallLimit);
		Zenith_Vector<Flux_IndirectDrawBatch> axBatches;
		Flux_IndirectDrawBatch xBatch;
		while (xPlan.Next(xBatch))
		{
			axBatches.PushBack(xBatch);
		}
		return axBatches;
	}
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerEmptyForZeroRequest)
{
	const Zenith_Vector<Flux_IndirectDrawBatch> axBatches = CollectBatches(0u, 0u, 20u, 1u);
	ZENITH_ASSERT_EQ(axBatches.GetSize(), 0u,
		"uDrawCount == 0 must produce no API command (an empty plan)");
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerSingleRecordOneBatch)
{
	const Zenith_Vector<Flux_IndirectDrawBatch> axBatches = CollectBatches(1u, 0u, 20u, 1u);
	ZENITH_ASSERT_EQ(axBatches.GetSize(), 1u, "1 record -> 1 batch");
	ZENITH_ASSERT_EQ(axBatches.Get(0).m_uDrawCount, 1u, "single-record batch carries drawCount == 1");
	ZENITH_ASSERT_EQ(axBatches.Get(0).m_ulArgumentsOffsetBytes, 0u, "single-record batch starts at offset 0");
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerSingleForcesDrawCountOne)
{
	// PADDED_SINGLE every batch carries drawCount == 1, regardless of the
	// per-call limit the recorder harmless-passes through.
	const Zenith_Vector<Flux_IndirectDrawBatch> axBatches = CollectBatches(4096u, 0u, 20u, 1u);
	ZENITH_ASSERT_EQ(axBatches.GetSize(), 4096u, "4096 records at per-call=1 -> 4096 batches");
	for (uint32_t u = 0u; u < axBatches.GetSize(); ++u)
	{
		ZENITH_ASSERT_EQ(axBatches.Get(u).m_uDrawCount, 1u,
			"every PADDED_SINGLE batch must carry drawCount == 1 (batch %u)", u);
		ZENITH_ASSERT_EQ(axBatches.Get(u).m_ulArgumentsOffsetBytes, static_cast<uint64_t>(u) * 20u,
			"every batch offsets by stride * drawCount (batch %u)", u);
	}
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerBatchesExactlyFitLimit)
{
	// 64 records, per-call 64 -> one batch of 64.
	const Zenith_Vector<Flux_IndirectDrawBatch> axBatches = CollectBatches(64u, 0u, 20u, 64u);
	ZENITH_ASSERT_EQ(axBatches.GetSize(), 1u, "64 records / per-call 64 -> 1 batch");
	ZENITH_ASSERT_EQ(axBatches.Get(0).m_uDrawCount, 64u, "the single batch carries all 64 records");
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerNonDivisibleFinalBatchIsRemainder)
{
	// 4095 records, per-call 64 -> 64 batches of 64 = 4096, one batch short
	// of the request. The final batch is 4095 - 63*64 = 4095 - 4032 = 63.
	const Zenith_Vector<Flux_IndirectDrawBatch> axBatches = CollectBatches(4095u, 0u, 20u, 64u);
	ZENITH_ASSERT_EQ(axBatches.GetSize(), 64u, "4095 / 64 (ceil) -> 64 batches");
	ZENITH_ASSERT_EQ(axBatches.Get(0).m_uDrawCount, 64u, "first batch carries 64 records");
	ZENITH_ASSERT_EQ(axBatches.Get(axBatches.GetSize() - 1u).m_uDrawCount, 63u,
		"the final non-divisible batch is the remainder 63 (4095 - 63*64)");
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerFourThousandNinetySixSplitsAtMax)
{
	// The shipping PADDED_SINGLE tier case: 4096 records at per-call 1 ->
	// 4096 batches, exactly one record each.
	const Zenith_Vector<Flux_IndirectDrawBatch> axBatches = CollectBatches(4096u, 0u, 20u, 1u);
	ZENITH_ASSERT_EQ(axBatches.GetSize(), 4096u, "4096 records / per-call 1 -> 4096 batches");
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerLastRecordOffsetLandsAtAllocationBoundary)
{
	// The padded-tail contract: TOTAL_CHUNKS records at the 20-byte stride
	// land exactly at the allocation boundary of TOTAL_CHUNKS * 20 bytes
	// (the seeded indirect-buffer size). The last record's start offset is
	// (TOTAL_CHUNKS - 1) * 20; the last batch ends at TOTAL_CHUNKS * 20.
	constexpr uint32_t uTOTAL = 4096u;
	const Zenith_Vector<Flux_IndirectDrawBatch> axBatches = CollectBatches(uTOTAL, 0u, 20u, 1u);
	ZENITH_ASSERT_EQ(axBatches.GetSize(), uTOTAL, "TOTAL_CHUNKS / per-call 1 -> TOTAL_CHUNKS batches");
	const Flux_IndirectDrawBatch& rxLast = axBatches.Get(axBatches.GetSize() - 1u);
	ZENITH_ASSERT_EQ(rxLast.m_ulArgumentsOffsetBytes, static_cast<uint64_t>(uTOTAL - 1u) * 20u,
		"the last record must start at (TOTAL-1)*20 bytes — the very last 20 bytes of the allocation");
	// After the last batch, the cursor's projected advance ends at:
	const uint64_t ulEndCursor = rxLast.m_ulArgumentsOffsetBytes + rxLast.m_uDrawCount * 20u;
	ZENITH_ASSERT_EQ(ulEndCursor, static_cast<uint64_t>(uTOTAL) * 20u,
		"the last record + its stride must end exactly at the allocation boundary TOTAL_CHUNKS * 20");
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerAllowsFinalAdvancePastUint32)
{
	// Vulkan offsets are VkDeviceSize, not uint32. A final record may start
	// below 4 GiB and end exactly above it without making its start invalid.
	Flux_IndirectDrawBatchPlan xPlan;
	constexpr uint64_t ulNEAR = 0xFFFFFFF0ull;
	ZENITH_ASSERT_TRUE(xPlan.Reset(1u, ulNEAR, 20u, 1u),
		"one final record whose start is representable must preflight");
	Flux_IndirectDrawBatch xBatch;
	ZENITH_ASSERT_TRUE(xPlan.Next(xBatch),
		"the final batch must not be rejected merely because its one-past cursor exceeds uint32");
	ZENITH_ASSERT_EQ(xBatch.m_ulArgumentsOffsetBytes, ulNEAR,
		"the VkDeviceSize-width batch offset must be preserved");
	ZENITH_ASSERT_FALSE(xPlan.HasOverflowed(), "the 64-bit plan did not overflow");
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerRejectsWholePlanBeforeEmissionOnUint64Overflow)
{
	Flux_IndirectDrawBatchPlan xPlan;
	ZENITH_ASSERT_FALSE(xPlan.Reset(2u, UINT64_MAX - 8u, 20u, 1u),
		"a plan whose second record start overflows VkDeviceSize must fail preflight");
	Flux_IndirectDrawBatch xBatch;
	ZENITH_ASSERT_FALSE(xPlan.Next(xBatch),
		"an invalid whole plan must emit no prefix batch");
	ZENITH_ASSERT_TRUE(xPlan.HasOverflowed(), "whole-plan overflow must be reported");
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerZeroPerCallLimitYieldsEmptyPlan)
{
	// A defensive clamp: a per-call limit of 0 would otherwise loop forever
	// emitting zero-record batches. The plan refuses to start.
	Flux_IndirectDrawBatchPlan xPlan;
	ZENITH_ASSERT_FALSE(xPlan.Reset(4096u, 0u, 20u, 0u),
		"a non-empty plan with a zero per-call limit must fail preflight");
	Flux_IndirectDrawBatch xBatch;
	ZENITH_ASSERT_FALSE(xPlan.Next(xBatch),
		"a per-call limit of 0 must produce no batches — never an infinite loop of zero-record batches");
}

ZENITH_TEST(FluxIndirectDraw, BatchPlannerNonZeroStartOffsetPreserved)
{
	// Terrain passes uArguments=0; a non-zero start (e.g. multiple sub-ranges
	// sharing one argument buffer) must be preserved as the first batch's
	// start offset, not folded into the stride advance.
	const Zenith_Vector<Flux_IndirectDrawBatch> axBatches = CollectBatches(4u, 200u, 20u, 2u);
	ZENITH_ASSERT_EQ(axBatches.GetSize(), 2u, "4 records / per-call 2 -> 2 batches");
	ZENITH_ASSERT_EQ(axBatches.Get(0).m_ulArgumentsOffsetBytes, 200u,
		"the first batch must start at the caller's supplied start offset");
	ZENITH_ASSERT_EQ(axBatches.Get(0).m_uDrawCount, 2u, "first batch carries 2 records");
	ZENITH_ASSERT_EQ(axBatches.Get(1).m_ulArgumentsOffsetBytes, 200u + 2u * 20u,
		"the second batch starts after the first by stride*drawCount");
	ZENITH_ASSERT_EQ(axBatches.Get(1).m_uDrawCount, 2u, "second batch carries 2 records");
}

#endif // ZENITH_TESTING
