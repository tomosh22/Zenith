// =====================================================================
// Flux_ParticleUpdate — Slang module.
//
// Subsystem: Particles. Compute particle simulation step. One thread
// per particle: integrate velocity (gravity + drag + per-particle
// turbulence), advance age, write the updated particle to the next-
// frame buffer, and emit a render instance for surviving particles to
// a compacted instance buffer.
//
// Buffer layout:
//   set=0 binding=0  InputParticles      (read)
//   set=0 binding=1  PushConstants       (CB)
//   set=0 binding=2  OutputParticles     (write)
//   set=0 binding=3  InstanceBuffer      (write)
//   set=0 binding=4  aliveCount          (atomic uint)
// =====================================================================

// Pure compute on its own buffers — uses no FrameConstants. Still #include the
// spine so it reserves sets 0/1/2 and this shader's resources (PassParams) land
// at set 3, identical to every other converted shader.
#include "Common/Bindings.slang"

struct Particle
{
	float4 positionAge;       // xyz pos, w age
	float4 velocityLifetime;  // xyz vel, w lifetime
	float4 colorStart;
	float4 colorEnd;
	float4 sizeRotation;      // x sizeStart, y sizeEnd, z rotation, w rotationSpeed
	float4 padding;
};

struct RenderInstance
{
	float4 positionSize; // xyz pos, w size
	float4 color;
};

struct PushConstantsLayout
{
	float  deltaTime;
	uint   particleCount;
	float  turbulence;
	float  pad1;
	float4 gravity; // xyz gravity, w drag
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
	RWStructuredBuffer<RenderInstance> InstanceBuffer;
	RWStructuredBuffer<uint>           aliveCount;
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
	uint idx = dispatchId.x;
	if (idx >= g_xPassSet.PushConstants.particleCount) return;

	Particle p = g_xPassSet.InputParticles[idx];

	float age      = p.positionAge.w;
	float lifetime = p.velocityLifetime.w;

	// Dead-on-entry: write through unchanged so the buffer stays
	// consistent (subsequent dispatches won't see stale slots).
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

	uint instanceIdx;
	InterlockedAdd(g_xPassSet.aliveCount[0], 1u, instanceIdx);

	// Defensive bounds check — instance buffer is sized to particleCount,
	// but a livelock in alive accounting could otherwise overrun.
	if (instanceIdx >= g_xPassSet.PushConstants.particleCount) return;

	g_xPassSet.InstanceBuffer[instanceIdx].positionSize = float4(p.positionAge.xyz, size);
	g_xPassSet.InstanceBuffer[instanceIdx].color        = color;
}
