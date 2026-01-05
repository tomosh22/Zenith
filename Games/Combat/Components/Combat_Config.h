#pragma once

#include "AssetHandling/Zenith_DataAsset.h"
#include "AssetHandling/Zenith_DataAssetManager.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

/**
 * Combat_Config - DataAsset for combat game configuration
 *
 * Contains all tunable parameters for combat gameplay:
 * - Player stats (health, damage, speed)
 * - Enemy stats (health, damage, speed, AI parameters)
 * - Animation timing (attack windows, combo timing)
 * - IK settings (foot placement, look-at)
 * - Arena configuration
 */
class Combat_Config : public Zenith_DataAsset
{
public:
	ZENITH_DATA_ASSET_TYPE_NAME(Combat_Config)

	// ========================================================================
	// Player Settings
	// ========================================================================
	float m_fPlayerHealth = 100.0f;
	float m_fPlayerMoveSpeed = 5.0f;
	float m_fPlayerRotationSpeed = 10.0f;
	float m_fPlayerDodgeSpeed = 12.0f;
	float m_fPlayerDodgeDuration = 0.4f;
	float m_fPlayerDodgeCooldown = 0.5f;

	// Player attack damage
	float m_fLightAttackDamage = 10.0f;
	float m_fHeavyAttackDamage = 25.0f;
	float m_fComboMultiplier = 1.2f;  // Damage multiplier per combo hit

	// Attack timing
	float m_fLightAttackDuration = 0.3f;
	float m_fHeavyAttackDuration = 0.6f;
	float m_fComboWindowTime = 0.5f;  // Time window to chain combos
	float m_fAttackRecoveryTime = 0.2f;

	// Attack ranges
	float m_fLightAttackRange = 1.5f;
	float m_fHeavyAttackRange = 2.0f;

	// ========================================================================
	// Enemy Settings
	// ========================================================================
	float m_fEnemyHealth = 50.0f;
	float m_fEnemyMoveSpeed = 3.0f;
	float m_fEnemyAttackDamage = 15.0f;
	float m_fEnemyAttackRange = 1.5f;
	float m_fEnemyAttackCooldown = 1.5f;
	float m_fEnemyDetectionRange = 15.0f;
	float m_fEnemyChaseStopDistance = 1.2f;

	uint32_t m_uEnemyCount = 3;  // Number of enemies per round

	// ========================================================================
	// IK Settings
	// ========================================================================
	float m_fFootIKRaycastHeight = 1.0f;
	float m_fFootIKRaycastDistance = 1.5f;
	float m_fFootIKBlendSpeed = 10.0f;
	float m_fLookAtIKMaxAngle = 1.2f;  // ~70 degrees
	float m_fLookAtIKBlendSpeed = 5.0f;

	// ========================================================================
	// Animation Settings
	// ========================================================================
	float m_fAnimationBlendTime = 0.15f;
	float m_fIdleToWalkThreshold = 0.1f;

	// ========================================================================
	// Arena Settings
	// ========================================================================
	float m_fArenaRadius = 15.0f;
	float m_fArenaWallHeight = 3.0f;
	uint32_t m_uArenaWallSegments = 24;

	// ========================================================================
	// Camera Settings
	// ========================================================================
	float m_fCameraDistance = 10.0f;
	float m_fCameraHeight = 8.0f;
	float m_fCameraPitch = -0.6f;  // Looking down at arena
	float m_fCameraFollowSpeed = 5.0f;

	// ========================================================================
	// Serialization
	// ========================================================================
	void WriteToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;

		// Player settings
		xStream << m_fPlayerHealth;
		xStream << m_fPlayerMoveSpeed;
		xStream << m_fPlayerRotationSpeed;
		xStream << m_fPlayerDodgeSpeed;
		xStream << m_fPlayerDodgeDuration;
		xStream << m_fPlayerDodgeCooldown;

		// Attack damage
		xStream << m_fLightAttackDamage;
		xStream << m_fHeavyAttackDamage;
		xStream << m_fComboMultiplier;

		// Attack timing
		xStream << m_fLightAttackDuration;
		xStream << m_fHeavyAttackDuration;
		xStream << m_fComboWindowTime;
		xStream << m_fAttackRecoveryTime;

		// Attack ranges
		xStream << m_fLightAttackRange;
		xStream << m_fHeavyAttackRange;

		// Enemy settings
		xStream << m_fEnemyHealth;
		xStream << m_fEnemyMoveSpeed;
		xStream << m_fEnemyAttackDamage;
		xStream << m_fEnemyAttackRange;
		xStream << m_fEnemyAttackCooldown;
		xStream << m_fEnemyDetectionRange;
		xStream << m_fEnemyChaseStopDistance;
		xStream << m_uEnemyCount;

		// IK settings
		xStream << m_fFootIKRaycastHeight;
		xStream << m_fFootIKRaycastDistance;
		xStream << m_fFootIKBlendSpeed;
		xStream << m_fLookAtIKMaxAngle;
		xStream << m_fLookAtIKBlendSpeed;

		// Animation settings
		xStream << m_fAnimationBlendTime;
		xStream << m_fIdleToWalkThreshold;

		// Arena settings
		xStream << m_fArenaRadius;
		xStream << m_fArenaWallHeight;
		xStream << m_uArenaWallSegments;

		// Camera settings
		xStream << m_fCameraDistance;
		xStream << m_fCameraHeight;
		xStream << m_fCameraPitch;
		xStream << m_fCameraFollowSpeed;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;

		if (uVersion >= 1)
		{
			// Player settings
			xStream >> m_fPlayerHealth;
			xStream >> m_fPlayerMoveSpeed;
			xStream >> m_fPlayerRotationSpeed;
			xStream >> m_fPlayerDodgeSpeed;
			xStream >> m_fPlayerDodgeDuration;
			xStream >> m_fPlayerDodgeCooldown;

			// Attack damage
			xStream >> m_fLightAttackDamage;
			xStream >> m_fHeavyAttackDamage;
			xStream >> m_fComboMultiplier;

			// Attack timing
			xStream >> m_fLightAttackDuration;
			xStream >> m_fHeavyAttackDuration;
			xStream >> m_fComboWindowTime;
			xStream >> m_fAttackRecoveryTime;

			// Attack ranges
			xStream >> m_fLightAttackRange;
			xStream >> m_fHeavyAttackRange;

			// Enemy settings
			xStream >> m_fEnemyHealth;
			xStream >> m_fEnemyMoveSpeed;
			xStream >> m_fEnemyAttackDamage;
			xStream >> m_fEnemyAttackRange;
			xStream >> m_fEnemyAttackCooldown;
			xStream >> m_fEnemyDetectionRange;
			xStream >> m_fEnemyChaseStopDistance;
			xStream >> m_uEnemyCount;

			// IK settings
			xStream >> m_fFootIKRaycastHeight;
			xStream >> m_fFootIKRaycastDistance;
			xStream >> m_fFootIKBlendSpeed;
			xStream >> m_fLookAtIKMaxAngle;
			xStream >> m_fLookAtIKBlendSpeed;

			// Animation settings
			xStream >> m_fAnimationBlendTime;
			xStream >> m_fIdleToWalkThreshold;

			// Arena settings
			xStream >> m_fArenaRadius;
			xStream >> m_fArenaWallHeight;
			xStream >> m_uArenaWallSegments;

			// Camera settings
			xStream >> m_fCameraDistance;
			xStream >> m_fCameraHeight;
			xStream >> m_fCameraPitch;
			xStream >> m_fCameraFollowSpeed;
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override
	{
		ImGui::Text("Combat Game Configuration");
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Player Movement", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Health", &m_fPlayerHealth, 1.0f, 10.0f, 500.0f);
			ImGui::DragFloat("Move Speed", &m_fPlayerMoveSpeed, 0.1f, 1.0f, 20.0f);
			ImGui::DragFloat("Rotation Speed", &m_fPlayerRotationSpeed, 0.1f, 1.0f, 20.0f);
			ImGui::DragFloat("Dodge Speed", &m_fPlayerDodgeSpeed, 0.1f, 5.0f, 30.0f);
			ImGui::DragFloat("Dodge Duration", &m_fPlayerDodgeDuration, 0.01f, 0.1f, 1.0f);
			ImGui::DragFloat("Dodge Cooldown", &m_fPlayerDodgeCooldown, 0.01f, 0.1f, 2.0f);
		}

		if (ImGui::CollapsingHeader("Player Attacks", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Light Attack Damage", &m_fLightAttackDamage, 1.0f, 1.0f, 100.0f);
			ImGui::DragFloat("Heavy Attack Damage", &m_fHeavyAttackDamage, 1.0f, 1.0f, 200.0f);
			ImGui::DragFloat("Combo Multiplier", &m_fComboMultiplier, 0.05f, 1.0f, 3.0f);
			ImGui::Separator();
			ImGui::DragFloat("Light Attack Duration", &m_fLightAttackDuration, 0.01f, 0.1f, 1.0f);
			ImGui::DragFloat("Heavy Attack Duration", &m_fHeavyAttackDuration, 0.01f, 0.1f, 2.0f);
			ImGui::DragFloat("Combo Window", &m_fComboWindowTime, 0.01f, 0.1f, 1.0f);
			ImGui::DragFloat("Recovery Time", &m_fAttackRecoveryTime, 0.01f, 0.0f, 0.5f);
			ImGui::Separator();
			ImGui::DragFloat("Light Attack Range", &m_fLightAttackRange, 0.1f, 0.5f, 5.0f);
			ImGui::DragFloat("Heavy Attack Range", &m_fHeavyAttackRange, 0.1f, 0.5f, 5.0f);
		}

		if (ImGui::CollapsingHeader("Enemy Settings"))
		{
			ImGui::DragFloat("Enemy Health", &m_fEnemyHealth, 1.0f, 10.0f, 200.0f);
			ImGui::DragFloat("Enemy Move Speed", &m_fEnemyMoveSpeed, 0.1f, 1.0f, 10.0f);
			ImGui::DragFloat("Enemy Attack Damage", &m_fEnemyAttackDamage, 1.0f, 1.0f, 50.0f);
			ImGui::DragFloat("Enemy Attack Range", &m_fEnemyAttackRange, 0.1f, 0.5f, 5.0f);
			ImGui::DragFloat("Enemy Attack Cooldown", &m_fEnemyAttackCooldown, 0.1f, 0.5f, 5.0f);
			ImGui::DragFloat("Detection Range", &m_fEnemyDetectionRange, 0.5f, 5.0f, 50.0f);
			ImGui::DragFloat("Chase Stop Distance", &m_fEnemyChaseStopDistance, 0.1f, 0.5f, 5.0f);
			ImGui::DragScalar("Enemy Count", ImGuiDataType_U32, &m_uEnemyCount, 1.0f);
		}

		if (ImGui::CollapsingHeader("Inverse Kinematics"))
		{
			ImGui::DragFloat("Foot IK Ray Height", &m_fFootIKRaycastHeight, 0.1f, 0.1f, 3.0f);
			ImGui::DragFloat("Foot IK Ray Distance", &m_fFootIKRaycastDistance, 0.1f, 0.5f, 3.0f);
			ImGui::DragFloat("Foot IK Blend Speed", &m_fFootIKBlendSpeed, 0.5f, 1.0f, 30.0f);
			ImGui::DragFloat("Look-At Max Angle", &m_fLookAtIKMaxAngle, 0.05f, 0.1f, 1.57f);
			ImGui::DragFloat("Look-At Blend Speed", &m_fLookAtIKBlendSpeed, 0.5f, 1.0f, 20.0f);
		}

		if (ImGui::CollapsingHeader("Animation"))
		{
			ImGui::DragFloat("Blend Time", &m_fAnimationBlendTime, 0.01f, 0.01f, 0.5f);
			ImGui::DragFloat("Idle/Walk Threshold", &m_fIdleToWalkThreshold, 0.01f, 0.01f, 0.5f);
		}

		if (ImGui::CollapsingHeader("Arena"))
		{
			ImGui::DragFloat("Arena Radius", &m_fArenaRadius, 0.5f, 5.0f, 50.0f);
			ImGui::DragFloat("Wall Height", &m_fArenaWallHeight, 0.1f, 1.0f, 10.0f);
			ImGui::DragScalar("Wall Segments", ImGuiDataType_U32, &m_uArenaWallSegments, 1.0f);
		}

		if (ImGui::CollapsingHeader("Camera"))
		{
			ImGui::DragFloat("Camera Distance", &m_fCameraDistance, 0.5f, 5.0f, 30.0f);
			ImGui::DragFloat("Camera Height", &m_fCameraHeight, 0.5f, 2.0f, 20.0f);
			ImGui::DragFloat("Camera Pitch", &m_fCameraPitch, 0.05f, -1.5f, -0.1f);
			ImGui::DragFloat("Follow Speed", &m_fCameraFollowSpeed, 0.5f, 1.0f, 20.0f);
		}
	}
#endif
};

// Register the DataAsset type (call once at startup)
inline void RegisterCombatDataAssets()
{
	Zenith_DataAssetManager::RegisterDataAssetType<Combat_Config>();
}
