#pragma once

#include "Maths/Zenith_Maths.h"
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
 * Per-particle instance data for rendering (32 bytes)
 * This is the minimal data needed by the vertex shader for billboarding.
 */
struct Flux_ParticleInstance
{
	Zenith_Maths::Vector4 m_xPosition_Size; // xyz=position, w=size
	Zenith_Maths::Vector4 m_xColor;         // rgba

	Flux_ParticleInstance() = default;

	Flux_ParticleInstance(const Zenith_Maths::Vector3& xPos, float fSize, const Zenith_Maths::Vector4& xColor)
		: m_xPosition_Size(xPos.x, xPos.y, xPos.z, fSize)
		, m_xColor(xColor)
	{
	}

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

// Compile-time size verification
static_assert(sizeof(Flux_Particle) == 96, "Flux_Particle must be 96 bytes for GPU alignment");
static_assert(sizeof(Flux_ParticleInstance) == 32, "Flux_ParticleInstance must be 32 bytes");

// ...and the same 32 bytes as the Particles program's baked per-instance layout, field
// by field. The size assert above cannot see a swapped position/colour pair; these can.
// Elements 0/1 of that layout are the shared unit quad on binding 0.
static_assert(sizeof(Flux_ParticleInstance) == Flux_Generated_Particles::Particles::kVertexLayout.m_auStrides[1],
	"Flux_ParticleInstance must match the per-instance stride the Particles VS fetches");
static_assert(offsetof(Flux_ParticleInstance, m_xPosition_Size) == Flux_Generated_Particles::Particles::kaxVertexAttribs[2].m_uOffset,
	"Flux_ParticleInstance.m_xPosition_Size drifted from the generated per-instance layout");
static_assert(offsetof(Flux_ParticleInstance, m_xColor) == Flux_Generated_Particles::Particles::kaxVertexAttribs[3].m_uOffset,
	"Flux_ParticleInstance.m_xColor drifted from the generated per-instance layout");

// Full-table pin: the two instance lanes are both float4 at 0/16, so a shader-side
// swap preserves every offset above — only a semantic/type comparison can see it.
inline constexpr Flux_VertexLayoutElement kaxFLUX_PARTICLE_EXPECTED_LAYOUT[] =
{
	{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3, 0u,  0u },
	{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2, 0u, 12u },
	{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 1u, SHADER_DATA_TYPE_FLOAT4, 1u,  0u },
	{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 2u, SHADER_DATA_TYPE_FLOAT4, 1u, 16u },
};
static_assert(Flux_Generated_Particles::Particles::kVertexLayout == Flux_VertexLayoutDesc{ kaxFLUX_PARTICLE_EXPECTED_LAYOUT, 4u, { 20u, 32u } },
	"The Particles program's vertex layout drifted from the pinned contract — re-derive the expected table consciously if the VsIn really changed");
