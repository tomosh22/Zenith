#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

/**
 * Runner_Config - Serializable asset for Runner game configuration
 *
 * Stores all tunable gameplay parameters for the endless runner:
 * - Character movement and physics
 * - Terrain generation
 * - Obstacle and collectible spawning
 * - Animation parameters
 * - Scoring
 */
class Runner_Config : public Zenith_Asset
{
public:
	ZENITH_ASSET_TYPE_NAME(Runner_Config)

	// ========================================================================
	// Character Movement
	// ========================================================================
	float m_fForwardSpeed = 15.0f;           // Base forward speed
	float m_fMaxForwardSpeed = 35.0f;        // Maximum speed after acceleration
	float m_fSpeedIncreaseRate = 0.5f;       // Speed increase per second
	float m_fLateralMoveSpeed = 8.0f;        // Side-to-side movement speed
	float m_fJumpForce = 12.0f;              // Jump impulse
	float m_fGravity = 30.0f;                // Gravity acceleration
	float m_fSlideSpeed = 1.2f;              // Speed multiplier while sliding
	float m_fSlideDuration = 0.8f;           // How long slide lasts

	// ========================================================================
	// Character Dimensions
	// ========================================================================
	float m_fCharacterHeight = 1.8f;         // Character capsule height
	float m_fCharacterRadius = 0.4f;         // Character capsule radius
	float m_fSlideHeight = 0.6f;             // Height when sliding

	// ========================================================================
	// Terrain Generation
	// ========================================================================
	float m_fTerrainChunkLength = 100.0f;    // Length of each terrain chunk
	float m_fTerrainWidth = 20.0f;           // Width of terrain
	uint32_t m_uActiveChunkCount = 5;        // Number of chunks ahead to keep loaded
	float m_fTerrainHeightVariation = 2.0f;  // Max height variation

	// ========================================================================
	// Lane System
	// ========================================================================
	uint32_t m_uLaneCount = 3;               // Number of lanes
	float m_fLaneWidth = 3.0f;               // Width of each lane
	float m_fLaneSwitchTime = 0.2f;          // Time to switch lanes

	// ========================================================================
	// Obstacles
	// ========================================================================
	float m_fObstacleSpawnDistance = 50.0f;  // Distance ahead to spawn obstacles
	float m_fMinObstacleGap = 15.0f;         // Minimum gap between obstacles
	float m_fMaxObstacleGap = 30.0f;         // Maximum gap between obstacles
	float m_fObstacleHeight = 1.5f;          // Height of jump obstacles
	float m_fSlideObstacleHeight = 2.5f;     // Height of slide obstacles

	// ========================================================================
	// Collectibles
	// ========================================================================
	float m_fCollectibleSpawnDistance = 80.0f;  // Distance ahead to spawn collectibles
	float m_fCollectibleRadius = 0.5f;          // Collectible pickup radius
	float m_fCollectibleBobSpeed = 3.0f;        // Bobbing animation speed
	float m_fCollectibleBobHeight = 0.3f;       // Bobbing animation height
	float m_fCollectibleRotateSpeed = 2.0f;     // Rotation speed
	uint32_t m_uPointsPerCollectible = 10;      // Points per pickup

	// ========================================================================
	// Animation
	// ========================================================================
	float m_fRunAnimSpeedMultiplier = 1.0f;  // Animation speed multiplier for run
	float m_fBlendSpaceMinSpeed = 0.0f;      // BlendSpace1D min speed
	float m_fBlendSpaceMaxSpeed = 35.0f;     // BlendSpace1D max speed

	// ========================================================================
	// Camera
	// ========================================================================
	float m_fCameraDistance = 8.0f;          // Distance behind player
	float m_fCameraHeight = 4.0f;            // Height above player
	float m_fCameraLookAhead = 5.0f;         // How far ahead camera looks
	float m_fCameraSmoothSpeed = 5.0f;       // Camera follow smoothing

	// ========================================================================
	// Particles
	// ========================================================================
	float m_fDustSpawnRate = 20.0f;          // Dust particles per second while running
	float m_fDustParticleLifetime = 0.5f;    // Dust particle lifetime
	float m_fCollectParticleCount = 8.0f;    // Particles when collecting item

	// ========================================================================
	// Serialization
	// ========================================================================
	void WriteToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;

		// Character movement
		xStream << m_fForwardSpeed;
		xStream << m_fMaxForwardSpeed;
		xStream << m_fSpeedIncreaseRate;
		xStream << m_fLateralMoveSpeed;
		xStream << m_fJumpForce;
		xStream << m_fGravity;
		xStream << m_fSlideSpeed;
		xStream << m_fSlideDuration;

		// Character dimensions
		xStream << m_fCharacterHeight;
		xStream << m_fCharacterRadius;
		xStream << m_fSlideHeight;

		// Terrain
		xStream << m_fTerrainChunkLength;
		xStream << m_fTerrainWidth;
		xStream << m_uActiveChunkCount;
		xStream << m_fTerrainHeightVariation;

		// Lanes
		xStream << m_uLaneCount;
		xStream << m_fLaneWidth;
		xStream << m_fLaneSwitchTime;

		// Obstacles
		xStream << m_fObstacleSpawnDistance;
		xStream << m_fMinObstacleGap;
		xStream << m_fMaxObstacleGap;
		xStream << m_fObstacleHeight;
		xStream << m_fSlideObstacleHeight;

		// Collectibles
		xStream << m_fCollectibleSpawnDistance;
		xStream << m_fCollectibleRadius;
		xStream << m_fCollectibleBobSpeed;
		xStream << m_fCollectibleBobHeight;
		xStream << m_fCollectibleRotateSpeed;
		xStream << m_uPointsPerCollectible;

		// Animation
		xStream << m_fRunAnimSpeedMultiplier;
		xStream << m_fBlendSpaceMinSpeed;
		xStream << m_fBlendSpaceMaxSpeed;

		// Camera
		xStream << m_fCameraDistance;
		xStream << m_fCameraHeight;
		xStream << m_fCameraLookAhead;
		xStream << m_fCameraSmoothSpeed;

		// Particles
		xStream << m_fDustSpawnRate;
		xStream << m_fDustParticleLifetime;
		xStream << m_fCollectParticleCount;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;

		if (uVersion >= 1)
		{
			// Character movement
			xStream >> m_fForwardSpeed;
			xStream >> m_fMaxForwardSpeed;
			xStream >> m_fSpeedIncreaseRate;
			xStream >> m_fLateralMoveSpeed;
			xStream >> m_fJumpForce;
			xStream >> m_fGravity;
			xStream >> m_fSlideSpeed;
			xStream >> m_fSlideDuration;

			// Character dimensions
			xStream >> m_fCharacterHeight;
			xStream >> m_fCharacterRadius;
			xStream >> m_fSlideHeight;

			// Terrain
			xStream >> m_fTerrainChunkLength;
			xStream >> m_fTerrainWidth;
			xStream >> m_uActiveChunkCount;
			xStream >> m_fTerrainHeightVariation;

			// Lanes
			xStream >> m_uLaneCount;
			xStream >> m_fLaneWidth;
			xStream >> m_fLaneSwitchTime;

			// Obstacles
			xStream >> m_fObstacleSpawnDistance;
			xStream >> m_fMinObstacleGap;
			xStream >> m_fMaxObstacleGap;
			xStream >> m_fObstacleHeight;
			xStream >> m_fSlideObstacleHeight;

			// Collectibles
			xStream >> m_fCollectibleSpawnDistance;
			xStream >> m_fCollectibleRadius;
			xStream >> m_fCollectibleBobSpeed;
			xStream >> m_fCollectibleBobHeight;
			xStream >> m_fCollectibleRotateSpeed;
			xStream >> m_uPointsPerCollectible;

			// Animation
			xStream >> m_fRunAnimSpeedMultiplier;
			xStream >> m_fBlendSpaceMinSpeed;
			xStream >> m_fBlendSpaceMaxSpeed;

			// Camera
			xStream >> m_fCameraDistance;
			xStream >> m_fCameraHeight;
			xStream >> m_fCameraLookAhead;
			xStream >> m_fCameraSmoothSpeed;

			// Particles
			xStream >> m_fDustSpawnRate;
			xStream >> m_fDustParticleLifetime;
			xStream >> m_fCollectParticleCount;
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override
	{
		ImGui::Text("Runner Game Configuration");
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Character Movement", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Forward Speed", &m_fForwardSpeed, 0.5f, 5.0f, 50.0f);
			ImGui::DragFloat("Max Forward Speed", &m_fMaxForwardSpeed, 0.5f, 10.0f, 100.0f);
			ImGui::DragFloat("Speed Increase Rate", &m_fSpeedIncreaseRate, 0.1f, 0.0f, 5.0f);
			ImGui::DragFloat("Lateral Speed", &m_fLateralMoveSpeed, 0.5f, 1.0f, 20.0f);
			ImGui::DragFloat("Jump Force", &m_fJumpForce, 0.5f, 5.0f, 25.0f);
			ImGui::DragFloat("Gravity", &m_fGravity, 1.0f, 10.0f, 60.0f);
			ImGui::DragFloat("Slide Speed Mult", &m_fSlideSpeed, 0.1f, 0.5f, 2.0f);
			ImGui::DragFloat("Slide Duration", &m_fSlideDuration, 0.1f, 0.2f, 2.0f);
		}

		if (ImGui::CollapsingHeader("Character Dimensions"))
		{
			ImGui::DragFloat("Height", &m_fCharacterHeight, 0.1f, 1.0f, 3.0f);
			ImGui::DragFloat("Radius", &m_fCharacterRadius, 0.1f, 0.2f, 1.0f);
			ImGui::DragFloat("Slide Height", &m_fSlideHeight, 0.1f, 0.3f, 1.5f);
		}

		if (ImGui::CollapsingHeader("Terrain"))
		{
			ImGui::DragFloat("Chunk Length", &m_fTerrainChunkLength, 10.0f, 50.0f, 500.0f);
			ImGui::DragFloat("Width", &m_fTerrainWidth, 1.0f, 10.0f, 50.0f);
			ImGui::DragScalar("Active Chunks", ImGuiDataType_U32, &m_uActiveChunkCount, 1.0f);
			ImGui::DragFloat("Height Variation", &m_fTerrainHeightVariation, 0.5f, 0.0f, 10.0f);
		}

		if (ImGui::CollapsingHeader("Lanes"))
		{
			ImGui::DragScalar("Lane Count", ImGuiDataType_U32, &m_uLaneCount, 1.0f);
			ImGui::DragFloat("Lane Width", &m_fLaneWidth, 0.5f, 1.0f, 10.0f);
			ImGui::DragFloat("Lane Switch Time", &m_fLaneSwitchTime, 0.05f, 0.05f, 1.0f);
		}

		if (ImGui::CollapsingHeader("Obstacles"))
		{
			ImGui::DragFloat("Spawn Distance", &m_fObstacleSpawnDistance, 5.0f, 20.0f, 200.0f);
			ImGui::DragFloat("Min Gap", &m_fMinObstacleGap, 1.0f, 5.0f, 50.0f);
			ImGui::DragFloat("Max Gap", &m_fMaxObstacleGap, 1.0f, 10.0f, 100.0f);
			ImGui::DragFloat("Obstacle Height", &m_fObstacleHeight, 0.1f, 0.5f, 3.0f);
			ImGui::DragFloat("Slide Obstacle Height", &m_fSlideObstacleHeight, 0.1f, 1.5f, 5.0f);
		}

		if (ImGui::CollapsingHeader("Collectibles"))
		{
			ImGui::DragFloat("Spawn Distance", &m_fCollectibleSpawnDistance, 5.0f, 30.0f, 200.0f);
			ImGui::DragFloat("Pickup Radius", &m_fCollectibleRadius, 0.1f, 0.2f, 2.0f);
			ImGui::DragFloat("Bob Speed", &m_fCollectibleBobSpeed, 0.5f, 0.5f, 10.0f);
			ImGui::DragFloat("Bob Height", &m_fCollectibleBobHeight, 0.1f, 0.1f, 1.0f);
			ImGui::DragFloat("Rotate Speed", &m_fCollectibleRotateSpeed, 0.5f, 0.5f, 10.0f);
			ImGui::DragScalar("Points Per Item", ImGuiDataType_U32, &m_uPointsPerCollectible, 5.0f);
		}

		if (ImGui::CollapsingHeader("Animation"))
		{
			ImGui::DragFloat("Run Anim Speed Mult", &m_fRunAnimSpeedMultiplier, 0.1f, 0.1f, 3.0f);
			ImGui::DragFloat("BlendSpace Min Speed", &m_fBlendSpaceMinSpeed, 1.0f, 0.0f, 20.0f);
			ImGui::DragFloat("BlendSpace Max Speed", &m_fBlendSpaceMaxSpeed, 1.0f, 10.0f, 100.0f);
		}

		if (ImGui::CollapsingHeader("Camera"))
		{
			ImGui::DragFloat("Distance", &m_fCameraDistance, 0.5f, 3.0f, 20.0f);
			ImGui::DragFloat("Height", &m_fCameraHeight, 0.5f, 1.0f, 15.0f);
			ImGui::DragFloat("Look Ahead", &m_fCameraLookAhead, 0.5f, 0.0f, 20.0f);
			ImGui::DragFloat("Smooth Speed", &m_fCameraSmoothSpeed, 0.5f, 1.0f, 20.0f);
		}

		if (ImGui::CollapsingHeader("Particles"))
		{
			ImGui::DragFloat("Dust Spawn Rate", &m_fDustSpawnRate, 1.0f, 1.0f, 100.0f);
			ImGui::DragFloat("Dust Lifetime", &m_fDustParticleLifetime, 0.1f, 0.1f, 2.0f);
			ImGui::DragFloat("Collect Particle Count", &m_fCollectParticleCount, 1.0f, 1.0f, 50.0f);
		}
	}
#endif
};

// Register the asset type (automatically called via static initialization)
ZENITH_REGISTER_ASSET_TYPE(Runner_Config)
