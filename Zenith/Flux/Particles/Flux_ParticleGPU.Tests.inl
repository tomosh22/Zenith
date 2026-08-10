#include "UnitTests/Zenith_UnitTests.h"
#include "FileAccess/Zenith_FileAccess.h"   // the source-text pin reads the compute shader

#include <string>    // bounded search over the shader source

// ============================================================================
// Flux_ParticleGPU — the GPU-driven pool's addressing + lifetime contracts.
//
// The three things that can go wrong here are all silent:
//   * the spawn RING mis-splits at the wrap, so a burst either overruns the
//     emitter's reservation (writing a neighbour's particles) or drops records;
//   * the blend PARTITIONS overlap, so additive particles are fetched by the
//     alpha draw at the wrong byte base;
//   * an emitter's RESERVATION is not reclaimed, so a register/unregister cycle
//     exhausts a pool that looks empty.
// None of them fail loudly on the GPU — they render the wrong particles. The
// pure helpers below exist so each one is checkable without a device, and the
// pool tests drive the LIVE impl (restoring its state) so the accounting is
// pinned against the code that actually runs.
// ============================================================================

namespace
{
	// The pool tests mutate process-wide state. Both fields are public and plain,
	// so an exact snapshot/restore is possible — and mandatory: leaking a
	// reservation would shrink the pool for the rest of the process, and the next
	// test to register would see a different base offset.
	struct ScopedParticlePoolState
	{
		ScopedParticlePoolState()
			: m_xImpl(g_xEngine.ParticleGPU())
			, m_axSavedEmitters(g_xEngine.ParticleGPU().m_axEmitters)
			, m_uSavedTotal(g_xEngine.ParticleGPU().m_uTotalAllocatedParticles)
			, m_bSavedActive(g_xEngine.ParticleGPU().IsActiveThisFrame())
		{
			m_xImpl.m_axEmitters.Clear();
			m_xImpl.m_uTotalAllocatedParticles = 0;
		}

		~ScopedParticlePoolState()
		{
			m_xImpl.m_axEmitters = m_axSavedEmitters;
			m_xImpl.m_uTotalAllocatedParticles = m_uSavedTotal;
			m_xImpl.m_bActiveThisFrame = m_bSavedActive;
		}

		Flux_ParticleGPUImpl& m_xImpl;
		Zenith_Vector<Flux_ParticleGPUImpl::EmitterData> m_axSavedEmitters;
		u_int m_uSavedTotal;
		bool  m_bSavedActive;
	};
}

// ---------------------------------------------------------------------------
// Spawn ring
// ---------------------------------------------------------------------------

ZENITH_TEST(ParticleGPU, SpawnRingKeepsAnUnwrappedBurstInOneRun)
{
	const Flux_ParticleSpawnRuns xRuns = Flux_SplitParticleSpawnRing(/*cursor*/ 2u, /*count*/ 5u, /*capacity*/ 16u);

	ZENITH_ASSERT_EQ(xRuns.m_uFirstStart, 2u, "an unwrapped burst starts at the cursor");
	ZENITH_ASSERT_EQ(xRuns.m_uFirstCount, 5u, "and is one contiguous run");
	ZENITH_ASSERT_EQ(xRuns.m_uSecondCount, 0u, "with no wrapped remainder");
	ZENITH_ASSERT_EQ(xRuns.m_uNextCursor, 7u, "the cursor advances by the burst");
	ZENITH_ASSERT_EQ(xRuns.TotalCount(), 5u, "every requested record is placed");
}

ZENITH_TEST(ParticleGPU, SpawnRingSplitsAtTheWrap)
{
	// The upload is two vkCmdCopyBuffer regions, so the split has to be exact:
	// a first run that ends AT the reservation's last slot, and a second that
	// restarts at slot 0. One-off either way writes into the neighbouring emitter.
	const Flux_ParticleSpawnRuns xRuns = Flux_SplitParticleSpawnRing(/*cursor*/ 14u, /*count*/ 5u, /*capacity*/ 16u);

	ZENITH_ASSERT_EQ(xRuns.m_uFirstStart, 14u, "the first run still starts at the cursor");
	ZENITH_ASSERT_EQ(xRuns.m_uFirstCount, 2u, "and stops at the end of the reservation");
	ZENITH_ASSERT_EQ(xRuns.m_uSecondCount, 3u, "the remainder restarts at slot 0");
	ZENITH_ASSERT_EQ(xRuns.m_uNextCursor, 3u, "the cursor lands past the wrapped remainder");
	ZENITH_ASSERT_EQ(xRuns.TotalCount(), 5u, "nothing is lost across the wrap");
}

ZENITH_TEST(ParticleGPU, SpawnRingEndsExactlyOnTheBoundaryWithoutAnEmptySecondRun)
{
	// The off-by-one case: a burst that finishes on the last slot must NOT emit a
	// zero-length second upload, and must leave the cursor wrapped to 0.
	const Flux_ParticleSpawnRuns xRuns = Flux_SplitParticleSpawnRing(/*cursor*/ 12u, /*count*/ 4u, /*capacity*/ 16u);

	ZENITH_ASSERT_EQ(xRuns.m_uFirstCount, 4u, "the burst fits exactly to the boundary");
	ZENITH_ASSERT_EQ(xRuns.m_uSecondCount, 0u, "so there is no wrapped run at all");
	ZENITH_ASSERT_EQ(xRuns.m_uNextCursor, 0u, "and the cursor wraps to the start");
}

ZENITH_TEST(ParticleGPU, SpawnRingClampsABurstLargerThanTheReservation)
{
	// A burst bigger than the ring would overwrite its OWN earlier records inside a
	// single upload — only the last uCapacity could survive, so the surplus is
	// dropped rather than written twice.
	const Flux_ParticleSpawnRuns xRuns = Flux_SplitParticleSpawnRing(/*cursor*/ 3u, /*count*/ 100u, /*capacity*/ 8u);

	ZENITH_ASSERT_EQ(xRuns.TotalCount(), 8u, "an over-capacity burst is clamped to the ring");
	ZENITH_ASSERT_EQ(xRuns.m_uFirstStart, 3u, "still starting at the cursor");
	ZENITH_ASSERT_EQ(xRuns.m_uFirstCount, 5u, "5 slots to the end...");
	ZENITH_ASSERT_EQ(xRuns.m_uSecondCount, 3u, "...then 3 more from slot 0");
	ZENITH_ASSERT_EQ(xRuns.m_uNextCursor, 3u, "a full lap returns the cursor where it started");
}

ZENITH_TEST(ParticleGPU, SpawnRingIsInertWithNoReservation)
{
	// An unregistered / zero-capacity emitter must produce no upload at all rather
	// than a modulo by zero.
	const Flux_ParticleSpawnRuns xRuns = Flux_SplitParticleSpawnRing(/*cursor*/ 0u, /*count*/ 4u, /*capacity*/ 0u);

	ZENITH_ASSERT_EQ(xRuns.TotalCount(), 0u, "a zero-capacity ring places nothing");
	ZENITH_ASSERT_EQ(xRuns.m_uFirstCount, 0u, "no first run");
	ZENITH_ASSERT_EQ(xRuns.m_uSecondCount, 0u, "no second run");
}

ZENITH_TEST(ParticleGPU, SpawnRingWalksEverySlotExactlyOncePerLap)
{
	// Property check over a full lap of single spawns: the cursor visits 0..N-1 in
	// order and returns home, which is what makes "overwrite the oldest" true.
	const u_int uCapacity = 7u;
	u_int uCursor = 0u;
	for (u_int u = 0; u < uCapacity; ++u)
	{
		const Flux_ParticleSpawnRuns xRuns = Flux_SplitParticleSpawnRing(uCursor, 1u, uCapacity);
		ZENITH_ASSERT_EQ(xRuns.m_uFirstStart, u, "lap step %u writes slot %u", u, u);
		ZENITH_ASSERT_EQ(xRuns.m_uFirstCount, 1u, "one record per step");
		uCursor = xRuns.m_uNextCursor;
	}
	ZENITH_ASSERT_EQ(uCursor, 0u, "a full lap returns the cursor to the start");
}

ZENITH_TEST(ParticleGPU, RingOccupancySaturatesAtTheReservation)
{
	ZENITH_ASSERT_EQ(Flux_AdvanceParticleRingOccupancy(0u, 3u, 10u), 3u, "occupancy grows while the ring has room");
	ZENITH_ASSERT_EQ(Flux_AdvanceParticleRingOccupancy(8u, 1u, 10u), 9u, "...right up to the last free slot");
	ZENITH_ASSERT_EQ(Flux_AdvanceParticleRingOccupancy(9u, 1u, 10u), 10u, "the filling spawn saturates it");
	ZENITH_ASSERT_EQ(Flux_AdvanceParticleRingOccupancy(10u, 5u, 10u), 10u,
		"and further spawns RECYCLE slots — the count must never exceed the reservation");
}

// ---------------------------------------------------------------------------
// Blend partitions + the indirect block
// ---------------------------------------------------------------------------

ZENITH_TEST(ParticleGPU, BlendPartitionsAreDisjointAndCoverTheInstanceBuffer)
{
	// The alpha and additive draws bind the SAME buffer at different byte bases, so
	// an overlap would silently make one draw fetch the other's instances.
	const size_t uAlpha    = Flux_ParticleGPUImpl::GetPartitionByteOffset(uFLUX_PARTICLE_PARTITION_ALPHA);
	const size_t uAdditive = Flux_ParticleGPUImpl::GetPartitionByteOffset(uFLUX_PARTICLE_PARTITION_ADDITIVE);
	const size_t uStride   = sizeof(Flux_ParticleInstance);

	ZENITH_ASSERT_EQ(static_cast<u_int>(uAlpha), 0u, "the alpha partition is the buffer's head");
	ZENITH_ASSERT_EQ(static_cast<u_int>(uAdditive),
		static_cast<u_int>(Flux_ParticleGPUImpl::s_uPartitionCapacity * uStride),
		"the additive partition starts exactly one whole partition in");
	ZENITH_ASSERT_EQ(static_cast<u_int>(uAdditive + Flux_ParticleGPUImpl::s_uPartitionCapacity * uStride),
		static_cast<u_int>(Flux_ParticleGPUImpl::s_uInstanceCapacity * uStride),
		"and the two together are exactly the allocated instance buffer");
}

ZENITH_TEST(ParticleGPU, NeitherPartitionCanOverflowGivenThePoolSize)
{
	// THE reason there is no indirect-fixup clamp pass: a particle emits at most one
	// instance and the pool is shared across blend modes, so even an all-alpha or
	// all-additive frame fits its partition.
	ZENITH_ASSERT_TRUE(Flux_ParticleGPUImpl::s_uPartitionCapacity >= Flux_ParticleGPUImpl::s_uMaxGPUParticles,
		"a partition must hold the WHOLE pool, or a single-blend-mode frame could overrun it");
}

ZENITH_TEST(ParticleGPU, EachPartitionOwnsOneIndirectCommand)
{
	ZENITH_ASSERT_EQ(Flux_ParticleGPUImpl::GetPartitionArgsByteOffset(uFLUX_PARTICLE_PARTITION_ALPHA), 0u,
		"the alpha command is the first in the block");
	ZENITH_ASSERT_EQ(Flux_ParticleGPUImpl::GetPartitionArgsByteOffset(uFLUX_PARTICLE_PARTITION_ADDITIVE),
		uFLUX_PARTICLE_INDIRECT_STRIDE,
		"the additive command is one VkDrawIndexedIndirectCommand later");
	ZENITH_ASSERT_EQ(uFLUX_PARTICLE_INDIRECT_STRIDE, 20u,
		"VkDrawIndexedIndirectCommand is five 32-bit words — the stride the draw is recorded with");
}

ZENITH_TEST(ParticleGPU, IndirectSeedIsAZeroInstanceUnitQuadDraw)
{
	// What PreExecuteCompute uploads every frame. instanceCount MUST start at 0 —
	// it is the compute pass's atomic, and a non-zero seed would draw stale
	// instances before a single particle had been compacted.
	u_int auCommand[uFLUX_PARTICLE_INDIRECT_WORDS] = { 0xDEADu, 0xDEADu, 0xDEADu, 0xDEADu, 0xDEADu };
	Flux_PackResetIndirectCommand(auCommand, uFLUX_PARTICLE_QUAD_INDEX_COUNT);

	ZENITH_ASSERT_EQ(auCommand[0], 6u, "the billboard is the shared unit quad: two triangles, six indices");
	ZENITH_ASSERT_EQ(auCommand[1], 0u, "instanceCount starts at zero — the compute pass counts into it");
	ZENITH_ASSERT_EQ(auCommand[2], 0u, "firstIndex");
	ZENITH_ASSERT_EQ(auCommand[3], 0u, "vertexOffset");
	ZENITH_ASSERT_EQ(auCommand[4], 0u,
		"firstInstance stays 0 — the partition base is applied by the vertex-stream bind offset, "
		"so the draw needs no drawIndirectFirstInstance device feature");
	ZENITH_ASSERT_EQ(uFLUX_PARTICLE_QUAD_INDEX_COUNT, 6u, "and the CPU draw path indexes the same quad");
}

ZENITH_TEST(ParticleGPU, SlangIndirectWordCountMatchesTheMirror)
{
	// Twin of ParticleInstance.SlangWriterWordCountMatchesTheMirror, for the OTHER
	// cross-language literal in this pass. uINDIRECT_WORDS in the shader strides the
	// atomic into word (partition*N + 1); the C++ side strides the SEED upload and
	// the draw's indirect offset by the same N. An RWStructuredBuffer<uint> reflects
	// a 4-byte element that says nothing about 5, so no static_assert can reach it —
	// read the source and parse the literal.
	const char* szPath = SHADER_SOURCE_ROOT "Particles/Flux_ParticleUpdate.slang";
	if (!Zenith_FileAccess::FileExists(szPath))
	{
		Zenith_Log(LOG_CATEGORY_PARTICLES, "SlangIndirectWordCountMatchesTheMirror: shader source absent (%s) — packaged tree, pin skipped", szPath);
		return;
	}

	uint64_t ulSize = 0u;
	char* pcData = Zenith_FileAccess::ReadFile(szPath, ulSize);
	ZENITH_ASSERT_TRUE(pcData != nullptr, "shader source exists but failed to read");
	if (pcData == nullptr)
	{
		return;
	}
	const std::string strSource(pcData, static_cast<size_t>(ulSize));
	Zenith_FileAccess::FreeFileData(pcData);

	const size_t ulDecl = strSource.find("static const uint uINDIRECT_WORDS");
	ZENITH_ASSERT_TRUE(ulDecl != std::string::npos,
		"Flux_ParticleUpdate.slang must declare uINDIRECT_WORDS on one line (the pin parses it)");
	if (ulDecl == std::string::npos)
	{
		return;
	}
	const size_t ulEquals = strSource.find('=', ulDecl);
	ZENITH_ASSERT_TRUE(ulEquals != std::string::npos, "uINDIRECT_WORDS declaration must carry its initialiser");
	if (ulEquals == std::string::npos)
	{
		return;
	}
	const u_int uShaderWords = static_cast<u_int>(atoi(strSource.c_str() + ulEquals + 1));
	ZENITH_ASSERT_EQ(uShaderWords, uFLUX_PARTICLE_INDIRECT_WORDS,
		"Flux_ParticleUpdate.slang's uINDIRECT_WORDS has drifted from uFLUX_PARTICLE_INDIRECT_WORDS — "
		"the compute pass would count into a different word than the CPU seeds and the draw reads");
}

// ---------------------------------------------------------------------------
// Pool registration lifetime (drives the live impl, restores its state)
// ---------------------------------------------------------------------------

ZENITH_TEST(ParticleGPU, RegisterCarvesDisjointPoolRanges)
{
	ScopedParticlePoolState xScope;
	Flux_ParticleGPUImpl& xGPU = xScope.m_xImpl;

	Flux_ParticleEmitterConfig xConfigA;
	Flux_ParticleEmitterConfig xConfigB;

	const u_int uA = xGPU.RegisterEmitter(&xConfigA, 128u);
	const u_int uB = xGPU.RegisterEmitter(&xConfigB, 64u);

	ZENITH_ASSERT_TRUE(uA != UINT32_MAX && uB != UINT32_MAX, "an empty pool must accept both emitters");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uA).m_uBaseOffset, 0u, "the first emitter takes the head of the pool");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uB).m_uBaseOffset, 128u,
		"the second starts exactly past the first — an overlap would have them integrating each other's particles");
	ZENITH_ASSERT_EQ(xGPU.m_uTotalAllocatedParticles, 192u, "the pool watermark is the sum of the reservations");
	ZENITH_ASSERT_TRUE(xGPU.HasGPUEmitters(), "two live registrations report as live");
}

ZENITH_TEST(ParticleGPU, UnregisterKeepsTheReservationForReuse)
{
	// The pool is carved by base offset and nothing compacts it. If unregistering
	// released the range outright, a register/unregister cycle (a scene that
	// respawns an effect) would march the watermark to the cap and then refuse
	// every emitter, with the pool sitting empty.
	ScopedParticlePoolState xScope;
	Flux_ParticleGPUImpl& xGPU = xScope.m_xImpl;

	Flux_ParticleEmitterConfig xConfigA;
	Flux_ParticleEmitterConfig xConfigB;

	const u_int uA = xGPU.RegisterEmitter(&xConfigA, 256u);
	const u_int uBase = xGPU.m_axEmitters.Get(uA).m_uBaseOffset;
	xGPU.UnregisterEmitter(uA);

	ZENITH_ASSERT_TRUE(!xGPU.HasGPUEmitters(), "the retired emitter is no longer live");
	ZENITH_ASSERT_EQ(xGPU.m_uTotalAllocatedParticles, 256u, "the reservation is kept, not released");

	const u_int uB = xGPU.RegisterEmitter(&xConfigB, 200u);
	ZENITH_ASSERT_EQ(uB, uA, "a fitting request reuses the retired SLOT");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uB).m_uBaseOffset, uBase, "...at the same base offset");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uB).m_uMaxParticles, 200u, "with the NEW config's live capacity");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uB).m_uReservedParticles, 256u, "over the original reservation");
	ZENITH_ASSERT_EQ(xGPU.m_uTotalAllocatedParticles, 256u, "and the watermark does not move");

	xGPU.UnregisterEmitter(uB);
}

ZENITH_TEST(ParticleGPU, ReusedSlotsRestartTheirRingEmpty)
{
	ScopedParticlePoolState xScope;
	Flux_ParticleGPUImpl& xGPU = xScope.m_xImpl;

	Flux_ParticleEmitterConfig xConfigA;
	Flux_ParticleEmitterConfig xConfigB;

	const u_int uA = xGPU.RegisterEmitter(&xConfigA, 32u);
	// Pretend the first emitter ran for a while.
	xGPU.m_axEmitters.Get(uA).m_uCurrentParticleCount = 32u;
	xGPU.m_axEmitters.Get(uA).m_uWriteCursor          = 17u;
	xGPU.m_axEmitters.Get(uA).m_uPendingSpawnCount    = 9u;
	xGPU.UnregisterEmitter(uA);

	const u_int uB = xGPU.RegisterEmitter(&xConfigB, 32u);
	ZENITH_ASSERT_EQ(uB, uA, "the slot is reused");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uB).m_uCurrentParticleCount, 0u, "the new emitter starts with no occupancy");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uB).m_uWriteCursor, 0u, "and writes from the head of the ring");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uB).m_uPendingSpawnCount, 0u,
		"inheriting the previous emitter's queued spawns would hatch its particles from the new config");

	xGPU.UnregisterEmitter(uB);
}

ZENITH_TEST(ParticleGPU, RegisterRefusesWhatThePoolCannotFit)
{
	ScopedParticlePoolState xScope;
	Flux_ParticleGPUImpl& xGPU = xScope.m_xImpl;

	Flux_ParticleEmitterConfig xConfig;

	ZENITH_ASSERT_EQ(xGPU.RegisterEmitter(&xConfig, Flux_ParticleGPUImpl::s_uMaxGPUParticles + 1u), UINT32_MAX,
		"a request larger than the whole pool is refused (the caller falls back to CPU simulation)");
	ZENITH_ASSERT_EQ(xGPU.m_uTotalAllocatedParticles, 0u, "a refused request must not consume the pool");

	ZENITH_ASSERT_EQ(xGPU.RegisterEmitter(nullptr, 16u), UINT32_MAX, "a null config is refused");
	ZENITH_ASSERT_EQ(xGPU.RegisterEmitter(&xConfig, 0u), UINT32_MAX, "a zero-particle emitter is refused");
	ZENITH_ASSERT_EQ(xGPU.m_uTotalAllocatedParticles, 0u, "and neither consumed the pool");
}

ZENITH_TEST(ParticleGPU, QueueSpawnOnlyReachesLiveRegistrations)
{
	ScopedParticlePoolState xScope;
	Flux_ParticleGPUImpl& xGPU = xScope.m_xImpl;

	Flux_ParticleEmitterConfig xConfig;
	const u_int uID = xGPU.RegisterEmitter(&xConfig, 64u);

	const Zenith_Maths::Vector3 xPos(1.0f, 2.0f, 3.0f);
	const Zenith_Maths::Vector3 xDir(0.0f, 1.0f, 0.0f);
	xGPU.QueueSpawn(uID, 4u, xPos, xDir);
	xGPU.QueueSpawn(uID, 3u, xPos, xDir);
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uPendingSpawnCount, 7u,
		"bursts within a frame accumulate — they are drained once, in PreExecuteCompute");

	// An out-of-range ID and a retired one must both be inert rather than writing
	// into whatever slot the index happens to land on.
	xGPU.QueueSpawn(uID + 99u, 5u, xPos, xDir);
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uPendingSpawnCount, 7u, "an out-of-range ID spawns nothing");

	xGPU.UnregisterEmitter(uID);
	xGPU.QueueSpawn(uID, 5u, xPos, xDir);
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uPendingSpawnCount, 0u, "a retired emitter spawns nothing");
}

ZENITH_TEST(ParticleGPU, ResetDropsQueuedSpawnsAndTheLatchButNotTheRings)
{
	// Reset is the SCENE lifecycle hook, and it is deliberately NARROW. Clearing a
	// still-registered emitter's ring here would desynchronise the CPU bookkeeping
	// from VRAM for any emitter that SURVIVES the load (a DontDestroyOnLoad one):
	// its particles would still be in the pool, still integrating and still drawing,
	// while the CPU believed the ring was empty and re-spawned over them from slot 0.
	// Ring state belongs to the EMITTER, and Unregister/Register already own it.
	ScopedParticlePoolState xScope;
	Flux_ParticleGPUImpl& xGPU = xScope.m_xImpl;

	Flux_ParticleEmitterConfig xConfig;
	const u_int uID = xGPU.RegisterEmitter(&xConfig, 64u);
	xGPU.m_axEmitters.Get(uID).m_uCurrentParticleCount = 40u;
	xGPU.m_axEmitters.Get(uID).m_uWriteCursor          = 40u;
	xGPU.m_axEmitters.Get(uID).m_uPendingSpawnCount    = 12u;
	xGPU.m_bActiveThisFrame = true;

	xGPU.Reset();

	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uPendingSpawnCount, 0u,
		"spawns queued on the frame the scene was torn down never fire");
	ZENITH_ASSERT_TRUE(!xGPU.IsActiveThisFrame(),
		"the frame latch drops, so a draw cannot follow a reset with args nothing re-seeded");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uCurrentParticleCount, 40u,
		"a SURVIVING emitter keeps its ring occupancy — its particles are still in the pool");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uWriteCursor, 40u,
		"...and its write cursor, or the next burst would overwrite live particles from slot 0");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uReservedParticles, 64u, "the reservation survives the scene");
	ZENITH_ASSERT_TRUE(xGPU.HasGPUEmitters(), "and the registration itself is untouched — its component still holds it");

	// Retiring the emitter is what clears the ring, wherever in the teardown it lands.
	xGPU.UnregisterEmitter(uID);
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uCurrentParticleCount, 0u, "unregistering clears the ring");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uWriteCursor, 0u, "cursor included");
}

// ---------------------------------------------------------------------------
// The frame's CPU half (the step that used to not exist)
// ---------------------------------------------------------------------------

ZENITH_TEST(ParticleGPU, PoolVramCoversTheWholeContract)
{
	// This pass allocated NOTHING before the path was wired (Initialise had no
	// caller). Pin the shapes the shader and the draw both assume, so a resize on
	// one side cannot quietly outgrow the other.
	Flux_ParticleGPUImpl& xGPU = g_xEngine.ParticleGPU();

	ZENITH_ASSERT_EQ(static_cast<u_int>(xGPU.m_xParticleBufferA.GetBuffer().m_ulSize),
		static_cast<u_int>(sizeof(Flux_Particle) * Flux_ParticleGPUImpl::s_uMaxGPUParticles),
		"pool half A must hold the whole particle pool");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xGPU.m_xParticleBufferB.GetBuffer().m_ulSize),
		static_cast<u_int>(xGPU.m_xParticleBufferA.GetBuffer().m_ulSize),
		"the ping-pong halves must be the same size — they swap roles every frame");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xGPU.GetInstanceBuffer().GetBuffer().m_ulSize),
		static_cast<u_int>(sizeof(Flux_ParticleInstance) * Flux_ParticleGPUImpl::s_uInstanceCapacity),
		"the instance buffer must cover BOTH blend partitions, or the additive draw's bind offset "
		"walks off the end");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xGPU.GetIndirectArgsBuffer().GetBuffer().m_ulSize),
		uFLUX_PARTICLE_INDIRECT_STRIDE * uFLUX_PARTICLE_PARTITION_COUNT,
		"one VkDrawIndexedIndirectCommand per blend partition");
	ZENITH_ASSERT_TRUE(xGPU.GetInstanceBuffer().GetBuffer().m_xVRAMHandle.IsValid(),
		"the instance buffer must actually be allocated — the draw binds it as a vertex stream");
	ZENITH_ASSERT_TRUE(xGPU.GetIndirectArgsBuffer().GetBuffer().m_xVRAMHandle.IsValid(),
		"and so must the indirect block the draw sources its instance counts from");
}

ZENITH_TEST(ParticleGPU, PreExecuteDrainsQueuedSpawnsAndArmsTheFrame)
{
	// The whole CPU half of a GPU particle frame, in order: a burst is queued by the
	// emitter tick, PreExecuteCompute drains it into the pool and arms the latch that
	// BOTH the dispatch and the draw read.
	ScopedParticlePoolState xScope;
	Flux_ParticleGPUImpl& xGPU = xScope.m_xImpl;

	// Stated, not assumed: the latch is (option AND live emitters), so this test and
	// the next only mean anything with the option on. It is on by default and no game
	// overrides it — a game that starts to must re-home these two rather than let them
	// pass vacuously.
	ZENITH_ASSERT_TRUE(Zenith_GraphicsOptions::Get().m_bGPUParticlesEnabled,
		"GPU particles are enabled by default engine-wide");

	Flux_ParticleEmitterConfig xConfig;
	xConfig.m_uMaxParticles = 64u;
	const u_int uID = xGPU.RegisterEmitter(&xConfig, 64u);
	ZENITH_ASSERT_TRUE(uID != UINT32_MAX, "the emptied pool must accept the emitter");

	xGPU.QueueSpawn(uID, 10u, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	xGPU.PreExecuteCompute();

	ZENITH_ASSERT_TRUE(xGPU.IsActiveThisFrame(),
		"a live registration with the option on arms the frame — the draw records its indirect draws off this");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uPendingSpawnCount, 0u, "the queue is drained, not re-spawned next frame");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uCurrentParticleCount, 10u, "ten slots of the ring are now live");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uWriteCursor, 10u, "and the next burst starts after them");

	// A second frame's burst continues around the ring rather than restarting.
	xGPU.QueueSpawn(uID, 60u, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	xGPU.PreExecuteCompute();
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uWriteCursor, 6u, "the burst wrapped past the end of the reservation");
	ZENITH_ASSERT_EQ(xGPU.m_axEmitters.Get(uID).m_uCurrentParticleCount, 64u, "and the ring is now saturated");

	xGPU.UnregisterEmitter(uID);
}

ZENITH_TEST(ParticleGPU, PreExecuteDisarmsTheFrameWithNoEmitters)
{
	// The empty case has to DISARM, not just skip work: the draw is recorded off the
	// same latch, and drawing from indirect args nothing seeded this frame would
	// replay the last armed frame's instance counts against a pool nothing refilled.
	ScopedParticlePoolState xScope;
	Flux_ParticleGPUImpl& xGPU = xScope.m_xImpl;

	ZENITH_ASSERT_TRUE(Zenith_GraphicsOptions::Get().m_bGPUParticlesEnabled,
		"GPU particles are enabled by default engine-wide");

	xGPU.m_bActiveThisFrame = true;
	xGPU.PreExecuteCompute();
	ZENITH_ASSERT_TRUE(!xGPU.IsActiveThisFrame(), "no registered emitters means no GPU particle draw this frame");

	Flux_ParticleEmitterConfig xConfig;
	const u_int uID = xGPU.RegisterEmitter(&xConfig, 32u);
	xGPU.PreExecuteCompute();
	ZENITH_ASSERT_TRUE(xGPU.IsActiveThisFrame(), "...and registering one arms it again");

	xGPU.UnregisterEmitter(uID);
	xGPU.PreExecuteCompute();
	ZENITH_ASSERT_TRUE(!xGPU.IsActiveThisFrame(),
		"a retired emitter disarms it again — a kept RESERVATION is not a live emitter");
}

ZENITH_TEST(ParticleGPU, GetEmitterParticleCountIsBoundsChecked)
{
	ScopedParticlePoolState xScope;
	Flux_ParticleGPUImpl& xGPU = xScope.m_xImpl;

	// The tools panel calls this with whatever ID the component holds; an
	// out-of-range one must read 0, not walk off the vector.
	ZENITH_ASSERT_EQ(xGPU.GetEmitterParticleCount(UINT32_MAX), 0u, "an unregistered component reads zero");
	ZENITH_ASSERT_EQ(xGPU.GetEmitterParticleCount(0u), 0u, "so does an ID into an empty pool");

	Flux_ParticleEmitterConfig xConfig;
	const u_int uID = xGPU.RegisterEmitter(&xConfig, 16u);
	xGPU.m_axEmitters.Get(uID).m_uCurrentParticleCount = 11u;
	ZENITH_ASSERT_EQ(xGPU.GetEmitterParticleCount(uID), 11u, "a live emitter reports its ring occupancy");

	xGPU.UnregisterEmitter(uID);
}
