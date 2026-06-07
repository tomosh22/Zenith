#pragma once

#include "Maths/Zenith_Maths.h"

/**
 * Zenith_ParticleData - engine-side (EntityComponent) CPU particle record.
 *
 * This is a renderer-neutral MIRROR of Flux/Particles/Flux_ParticleData.h's
 * Flux_Particle (same 6-Vector4 layout + accessors). It exists so
 * Zenith_ParticleEmitterComponent can store + return particles by value WITHOUT
 * its header naming a Flux type — keeping every EntityComponent header Flux-free
 * (Wave-2 EC->Flux header-leak sever). The renderer (Flux_Particles.cpp) reads
 * these via GetParticles() and builds Flux_ParticleInstance from the accessors
 * below, so the two structs never need to be layout-compatible — but they are
 * kept field-identical so the mirror stays an obvious 1:1 of Flux_Particle.
 *
 * If Flux_Particle changes, update this mirror to match.
 */
struct Zenith_ParticleData
{
	Zenith_Maths::Vector4 m_xPosition_Age;      // xyz=position, w=age
	Zenith_Maths::Vector4 m_xVelocity_Lifetime; // xyz=velocity, w=lifetime
	Zenith_Maths::Vector4 m_xColorStart;        // rgba
	Zenith_Maths::Vector4 m_xColorEnd;          // rgba
	Zenith_Maths::Vector4 m_xSizeRotation;      // x=sizeStart, y=sizeEnd, z=rotation, w=rotationSpeed
	Zenith_Maths::Vector4 m_xPadding;           // reserved

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
