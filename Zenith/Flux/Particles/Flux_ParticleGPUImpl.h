#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Particles/Flux_ParticleData.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include <random>

class Flux_ParticleEmitterConfig;

// ---------------------------------------------------------------------------
// GPU contract — PINNED against Zenith/Flux/Shaders/Particles/Flux_ParticleUpdate.slang.
// ---------------------------------------------------------------------------

// The two blend partitions of the shared instance buffer. An emitter's config
// picks one; the compute dispatch for that emitter appends only into it, and the
// draw records one DrawIndexedIndirect per partition with the matching pipeline.
// Splitting on the GPU side (rather than per-particle) is what lets the particle
// record stay 96 B with no emitter back-reference: ONE DISPATCH PER EMITTER means
// the blend mode is a push constant.
inline constexpr u_int uFLUX_PARTICLE_PARTITION_ALPHA    = 0u;
inline constexpr u_int uFLUX_PARTICLE_PARTITION_ADDITIVE = 1u;
inline constexpr u_int uFLUX_PARTICLE_PARTITION_COUNT    = 2u;

// VkDrawIndexedIndirectCommand's five words. Mirrors uINDIRECT_WORDS in
// Flux_ParticleUpdate.slang (held to it by the source-text pin
// ParticleGPU.SlangIndirectWordCountMatchesTheMirror — a plain `static const uint`
// reflects into nothing a static_assert can reach).
inline constexpr u_int uFLUX_PARTICLE_INDIRECT_WORDS  = 5u;
inline constexpr u_int uFLUX_PARTICLE_INDIRECT_STRIDE = uFLUX_PARTICLE_INDIRECT_WORDS * 4u;   // bytes

// The billboard is the shared unit quad: two triangles, six indices. It is the
// indexCount the CPU seeds into every partition's draw command.
inline constexpr u_int uFLUX_PARTICLE_QUAD_INDEX_COUNT = 6u;

// ---------------------------------------------------------------------------
// Ring-buffered spawn addressing (pure — unit-tested in Flux_ParticleGPU.Tests.inl).
//
// An emitter's reservation is a RING, not a high-water stack. The GPU sim writes
// every slot through in place (a dead particle keeps its slot), so there is no GPU
// free list to consult and nothing ever hands a slot back; a cursor that only went
// forwards would wedge the emitter permanently after uCapacity lifetime spawns.
// Overwriting the OLDEST slot is both the cheap answer and the right one — under
// saturation the oldest particle is the one closest to death.
//
// A burst can straddle the wrap, so the upload is expressed as up to two contiguous
// runs; the second (when non-empty) always starts at slot 0 of the reservation.
// ---------------------------------------------------------------------------
struct Flux_ParticleSpawnRuns
{
	u_int m_uFirstStart  = 0u;   // slot offset within the emitter's reservation
	u_int m_uFirstCount  = 0u;
	u_int m_uSecondCount = 0u;   // starts at reservation slot 0 when non-zero
	u_int m_uNextCursor  = 0u;   // cursor after the burst (already wrapped)

	u_int TotalCount() const { return m_uFirstCount + m_uSecondCount; }
};

inline Flux_ParticleSpawnRuns Flux_SplitParticleSpawnRing(u_int uCursor, u_int uCount, u_int uCapacity)
{
	Flux_ParticleSpawnRuns xRuns;
	if (uCapacity == 0u)
	{
		return xRuns;
	}

	// A burst larger than the ring would overwrite its own earlier records within
	// the same upload; only the last uCapacity of them could survive, so clamp
	// rather than pretend the surplus was spawned.
	const u_int uClamped = (uCount < uCapacity) ? uCount : uCapacity;

	xRuns.m_uFirstStart = uCursor % uCapacity;
	const u_int uToEnd  = uCapacity - xRuns.m_uFirstStart;

	xRuns.m_uFirstCount  = (uClamped < uToEnd) ? uClamped : uToEnd;
	xRuns.m_uSecondCount = uClamped - xRuns.m_uFirstCount;
	xRuns.m_uNextCursor  = (xRuns.m_uFirstStart + uClamped) % uCapacity;
	return xRuns;
}

// Ring occupancy after a burst. It SATURATES rather than wrapping: once the ring
// is full every further spawn recycles a slot, so the count of slots ever written
// stops growing. (This is not an alive count — only the GPU knows that.)
inline u_int Flux_AdvanceParticleRingOccupancy(u_int uOccupancy, u_int uSpawned, u_int uCapacity)
{
	const u_int uNext = uOccupancy + uSpawned;
	return (uNext < uCapacity) ? uNext : uCapacity;
}

/**
 * GPU-based particle compute system.
 *
 * Uses compute shaders to update particles on the GPU, with double-buffered
 * particle storage (ping-pong pattern) for read/write separation. The pass is
 * fully GPU-driven: the compute step compacts surviving particles into a
 * per-blend-mode partition of a vertex-usable instance buffer and bumps that
 * partition's DrawIndexedIndirect instanceCount, so nothing is ever read back
 * to the CPU to size the draw.
 *
 * Frame shape (all four steps live under the "Particles" feature):
 *   PreExecuteCompute (Prepare)  latch enabled-ness, upload queued spawns, seed
 *                                both partitions' indirect commands
 *   DispatchCompute   (record)   one dispatch per registered emitter
 *   ExecuteParticles  (record)   two DrawIndexedIndirect, one per partition
 *
 * Emitters register through Zenith_ParticleEmitterComponent when their config
 * carries m_bUseGPUCompute.
 */
class Flux_ParticleGPUImpl
{
public:
	static constexpr uint32_t s_uMaxGPUParticles = 4096;
	static constexpr uint32_t s_uWorkgroupSize   = 64;

	// Each blend partition is sized for the WHOLE pool. That is not slack: a
	// particle emits at most one instance and the pool is shared across blend
	// modes, so neither partition can ever exceed it however the emitters split —
	// which is what removes the need for a grass-style indirect-fixup clamp pass.
	static constexpr uint32_t s_uPartitionCapacity = s_uMaxGPUParticles;
	static constexpr uint32_t s_uInstanceCapacity  = s_uPartitionCapacity * uFLUX_PARTICLE_PARTITION_COUNT;

	Flux_ParticleGPUImpl() = default;
	~Flux_ParticleGPUImpl() = default;

	Flux_ParticleGPUImpl(const Flux_ParticleGPUImpl&) = delete;
	Flux_ParticleGPUImpl& operator=(const Flux_ParticleGPUImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Shutdown();
	void Reset();

	/**
	 * Register a GPU emitter for compute processing.
	 * Returns an emitter ID for later reference, or UINT32_MAX when the pool
	 * cannot fit the request (the caller then falls back to CPU simulation).
	 */
	uint32_t RegisterEmitter(Flux_ParticleEmitterConfig* pxConfig, uint32_t uMaxParticles);

	/**
	 * Unregister a GPU emitter. The pool reservation is KEPT and offered back to
	 * the next RegisterEmitter that fits it — the pool is carved by base offset,
	 * so releasing the range outright would fragment it and a
	 * register/unregister cycle would exhaust the pool.
	 */
	void UnregisterEmitter(uint32_t uEmitterID);

	/**
	 * Queue particle spawns for a GPU emitter.
	 * Particles will be spawned on the next compute dispatch.
	 */
	void QueueSpawn(uint32_t uEmitterID, uint32_t uCount,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xDirection);

	/**
	 * Spawn particles immediately into the GPU buffer.
	 * Called during PreExecuteCompute to process pending spawns.
	 */
	void ProcessPendingSpawns();

	/**
	 * CPU-side pre-execute: latch enabled-ness, process spawns, seed the indirect
	 * commands. Runs as the tail of the Particles feature's single Prepare, after
	 * the emitter tick that produced this frame's QueueSpawn calls.
	 */
	void PreExecuteCompute();

	/**
	 * Record compute shader commands to update all GPU particles.
	 */
	void DispatchCompute(Flux_CommandBuffer* pxCmdList);

	/**
	 * The compute-written per-instance vertex stream. Bound as vertex binding 1 by
	 * ExecuteParticles at the partition's byte base (created with vertex-buffer
	 * usage alongside its UAV).
	 */
	Flux_ReadWriteBuffer& GetInstanceBuffer() { return m_xInstanceBuffer; }

	/**
	 * uFLUX_PARTICLE_PARTITION_COUNT VkDrawIndexedIndirectCommands, seeded by the
	 * CPU each frame and instance-counted by the compute pass.
	 */
	Flux_IndirectBuffer& GetIndirectArgsBuffer() { return m_xIndirectArgsBuffer; }

	/** Byte offset of a blend partition's first instance in the instance buffer. */
	static size_t GetPartitionByteOffset(u_int uPartition)
	{
		return static_cast<size_t>(uPartition) * s_uPartitionCapacity * sizeof(Flux_ParticleInstance);
	}

	/** Byte offset of a blend partition's draw command in the indirect buffer. */
	static uint32_t GetPartitionArgsByteOffset(u_int uPartition)
	{
		return uPartition * uFLUX_PARTICLE_INDIRECT_STRIDE;
	}

	/**
	 * Latched once per frame in PreExecuteCompute and read by BOTH the dispatch and
	 * the draw. The two must agree: a draw whose indirect args no PreExecute seeded
	 * this frame would replay the last enabled frame's instance counts against a
	 * buffer nothing refilled.
	 */
	bool IsActiveThisFrame() const { return m_bActiveThisFrame; }

	/**
	 * Check if any GPU emitters are registered.
	 */
	bool HasGPUEmitters() const;

	/**
	 * Slots this emitter has spawned into (its ring occupancy, saturating at the
	 * reservation) — NOT an alive count, which only the GPU knows. Drives the
	 * emitter component's tools panel.
	 */
	uint32_t GetEmitterParticleCount(uint32_t uEmitterID) const;

	/**
	 * The instanceCount the LAST compute dispatch counted into a blend partition —
	 * i.e. how many particles that partition's indirect draw is about to draw.
	 *
	 * EXPLICIT SLOW PATH: DownloadBufferData drains staged writes and idles the
	 * device. Never call it from a frame path. Headless it is 0 by construction
	 * (the GPU-less download zero-fills), so it is WINDOWED-ONLY truth and must
	 * never be asserted on in a `Null_` test. It exists because this is the one
	 * fact no CPU-side state can stand in for: whether the compute pass really
	 * produced the instances the draw consumes.
	 */
	uint32_t ReadbackPartitionInstanceCount(u_int uPartition);

	struct EmitterData
	{
		Flux_ParticleEmitterConfig* m_pxConfig            = nullptr;
		uint32_t                    m_uReservedParticles  = 0;   // pool slots owned forever (never re-carved)
		uint32_t                    m_uMaxParticles       = 0;   // slots the CURRENT config uses (<= reserved)
		uint32_t                    m_uBaseOffset         = 0;
		uint32_t                    m_uCurrentParticleCount = 0; // ring occupancy, saturates at m_uMaxParticles
		uint32_t                    m_uWriteCursor        = 0;   // next ring slot to overwrite

		uint32_t                    m_uPendingSpawnCount  = 0;
		Zenith_Maths::Vector3       m_xSpawnPosition;
		Zenith_Maths::Vector3       m_xSpawnDirection;
	};

	Zenith_Vector<EmitterData> m_axEmitters;
	uint32_t                   m_uTotalAllocatedParticles = 0;

	Flux_Particle*             m_pxStagingBuffer        = nullptr;
	uint32_t                   m_uStagingBufferSize     = 0;

	std::mt19937                          m_xRng{ std::random_device{}() };
	std::uniform_real_distribution<float> m_xDist{ 0.0f, 1.0f };

	Flux_ReadWriteBuffer m_xParticleBufferA;
	Flux_ReadWriteBuffer m_xParticleBufferB;
	bool                 m_bUseBufferA = true;

	Flux_ReadWriteBuffer m_xInstanceBuffer;
	Flux_IndirectBuffer  m_xIndirectArgsBuffer;

	bool                 m_bActiveThisFrame = false;

	Flux_Pipeline    m_xComputePipeline;
	Flux_Shader      m_xComputeShader;
	Flux_RootSig     m_xComputeRootSig;

private:
	// Upload a zero-instanceCount VkDrawIndexedIndirectCommand for every partition.
	void SeedIndirectCommands();

	// Blank an emitter's whole reservation in BOTH pool halves, so a reused slot
	// cannot inherit the previous emitter's live particles.
	void ZeroEmitterRange(const EmitterData& xEmitter);
};

// THE reason there is no indirect-fixup clamp pass (the grass pipeline needs one):
// a particle emits at most ONE instance and the pool is shared across blend modes,
// so an all-alpha or all-additive frame still fits its partition. Shrinking a
// partition below the pool would make the compute pass's bounds check load-bearing
// and start silently dropping particles.
static_assert(Flux_ParticleGPUImpl::s_uPartitionCapacity >= Flux_ParticleGPUImpl::s_uMaxGPUParticles,
	"a blend partition must hold the WHOLE particle pool");
