#pragma once

#include "Maths/Zenith_Maths.h"
#include "Flux/Flux_VertexCodec.h"   // the ONE place the packed colour lane is quantised
#include "Flux/Shaders/Generated/Particles.h"   // the baked vertex layout the pins below compare against

#include <cstddef>   // offsetof

/**
 * GPU-compatible particle data (96 bytes, std430 aligned)
 * Uses Vector4 throughout to avoid GPU alignment issues.
 *
 * This struct is used for both CPU and GPU simulation paths.
 * For GPU compute, it must match the GLSL struct layout exactly.
 */
struct Flux_Particle
{
	Zenith_Maths::Vector4 m_xPosition_Age;      // xyz=position, w=age       (16 bytes)
	Zenith_Maths::Vector4 m_xVelocity_Lifetime; // xyz=velocity, w=lifetime  (32 bytes)
	Zenith_Maths::Vector4 m_xColorStart;        // rgba                      (48 bytes)
	Zenith_Maths::Vector4 m_xColorEnd;          // rgba                      (64 bytes)
	Zenith_Maths::Vector4 m_xSizeRotation;      // x=sizeStart, y=sizeEnd, z=rotation, w=rotationSpeed (80 bytes)
	Zenith_Maths::Vector4 m_xPadding;           // reserved for future use   (96 bytes)

	// Helper accessors for readability
	Zenith_Maths::Vector3 GetPosition() const { return Zenith_Maths::Vector3(m_xPosition_Age); }
	void SetPosition(const Zenith_Maths::Vector3& xPos) { m_xPosition_Age = Zenith_Maths::Vector4(xPos, m_xPosition_Age.w); }

	float GetAge() const { return m_xPosition_Age.w; }
	void SetAge(float fAge) { m_xPosition_Age.w = fAge; }

	Zenith_Maths::Vector3 GetVelocity() const { return Zenith_Maths::Vector3(m_xVelocity_Lifetime); }
	void SetVelocity(const Zenith_Maths::Vector3& xVel) { m_xVelocity_Lifetime = Zenith_Maths::Vector4(xVel, m_xVelocity_Lifetime.w); }

	float GetLifetime() const { return m_xVelocity_Lifetime.w; }
	void SetLifetime(float fLifetime) { m_xVelocity_Lifetime.w = fLifetime; }

	float GetSizeStart() const { return m_xSizeRotation.x; }
	float GetSizeEnd() const { return m_xSizeRotation.y; }
	float GetRotation() const { return m_xSizeRotation.z; }
	float GetRotationSpeed() const { return m_xSizeRotation.w; }

	void SetSizeStart(float fSize) { m_xSizeRotation.x = fSize; }
	void SetSizeEnd(float fSize) { m_xSizeRotation.y = fSize; }
	void SetRotation(float fRot) { m_xSizeRotation.z = fRot; }
	void SetRotationSpeed(float fSpeed) { m_xSizeRotation.w = fSpeed; }

	// Check if particle is alive
	bool IsAlive() const { return GetAge() < GetLifetime(); }

	// Get normalized lifetime (0.0 to 1.0)
	float GetNormalizedAge() const
	{
		float fLifetime = GetLifetime();
		return (fLifetime > 0.0f) ? (GetAge() / fLifetime) : 1.0f;
	}

	// Get interpolated color based on current age
	Zenith_Maths::Vector4 GetCurrentColor() const
	{
		float t = GetNormalizedAge();
		return glm::mix(m_xColorStart, m_xColorEnd, t);
	}

	// Get interpolated size based on current age
	float GetCurrentSize() const
	{
		float t = GetNormalizedAge();
		return glm::mix(GetSizeStart(), GetSizeEnd(), t);
	}
};

/**
 * Per-particle instance data for rendering (20 bytes)
 * This is the minimal data needed by the vertex shader for billboarding.
 *
 * The position/size lane stays a float4: it holds a WORLD position, so it can be
 * arbitrarily far from the origin, and every narrower float format loses integer
 * exactness past 2048 — a particle a kilometre out would snap to a visible grid.
 * The colour is unorm8x4: every emitter colour in the engine is authored in [0,1]
 * and this lane only ever carries a lerp between two such endpoints.
 *
 * TWO PRODUCERS write this layout — this struct (the CPU emitter path) and the
 * ParticleUpdate compute shader, which writes the same bytes as raw u32 words
 * because a Slang structured-buffer element carrying a float4 would be std430-padded
 * to 32 B. uFLUX_PARTICLE_INSTANCE_WORDS below is the shared word count, pinned
 * against the generated stride so the two producers cannot drift apart silently.
 */
struct Flux_ParticleInstance
{
	Zenith_Maths::Vector4 m_xPosition_Size; // xyz=position, w=size
	u_int                 m_uColor;         // rgba, unorm8x4

	Flux_ParticleInstance() = default;

	Flux_ParticleInstance(const Zenith_Maths::Vector3& xPos, float fSize, const Zenith_Maths::Vector4& xColor)
		: m_xPosition_Size(xPos.x, xPos.y, xPos.z, fSize)
		, m_uColor(Flux_PackUnorm8x4(xColor))
	{
	}

	void SetColour(const Zenith_Maths::Vector4& xColor) { m_uColor = Flux_PackUnorm8x4(xColor); }
	Zenith_Maths::Vector4 GetColour() const { return Flux_UnpackUnorm8x4(m_uColor); }

	// Create instance from a Flux_Particle
	static Flux_ParticleInstance FromParticle(const Flux_Particle& xParticle)
	{
		return Flux_ParticleInstance(
			xParticle.GetPosition(),
			xParticle.GetCurrentSize(),
			xParticle.GetCurrentColor()
		);
	}
};

// The compute writer's stride, in u32 words. Mirrors uINSTANCE_WORDS in
// Flux/Shaders/Particles/Flux_ParticleUpdate.slang. The static_assert below ties
// THIS constant to the generated stride — it cannot see the shader's literal
// (nothing reflects a plain `static const uint`; the sidecar carries only
// [SpecializationConstant] decls). The cross-language tie is the SOURCE-TEXT pin
// `ParticleInstance.SlangWriterWordCountMatchesTheMirror`, which reads the .slang
// and parses the literal — without it, bumping one spelling compiles and tests
// green while the compute pass strides the buffer wrong.
inline constexpr u_int uFLUX_PARTICLE_INSTANCE_WORDS = 5u;

// Compile-time size verification
static_assert(sizeof(Flux_Particle) == 96, "Flux_Particle must be 96 bytes for GPU alignment");
static_assert(sizeof(Flux_ParticleInstance) == 20, "Flux_ParticleInstance must be 20 bytes");

// ...and the same 20 bytes as the Particles program's baked per-instance layout, field
// by field. The size assert above cannot see a swapped position/colour pair; these can.
// Elements 0/1 of that layout are the shared unit quad on binding 0.
static_assert(sizeof(Flux_ParticleInstance) == Flux_Generated_Particles::Particles::kVertexLayout.m_auStrides[1],
	"Flux_ParticleInstance must match the per-instance stride the Particles VS fetches");
static_assert(uFLUX_PARTICLE_INSTANCE_WORDS * 4u == Flux_Generated_Particles::Particles::kVertexLayout.m_auStrides[1],
	"Flux_ParticleUpdate.slang's uINSTANCE_WORDS must stride the instance buffer exactly as the Particles VS fetches it");
static_assert(offsetof(Flux_ParticleInstance, m_xPosition_Size) == Flux_Generated_Particles::Particles::kaxVertexAttribs[2].m_uOffset,
	"Flux_ParticleInstance.m_xPosition_Size drifted from the generated per-instance layout");
static_assert(offsetof(Flux_ParticleInstance, m_uColor) == Flux_Generated_Particles::Particles::kaxVertexAttribs[3].m_uOffset,
	"Flux_ParticleInstance.m_uColor drifted from the generated per-instance layout");

// Full-table pin: a shader-side swap of the two instance lanes would be invisible to
// the offsets above — only a semantic/type comparison can see it. Pinning the storage
// types is also what catches a dropped [VtxFmt] tag re-widening the stream to 32 B.
inline constexpr Flux_VertexLayoutElement kaxFLUX_PARTICLE_EXPECTED_LAYOUT[] =
{
	{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3,   0u,  0u },
	{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2,   0u, 12u },
	{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 1u, SHADER_DATA_TYPE_FLOAT4,   1u,  0u },
	{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 2u, SHADER_DATA_TYPE_UNORM8X4, 1u, 16u },
};
static_assert(Flux_Generated_Particles::Particles::kVertexLayout == Flux_VertexLayoutDesc{ kaxFLUX_PARTICLE_EXPECTED_LAYOUT, 4u, { 20u, 20u } },
	"The Particles program's vertex layout drifted from the pinned contract — re-derive the expected table consciously if the VsIn really changed");
