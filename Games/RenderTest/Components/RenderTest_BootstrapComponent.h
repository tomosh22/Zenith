#pragma once
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "RenderTest/RenderTest_Jetpack.h"
#include "RenderTest/RenderTest_Guns.h"
#include "RenderTest/RenderTest_Tennis.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// Outcome of one TryApplyGrassDensityFromDisk attempt. Defined here (the consumer);
// the body lives in RenderTest.cpp where the terrain / grass engine systems are
// reachable.
enum class RenderTest_GrassApplyResult
{
	Applied,          // density map loaded + applied to the (ready) terrain
	FileMissing,      // the density .ztxtr is absent or failed to parse — give up
	TerrainNotReady,  // terrain physics geometry not streamed in yet — retry later
	SkippedHeadless,  // headless run: Grass owns GPU resources, nothing to do
};
RenderTest_GrassApplyResult RenderTest_TryApplyGrassDensityFromDisk();

// Per-launch bootstrap for the RenderTest scene. The testbeds are now baked into
// the saved scene as authored entities/assets, so the per-launch *state* the old
// runtime spawns used to set (CLI tuning/showcase flags, the jetpack mount
// calibration override, the post-terrain grass apply) has no spawn to live in.
// This single non-transient component — authored FIRST so its OnAwake precedes
// every other entity's OnStart — owns that state.
//
// It carries no serialized payload beyond a version tag; ReadFromDataStream resets
// the runtime grass-retry state so a scene reload / Play->Stop->Play re-runs the
// apply.
class RenderTest_BootstrapComponent
{
public:
	RenderTest_BootstrapComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	}

	// OnAwake runs for ALL entities before any OnStart, so the CLI/tuning state set
	// here is visible to every consumer's OnStart (e.g. the tennis match). Also
	// resets the runtime grass state for Play->Stop->Play (static-state discipline).
	void OnAwake()
	{
		RenderTest_ParseJetpackCLI();
		RenderTest_ParseGunCLI();
		RenderTest_ParseTennisCLI();

		m_bGrassDone = false;
		m_uGrassRetryFrames = 0;
		m_bGrassWarned = false;
	}

	// Apply the jetpack mount calibration override, if any --jetpack-mount-* knob was
	// passed. The DEFAULT mount is already baked into the scene by AddStep_AttachToBone;
	// this only re-applies when the user is calibrating from screenshots.
	void OnStart()
	{
		using namespace RenderTest_JetpackTuning;
		const bool bMountOverridden =
			IsSet(s_fMountX) || IsSet(s_fMountY) || IsSet(s_fMountZ) ||
			IsSet(s_fMountPitch) || IsSet(s_fMountYaw) || IsSet(s_fMountRoll);
		if (!bMountOverridden)
		{
			return;
		}

		Zenith_SceneData* pxSceneData = m_xParentEntity.GetSceneData();
		if (!pxSceneData)
		{
			return;
		}
		Zenith_Entity xJetpack = pxSceneData->FindEntityByName("Jetpack");
		if (Zenith_AttachmentComponent* pxAttachment = xJetpack.TryGetComponent<Zenith_AttachmentComponent>())
		{
			pxAttachment->SetOffset(RenderTest_BuildJetpackMount());
		}
	}

	// Drive the grass density apply until it succeeds, the file is missing, or the
	// terrain never becomes ready within the retry budget. Idempotent: once done it
	// no-ops. Covers the non-tools + runtime/Playing paths (the tools Stopped-view
	// apply is a separate post-load AddStep_Custom calling the same shared helper).
	void OnUpdate(float)
	{
		if (m_bGrassDone)
		{
			return;
		}

		const RenderTest_GrassApplyResult eResult = RenderTest_TryApplyGrassDensityFromDisk();
		switch (eResult)
		{
		case RenderTest_GrassApplyResult::Applied:
		case RenderTest_GrassApplyResult::SkippedHeadless:
			m_bGrassDone = true;
			break;

		case RenderTest_GrassApplyResult::FileMissing:
			WarnGrassOnce("[RenderTest] grass density map missing/invalid — grass not applied");
			m_bGrassDone = true;
			break;

		case RenderTest_GrassApplyResult::TerrainNotReady:
			if (++m_uGrassRetryFrames >= uGRASS_RETRY_FRAME_CAP)
			{
				WarnGrassOnce("[RenderTest] terrain never became ready within the retry budget — grass not applied");
				m_bGrassDone = true;
			}
			break;
		}
	}

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
		// Re-arm the grass apply for this (re)load.
		m_bGrassDone = false;
		m_uGrassRetryFrames = 0;
		m_bGrassWarned = false;
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("RenderTest Bootstrap", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Grass applied: %s", m_bGrassDone ? "true" : "false");
			ImGui::Text("Grass retry frames: %u", m_uGrassRetryFrames);
		}
	}
#endif

private:
	void WarnGrassOnce(const char* szMsg)
	{
		if (!m_bGrassWarned)
		{
			Zenith_Warning(LOG_CATEGORY_TERRAIN, "%s", szMsg);
			m_bGrassWarned = true;
		}
	}

	// ~300 frames (~5s at 60fps) of waiting for terrain streaming before giving up.
	static constexpr uint32_t uGRASS_RETRY_FRAME_CAP = 300;

	Zenith_Entity m_xParentEntity;
	bool     m_bGrassDone        = false;
	bool     m_bGrassWarned      = false;
	uint32_t m_uGrassRetryFrames = 0;
};
