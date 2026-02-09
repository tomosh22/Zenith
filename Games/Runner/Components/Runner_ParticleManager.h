#pragma once
/**
 * Runner_ParticleManager.h - Particle effects management
 *
 * Demonstrates:
 * - Flux_Particles system integration
 * - Dust trail effects while running
 * - Collection particle bursts
 *
 * The Zenith engine's particle system (Flux_Particles) uses:
 * - Instance buffer for particle data (position, size, color)
 * - Billboard rendering facing camera
 * - GPU-based particle rendering
 *
 * For this demo, we simulate particle effects visually since
 * the particle system requires texture assets.
 */

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Prefab/Zenith_Prefab.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Maths/Zenith_Maths.h"
#include <vector>

/**
 * Runner_ParticleManager - Manages visual particle effects
 *
 * In a full implementation with Flux_Particles:
 * 1. Define particle emitter with spawn rate, velocity, lifetime
 * 2. Upload particle instance data each frame
 * 3. Flux_Particles renders billboarded quads
 *
 * For this demo:
 * - Uses small sphere entities as "particles"
 * - Simulates dust trail behind character
 * - Simulates collection burst effects
 */
class Runner_ParticleManager
{
public:
	// ========================================================================
	// Configuration
	// ========================================================================
	struct Config
	{
		float m_fDustSpawnRate = 20.0f;
		float m_fDustParticleLifetime = 0.5f;
		float m_fCollectParticleCount = 8.0f;
	};

	// ========================================================================
	// Particle data
	// ========================================================================
	struct Particle
	{
		Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xPosition;
		Zenith_Maths::Vector3 m_xVelocity;
		float m_fLifetime = 0.0f;
		float m_fMaxLifetime = 0.5f;
		float m_fSize = 0.1f;
	};

	// ========================================================================
	// Initialization
	// ========================================================================
	static void Initialize(
		const Config& xConfig,
		Zenith_Prefab* pxParticlePrefab,
		Flux_MeshGeometry* pxSphereGeometry,
		Zenith_MaterialAsset* pxDustMaterial,
		Zenith_MaterialAsset* pxCollectMaterial)
	{
		s_xConfig = xConfig;
		s_pxParticlePrefab = pxParticlePrefab;
		s_pxSphereGeometry = pxSphereGeometry;
		s_pxDustMaterial = pxDustMaterial;
		s_pxCollectMaterial = pxCollectMaterial;

		Reset();
	}

	static void Reset()
	{
		// Destroy existing particles
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		for (auto& xParticle : s_axParticles)
		{
			if (xParticle.m_uEntityID.IsValid() && pxSceneData->EntityExists(xParticle.m_uEntityID))
			{
				Zenith_Entity xEntity = pxSceneData->GetEntity(xParticle.m_uEntityID);
				Zenith_SceneManager::Destroy(xEntity);
			}
		}
		s_axParticles.clear();
		s_fDustSpawnAccumulator = 0.0f;
	}

	// ========================================================================
	// Update
	// ========================================================================
	static void Update(float fDt, const Zenith_Maths::Vector3& xPlayerPos, bool bIsRunning, bool bIsGrounded)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		// Spawn dust particles while running on ground
		if (bIsRunning && bIsGrounded)
		{
			s_fDustSpawnAccumulator += fDt * s_xConfig.m_fDustSpawnRate;
			while (s_fDustSpawnAccumulator >= 1.0f)
			{
				SpawnDustParticle(xPlayerPos);
				s_fDustSpawnAccumulator -= 1.0f;
			}
		}

		// Update all particles
		for (auto it = s_axParticles.begin(); it != s_axParticles.end();)
		{
			it->m_fLifetime += fDt;

			if (it->m_fLifetime >= it->m_fMaxLifetime)
			{
				// Destroy expired particle
				if (it->m_uEntityID.IsValid() && pxSceneData->EntityExists(it->m_uEntityID))
				{
					Zenith_Entity xEntity = pxSceneData->GetEntity(it->m_uEntityID);
					Zenith_SceneManager::Destroy(xEntity);
				}
				it = s_axParticles.erase(it);
			}
			else
			{
				// Update particle position and appearance
				UpdateParticle(*it, fDt);
				++it;
			}
		}
	}

	// ========================================================================
	// Effects
	// ========================================================================
	static void SpawnCollectEffect(const Zenith_Maths::Vector3& xPos)
	{
		if (s_pxParticlePrefab == nullptr || s_pxSphereGeometry == nullptr || s_pxCollectMaterial == nullptr)
		{
			return;
		}

		// Spawn burst of particles
		for (int i = 0; i < static_cast<int>(s_xConfig.m_fCollectParticleCount); i++)
		{
			SpawnCollectParticle(xPos, i);
		}
	}

	/*
	// ========================================================================
	// EXAMPLE: Real Flux_Particles usage
	// ========================================================================
	// This is how you would integrate with the actual particle system:

	static void InitializeRealParticles()
	{
		// Flux_Particles is initialized globally
		// Upload particle instance data each frame

		// Define particle structure matching shader:
		// struct Particle {
		//     Vector4 m_xPosition_Radius;  // xyz = position, w = radius
		//     Vector4 m_xColour;           // rgba color
		// };

		// Each frame:
		// 1. Build array of active particles
		// 2. Upload to s_xInstanceBuffer via Flux_MemoryManager
		// 3. Flux_Particles::Render() draws them as billboarded quads
	}

	static void UpdateRealParticles()
	{
		// Build particle data for GPU upload
		std::vector<ParticleGPU> axGPUParticles;

		for (const auto& xParticle : s_axParticles)
		{
			ParticleGPU xGPU;
			xGPU.m_xPosition_Radius = Zenith_Maths::Vector4(
				xParticle.m_xPosition, xParticle.m_fSize);

			// Fade out over lifetime
			float fAlpha = 1.0f - (xParticle.m_fLifetime / xParticle.m_fMaxLifetime);
			xGPU.m_xColour = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, fAlpha);

			axGPUParticles.push_back(xGPU);
		}

		// Upload to GPU
		Flux_MemoryManager::UploadBufferData(
			s_xInstanceBuffer.GetBuffer().m_xVRAMHandle,
			axGPUParticles.data(),
			axGPUParticles.size() * sizeof(ParticleGPU));
	}
	*/

private:
	// ========================================================================
	// Particle Spawning
	// ========================================================================
	static void SpawnDustParticle(const Zenith_Maths::Vector3& xPlayerPos)
	{
		if (s_pxParticlePrefab == nullptr || s_pxSphereGeometry == nullptr || s_pxDustMaterial == nullptr)
		{
			return;
		}

		Particle xParticle;

		// Random offset behind player
		float fRandX = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.5f;
		float fRandZ = static_cast<float>(rand()) / RAND_MAX * 0.3f;

		xParticle.m_xPosition = xPlayerPos + Zenith_Maths::Vector3(fRandX, 0.1f, -0.5f - fRandZ);

		// Random upward velocity
		xParticle.m_xVelocity = Zenith_Maths::Vector3(
			(static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.5f,
			static_cast<float>(rand()) / RAND_MAX * 1.0f + 0.5f,
			-0.5f
		);

		xParticle.m_fLifetime = 0.0f;
		xParticle.m_fMaxLifetime = s_xConfig.m_fDustParticleLifetime;
		xParticle.m_fSize = 0.1f + static_cast<float>(rand()) / RAND_MAX * 0.1f;

		// Create entity
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		Zenith_Entity xEntity = s_pxParticlePrefab->Instantiate(pxSceneData, "DustParticle");
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xParticle.m_xPosition);
		xTransform.SetScale(Zenith_Maths::Vector3(xParticle.m_fSize));

		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*s_pxSphereGeometry, *s_pxDustMaterial);

		xParticle.m_uEntityID = xEntity.GetEntityID();
		s_axParticles.push_back(xParticle);
	}

	static void SpawnCollectParticle(const Zenith_Maths::Vector3& xPos, int iIndex)
	{
		Particle xParticle;

		// Radial burst pattern
		float fAngle = static_cast<float>(iIndex) / s_xConfig.m_fCollectParticleCount * 6.28318f;
		float fSpeed = 3.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f;

		xParticle.m_xPosition = xPos;
		xParticle.m_xVelocity = Zenith_Maths::Vector3(
			cos(fAngle) * fSpeed,
			1.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f,
			sin(fAngle) * fSpeed
		);

		xParticle.m_fLifetime = 0.0f;
		xParticle.m_fMaxLifetime = 0.3f + static_cast<float>(rand()) / RAND_MAX * 0.2f;
		xParticle.m_fSize = 0.15f;

		// Create entity
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		Zenith_Entity xEntity = s_pxParticlePrefab->Instantiate(pxSceneData, "CollectParticle");
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xParticle.m_xPosition);
		xTransform.SetScale(Zenith_Maths::Vector3(xParticle.m_fSize));

		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*s_pxSphereGeometry, *s_pxCollectMaterial);

		xParticle.m_uEntityID = xEntity.GetEntityID();
		s_axParticles.push_back(xParticle);
	}

	// ========================================================================
	// Particle Update
	// ========================================================================
	static void UpdateParticle(Particle& xParticle, float fDt)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		if (!xParticle.m_uEntityID.IsValid() || !pxSceneData->EntityExists(xParticle.m_uEntityID))
		{
			return;
		}

		// Apply velocity
		xParticle.m_xPosition += xParticle.m_xVelocity * fDt;

		// Apply gravity
		xParticle.m_xVelocity.y -= 5.0f * fDt;

		// Fade out (shrink)
		float fLifeRatio = xParticle.m_fLifetime / xParticle.m_fMaxLifetime;
		float fScale = xParticle.m_fSize * (1.0f - fLifeRatio * 0.5f);

		// Update entity
		Zenith_Entity xEntity = pxSceneData->GetEntity(xParticle.m_uEntityID);
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xParticle.m_xPosition);
		xTransform.SetScale(Zenith_Maths::Vector3(fScale));
	}

	// ========================================================================
	// Static State
	// ========================================================================
	static inline Config s_xConfig;
	static inline std::vector<Particle> s_axParticles;
	static inline float s_fDustSpawnAccumulator = 0.0f;

	static inline Zenith_Prefab* s_pxParticlePrefab = nullptr;
	static inline Flux_MeshGeometry* s_pxSphereGeometry = nullptr;
	static inline Zenith_MaterialAsset* s_pxDustMaterial = nullptr;
	static inline Zenith_MaterialAsset* s_pxCollectMaterial = nullptr;
};
