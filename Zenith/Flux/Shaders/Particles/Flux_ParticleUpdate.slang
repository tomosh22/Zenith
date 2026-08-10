// =====================================================================
// Flux_ParticleUpdate — Slang module.
//
// Subsystem: Particles. Compute particle simulation step. One thread
// per particle SLOT OF ONE EMITTER: integrate velocity (gravity + drag
// + per-particle turbulence), advance age, write the updated particle
// to the next-frame buffer, and emit a render instance for surviving
// particles into that emitter's blend partition of the shared instance
// buffer — bumping the partition's DrawIndexedIndirect instanceCount as
// it goes, so the draw never needs a CPU-visible alive count.
//
// ONE DISPATCH PER EMITTER. The pool is a flat array carved into
// per-emitter reservations, and every parameter that varies per emitter
// (gravity, drag, turbulence, blend partition) rides the push constants
// rather than a per-particle field — which is why a particle record has
// no emitter back-reference and no parameter copy. baseOffset/particleCount
// select the emitter's slice; the launcher is Flux_ParticleGPUImpl::
// DispatchCompute.
//
// Buffer layout (PASS set, space 3):
//   binding=0  PushConstants       (CB)
//   binding=1  InputParticles      (read)
//   binding=2  OutputParticles     (write)
//   binding=3  InstanceBuffer      (write)
//   binding=4  IndirectArgs        (atomic VkDrawIndexedIndirectCommand[2])
// =====================================================================

// Pure compute on its own buffers — uses no FrameConstants. Still #include the
// spine so it reserves sets 0/1/2 and this shader's resources (PassParams) land
// at set 3, identical to every other converted shader.
#include "Common/Bindings.slang"
#include "Common/VertexFormats.slang"  // Flux_PackUnorm8x4 — the colour lane's codec

struct Particle
{
	float4 positionAge;       // xyz pos, w age
	float4 velocityLifetime;  // xyz vel, w lifetime
	float4 colorStart;
	float4 colorEnd;
	float4 sizeRotation;      // x sizeStart, y sizeEnd, z rotation, w rotationSpeed
	float4 padding;
};

// The render instance this pass emits is the SAME 20-byte per-instance vertex
// Flux_ParticleInstance describes on the CPU and Flux_Particles.slang's VsIn
// fetches — float3 position, float size, then a unorm8x4 colour word. The draw
// binds THIS buffer as vertex binding 1 (an indirect DrawIndexed per partition),
// so these bytes are fetched by the vertex unit, not read back.
//
// It is written as RAW u32 WORDS rather than through a `struct { float4; uint; }`
// because a Slang structured-buffer element is std430-laid-out: the float4 forces
// 16-byte element alignment, so such a struct would occupy 32 bytes and stride the
// buffer at 32 while the vertex fetch strides it at 20. Five explicit words are the
// only way to write the packed layout (the same reason Common/VertexFormats.slang's
// manual codecs exist for the skinning arena). uINSTANCE_WORDS is mirrored by
// uFLUX_PARTICLE_INSTANCE_WORDS in Flux/Particles/Flux_ParticleData.h; the C++
// static_assert pins THAT mirror against the generated stride, and the source-text
// pin ParticleInstance.SlangWriterWordCountMatchesTheMirror holds THIS literal to
// the mirror (a plain static const uint reflects into nothing a build can check).
// Keep the declaration on one line — the pin parses it.
static const uint uINSTANCE_WORDS = 5u;

// VkDrawIndexedIndirectCommand: [indexCount, instanceCount, firstIndex,
// vertexOffset, firstInstance]. The CPU seeds one command per blend partition
// each frame (Flux_PackResetIndirectCommand) and this pass atomically increments
// word 1. Mirrored by uFLUX_PARTICLE_INDIRECT_WORDS in Flux_ParticleGPUImpl.h and
// held to it by the source-text pin ParticleGPU.SlangIndirectWordCountMatchesTheMirror.
// Keep the declaration on one line — the pin parses it.
static const uint uINDIRECT_WORDS = 5u;

struct PushConstantsLayout
{
	float  deltaTime;
	uint   baseOffset;        // this emitter's first slot in the shared pool
	uint   particleCount;     // slots this emitter owns (the dispatch's thread bound)
	float  turbulence;
	float4 gravity;           // xyz gravity, w drag
	uint   instanceBase;      // first instance slot of this emitter's blend partition
	uint   instanceCapacity;  // instances that partition can hold
	uint   argsWordBase;      // word offset of that partition's draw command
	uint   pad0;
};

// PASS frequency. Declared AFTER the spine #include, so it lands at set 3
// (the first set past the persistent 0/1/2). Member names preserved exactly —
// the C++ binds key on them (h<member>).
// All buffers are RW because compute writes; the input slot is treated
// as read-only by the shader logic.
struct PassParams
{
	ConstantBuffer<PushConstantsLayout> PushConstants;
	RWStructuredBuffer<Particle>       InputParticles;
	RWStructuredBuffer<Particle>       OutputParticles;
	RWStructuredBuffer<uint>           InstanceBuffer;
	RWStructuredBuffer<uint>           IndirectArgs;
};
ParameterBlock<PassParams> g_xPassSet;

// PCG hash — small, well-distributed, no global state. Used for cheap
// per-particle deterministic randomness driving turbulence jitter.
uint pcgHash(uint x)
{
	uint state = x * 747796405u + 2891336453u;
	uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float randomFloat(uint seed)
{
	return (float)pcgHash(seed) / (float)0xFFFFFFFFu;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	uint local = dispatchId.x;
	if (local >= g_xPassSet.PushConstants.particleCount) return;

	// The dispatch owns [baseOffset, baseOffset + particleCount) of the shared pool.
	uint idx = g_xPassSet.PushConstants.baseOffset + local;

	Particle p = g_xPassSet.InputParticles[idx];

	float age      = p.positionAge.w;
	float lifetime = p.velocityLifetime.w;

	// Dead-on-entry: write through unchanged so the buffer stays
	// consistent (subsequent dispatches won't see stale slots). A never-spawned
	// slot is all-zero (age 0, lifetime 0), so it takes this branch too.
	if (age >= lifetime)
	{
		g_xPassSet.OutputParticles[idx] = p;
		return;
	}

	// Integrate.
	age              += g_xPassSet.PushConstants.deltaTime;
	p.positionAge.w   = age;

	float3 velocity = p.velocityLifetime.xyz;
	velocity       += g_xPassSet.PushConstants.gravity.xyz * g_xPassSet.PushConstants.deltaTime;
	float drag      = g_xPassSet.PushConstants.gravity.w;
	if (drag > 0.0) velocity *= (1.0 - drag * g_xPassSet.PushConstants.deltaTime);

	if (g_xPassSet.PushConstants.turbulence > 0.0)
	{
		// Time-varying seed so the same particle gets different jitter
		// each frame. age * 1000 → ms-scale variation.
		uint seed = idx * 3u + (uint)(age * 1000.0);
		float3 jitter = float3(
			randomFloat(seed)      * 2.0 - 1.0,
			randomFloat(seed + 1u) * 2.0 - 1.0,
			randomFloat(seed + 2u) * 2.0 - 1.0) * g_xPassSet.PushConstants.turbulence;
		velocity += jitter * g_xPassSet.PushConstants.deltaTime;
	}

	p.velocityLifetime.xyz = velocity;
	p.positionAge.xyz     += velocity * g_xPassSet.PushConstants.deltaTime;

	float rotation       = p.sizeRotation.z;
	float rotationSpeed  = p.sizeRotation.w;
	p.sizeRotation.z     = rotation + rotationSpeed * g_xPassSet.PushConstants.deltaTime;

	g_xPassSet.OutputParticles[idx] = p;

	if (age >= lifetime) return; // just died this frame — skip render emission

	float normalizedAge = age / lifetime;
	float4 color        = lerp(p.colorStart, p.colorEnd, normalizedAge);
	float  size         = lerp(p.sizeRotation.x, p.sizeRotation.y, normalizedAge);

	// Claim a slot in this emitter's blend partition by bumping the partition's
	// indirect instanceCount. The returned pre-increment value IS the slot, so
	// the compaction and the draw count come out of one atomic.
	uint instanceSlot;
	InterlockedAdd(g_xPassSet.IndirectArgs[g_xPassSet.PushConstants.argsWordBase + 1u], 1u, instanceSlot);

	// Defensive bounds check. It cannot fire with today's capacities (each
	// partition is sized for the WHOLE pool, and a particle emits at most one
	// instance), which is exactly why there is no indirect-fixup pass clamping
	// the count afterwards — but a capacity edit that broke that must corrupt
	// nothing, so the write is still guarded.
	if (instanceSlot >= g_xPassSet.PushConstants.instanceCapacity) return;

	// Lane order matches the vertex layout exactly: world position xyz, radius,
	// then the packed colour. asuint is a bit reinterpretation, not a conversion.
	const uint uBase = (g_xPassSet.PushConstants.instanceBase + instanceSlot) * uINSTANCE_WORDS;
	g_xPassSet.InstanceBuffer[uBase + 0u] = asuint(p.positionAge.x);
	g_xPassSet.InstanceBuffer[uBase + 1u] = asuint(p.positionAge.y);
	g_xPassSet.InstanceBuffer[uBase + 2u] = asuint(p.positionAge.z);
	g_xPassSet.InstanceBuffer[uBase + 3u] = asuint(size);
	g_xPassSet.InstanceBuffer[uBase + 4u] = Flux_PackUnorm8x4(color);
}
