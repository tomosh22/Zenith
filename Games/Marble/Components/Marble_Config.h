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
 * Marble_Config - DataAsset for Marble game configuration
 *
 * Demonstrates DataAsset system.
 * Game designers can create .zdata files with different configurations.
 */
class Marble_Config : public Zenith_DataAsset
{
public:
	ZENITH_DATA_ASSET_TYPE_NAME(Marble_Config)

	// Ball physics
	float m_fBallRadius = 0.5f;
	float m_fBallMass = 1.0f;
	float m_fBallFriction = 0.3f;
	float m_fMoveForce = 15.0f;
	float m_fJumpForce = 8.0f;
	float m_fMaxSpeed = 15.0f;

	// Camera
	float m_fCameraDistance = 8.0f;
	float m_fCameraHeight = 5.0f;
	float m_fCameraSmoothSpeed = 5.0f;

	// Level generation
	uint32_t m_uPlatformCount = 8;
	float m_fPlatformMinSize = 2.0f;
	float m_fPlatformMaxSize = 6.0f;
	float m_fPlatformSpacing = 3.0f;
	float m_fPlatformHeightVariation = 2.0f;

	// Collectibles
	uint32_t m_uCollectibleCount = 5;
	float m_fCollectibleRadius = 0.3f;
	float m_fCollectibleRotateSpeed = 2.0f;

	// Game settings
	float m_fTimeLimit = 60.0f;
	uint32_t m_uPointsPerCollectible = 100;

	void WriteToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;

		// Ball physics
		xStream << m_fBallRadius;
		xStream << m_fBallMass;
		xStream << m_fBallFriction;
		xStream << m_fMoveForce;
		xStream << m_fJumpForce;
		xStream << m_fMaxSpeed;

		// Camera
		xStream << m_fCameraDistance;
		xStream << m_fCameraHeight;
		xStream << m_fCameraSmoothSpeed;

		// Level generation
		xStream << m_uPlatformCount;
		xStream << m_fPlatformMinSize;
		xStream << m_fPlatformMaxSize;
		xStream << m_fPlatformSpacing;
		xStream << m_fPlatformHeightVariation;

		// Collectibles
		xStream << m_uCollectibleCount;
		xStream << m_fCollectibleRadius;
		xStream << m_fCollectibleRotateSpeed;

		// Game settings
		xStream << m_fTimeLimit;
		xStream << m_uPointsPerCollectible;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;

		if (uVersion >= 1)
		{
			// Ball physics
			xStream >> m_fBallRadius;
			xStream >> m_fBallMass;
			xStream >> m_fBallFriction;
			xStream >> m_fMoveForce;
			xStream >> m_fJumpForce;
			xStream >> m_fMaxSpeed;

			// Camera
			xStream >> m_fCameraDistance;
			xStream >> m_fCameraHeight;
			xStream >> m_fCameraSmoothSpeed;

			// Level generation
			xStream >> m_uPlatformCount;
			xStream >> m_fPlatformMinSize;
			xStream >> m_fPlatformMaxSize;
			xStream >> m_fPlatformSpacing;
			xStream >> m_fPlatformHeightVariation;

			// Collectibles
			xStream >> m_uCollectibleCount;
			xStream >> m_fCollectibleRadius;
			xStream >> m_fCollectibleRotateSpeed;

			// Game settings
			xStream >> m_fTimeLimit;
			xStream >> m_uPointsPerCollectible;
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override
	{
		ImGui::Text("Marble Game Configuration");
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Ball Physics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Ball Radius", &m_fBallRadius, 0.01f, 0.1f, 2.0f);
			ImGui::DragFloat("Ball Mass", &m_fBallMass, 0.1f, 0.1f, 10.0f);
			ImGui::DragFloat("Ball Friction", &m_fBallFriction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Move Force", &m_fMoveForce, 0.5f, 1.0f, 50.0f);
			ImGui::DragFloat("Jump Force", &m_fJumpForce, 0.5f, 1.0f, 20.0f);
			ImGui::DragFloat("Max Speed", &m_fMaxSpeed, 0.5f, 5.0f, 50.0f);
		}

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Camera Distance", &m_fCameraDistance, 0.1f, 2.0f, 20.0f);
			ImGui::DragFloat("Camera Height", &m_fCameraHeight, 0.1f, 1.0f, 15.0f);
			ImGui::DragFloat("Camera Smooth Speed", &m_fCameraSmoothSpeed, 0.1f, 0.5f, 20.0f);
		}

		if (ImGui::CollapsingHeader("Level Generation"))
		{
			ImGui::DragScalar("Platform Count", ImGuiDataType_U32, &m_uPlatformCount, 1.0f);
			ImGui::DragFloat("Platform Min Size", &m_fPlatformMinSize, 0.1f, 1.0f, 10.0f);
			ImGui::DragFloat("Platform Max Size", &m_fPlatformMaxSize, 0.1f, 2.0f, 15.0f);
			ImGui::DragFloat("Platform Spacing", &m_fPlatformSpacing, 0.1f, 1.0f, 10.0f);
			ImGui::DragFloat("Height Variation", &m_fPlatformHeightVariation, 0.1f, 0.0f, 5.0f);
		}

		if (ImGui::CollapsingHeader("Collectibles"))
		{
			ImGui::DragScalar("Collectible Count", ImGuiDataType_U32, &m_uCollectibleCount, 1.0f);
			ImGui::DragFloat("Collectible Radius", &m_fCollectibleRadius, 0.01f, 0.1f, 1.0f);
			ImGui::DragFloat("Rotate Speed", &m_fCollectibleRotateSpeed, 0.1f, 0.5f, 10.0f);
		}

		if (ImGui::CollapsingHeader("Game Settings"))
		{
			ImGui::DragFloat("Time Limit (sec)", &m_fTimeLimit, 1.0f, 10.0f, 300.0f);
			ImGui::DragScalar("Points Per Collectible", ImGuiDataType_U32, &m_uPointsPerCollectible, 10.0f);
		}
	}
#endif
};

inline void RegisterMarbleDataAssets()
{
	Zenith_DataAssetManager::RegisterDataAssetType<Marble_Config>();
}
