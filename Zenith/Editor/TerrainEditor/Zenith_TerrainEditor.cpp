#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditorUndo.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/FrameContext.h"
#include "Core/Zenith_CommandLine.h"
#include "DataStream/Zenith_DataStream.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_Gizmo.h"
#include "Editor/Zenith_UndoSystem.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Decals/Flux_DecalsImpl.h"
#include "Flux/Flux_BackendTypes.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include "Input/Zenith_Input.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include <filesystem>

// Tools heightmap loader (Tools/Zenith_Tools_TerrainExport.cpp).
extern Zenith_Image Zenith_Tools_LoadHeightmapAuto(const std::string& strPath);

// The header deliberately includes no Flux header — pin its mirrored terrain
// constants against the real config here instead.
static_assert(Zenith_TerrainEditor::uCHUNK_GRID == Flux_TerrainConfig::CHUNK_GRID_SIZE, "Terrain editor chunk grid out of sync with Flux_TerrainConfig");
static_assert(Zenith_TerrainEditor::uCHUNK_COUNT == Flux_TerrainConfig::TOTAL_CHUNKS, "Terrain editor chunk count out of sync with Flux_TerrainConfig");
static_assert(Zenith_TerrainEditor::fTERRAIN_WORLD_SIZE == Flux_TerrainConfig::TERRAIN_SIZE, "Terrain editor world size out of sync with Flux_TerrainConfig");
static_assert(Zenith_TerrainEditor::fTERRAIN_MAX_HEIGHT == Flux_TerrainConfig::MAX_TERRAIN_HEIGHT, "Terrain editor max height out of sync with Flux_TerrainConfig");

//-----------------------------------------------------------------------------
// SNORM10_10_10_2 packer — replica of the file-static packer in
// Tools/Zenith_Tools_TerrainExport.cpp (the hook must produce bit-identical
// packing to the baked vertex data).
//-----------------------------------------------------------------------------
static uint32_t PackSNORM10_10_10_2(float fX, float fY, float fZ, float fW)
{
	auto Clamp = [](float f) { return std::max(-1.0f, std::min(1.0f, f)); };
	fX = Clamp(fX);
	fY = Clamp(fY);
	fZ = Clamp(fZ);
	fW = Clamp(fW);

	int32_t iR = static_cast<int32_t>(std::round(fX * 511.0f));
	int32_t iG = static_cast<int32_t>(std::round(fY * 511.0f));
	int32_t iB = static_cast<int32_t>(std::round(fZ * 511.0f));
	int32_t iA = static_cast<int32_t>(std::round(fW * 1.0f));

	uint32_t uResult = 0;
	uResult |= (static_cast<uint32_t>(iR) & 0x3FF);
	uResult |= (static_cast<uint32_t>(iG) & 0x3FF) << 10;
	uResult |= (static_cast<uint32_t>(iB) & 0x3FF) << 20;
	uResult |= (static_cast<uint32_t>(iA) & 0x3) << 30;
	return uResult;
}

//-----------------------------------------------------------------------------
// Session lifecycle
//-----------------------------------------------------------------------------

bool Zenith_TerrainEditor::SetAssetSet(const std::string& strSet)
{
	std::string strResolvedDirectory;
	if (!Zenith_TerrainComponent::TryResolveTerrainAssetDirectory(strSet, strResolvedDirectory))
	{
		m_strAssetSetValidationError =
			"Invalid terrain set. Use 1-64 ASCII letters, digits, '_' or '-' (or empty for legacy).";
		m_strStatus = m_strAssetSetValidationError;
		return false;
	}

	// This is deliberately staging-only. An attached live component remains on
	// its persisted set until BakeFull reaches its synchronous commit boundary.
	m_strAssetSet = strSet;
	m_strAssetSetValidationError.clear();
	m_strStatus = m_strAssetSet.empty()
		? "Legacy terrain bake target staged"
		: "Terrain bake target staged: " + m_strAssetSet;
	return true;
}

const std::string& Zenith_TerrainEditor::GetAssetSet() const
{
	return m_strAssetSet;
}

std::string Zenith_TerrainEditor::GetMeshAssetDirectory() const
{
	std::string strDirectory;
	if (!Zenith_TerrainComponent::TryResolveTerrainAssetDirectory(m_strAssetSet, strDirectory))
	{
		Zenith_Assert(false, "Terrain editor stored an invalid staged asset set");
		Zenith_TerrainComponent::TryResolveTerrainAssetDirectory("", strDirectory);
	}
	return strDirectory;
}

std::string Zenith_TerrainEditor::GetTextureAssetDirectory() const
{
	if (!m_strAssetSet.empty())
	{
		return GetMeshAssetDirectory();
	}
	return std::string(Project_GetGameAssetsDirectory()) + "Textures/Terrain/";
}

void Zenith_TerrainEditor::Open(Zenith_EntityID uTerrainEntity)
{
	const bool bResumeDirtySession =
		m_uTargetEntity == uTerrainEntity && m_bSessionDirty;
	if (bResumeDirtySession)
	{
		// Close deliberately keeps the CPU maps, masks, staged set, and undo
		// commands. Reopening the same target re-pins the hook and streams every
		// dirty chunk through it so those exact edits become visible again.
		m_bActive = true;
		m_bEditModeEnabled = true;
		Zenith_TerrainComponent* pxTerrain = ResolveTargetComponent();
		Zenith_Assert(pxTerrain != nullptr, "Terrain editor resume target has no TerrainComponent");
		if (pxTerrain == nullptr)
		{
			return;
		}
		RegisterHook();
		ForceEvictSessionDirtyChunks();
		Zenith_Log(LOG_CATEGORY_EDITOR,
			"[TerrainEditor] Dirty session resumed (entity %u)", uTerrainEntity.m_uIndex);
		return;
	}

	// A different target (or a clean reopen) gets a clean persisted-state load.
	// Tear down the prior attached hook before changing the target ID; dirty
	// visuals are reverted before their CPU maps/masks are discarded below.
	if (m_bActive && m_uTargetEntity != INVALID_ENTITY_ID)
	{
		ClearHook();
		if (m_bSessionDirty)
		{
			ForceEvictSessionDirtyChunks();
		}
	}
	if (m_bSessionDirty)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR,
			"[TerrainEditor] Discarding unbaked session while opening a different target");
	}

	m_uTargetEntity = uTerrainEntity;
	m_bActive = true;
	m_bEditModeEnabled = true;
	m_strStatus.clear();

	// The component is the persisted authority for an attached session. Copy
	// its set before loading so Height/Splatmap/GrassDensity all seed from the
	// same directory the runtime mesh streamer uses.
	Zenith_TerrainComponent* pxTerrain = ResolveTargetComponent();
	Zenith_Assert(pxTerrain != nullptr, "Terrain editor target has no TerrainComponent");
	if (pxTerrain != nullptr)
	{
		m_strAssetSet = pxTerrain->GetTerrainAssetSet();
		std::string strValidatedDirectory;
		const bool bValidPersistedSet = Zenith_TerrainComponent::TryResolveTerrainAssetDirectory(
			m_strAssetSet, strValidatedDirectory);
		Zenith_Assert(bValidPersistedSet, "Terrain component exposed an invalid persisted asset set");
		if (!bValidPersistedSet)
		{
			m_strAssetSet.clear();
		}
	}
	m_strAssetSetValidationError.clear();

	// Undo snapshots and dirty masks refer to the previous CPU image contents.
	// Clear them before reseeding defaults and overlaying this set's files.
	g_xEngine.UndoSystem().Clear();
	m_bSessionDirty = false;
	m_bSplatGPUDirty = false;
	m_bGrassDirty = false;
	m_bErosionRunning = false;
	memset(m_aulSessionDirtyBits, 0, sizeof(m_aulSessionDirtyBits));
	memset(m_aulPendingEvictBits, 0, sizeof(m_aulPendingEvictBits));
	LoadImagesFromAssets();

	RegisterHook();

	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Session opened (entity %u, sessionDirty=%d)",
		uTerrainEntity.m_uIndex, m_bSessionDirty ? 1 : 0);
}

void Zenith_TerrainEditor::OpenStandalone()
{
	m_uTargetEntity = INVALID_ENTITY_ID;
	m_bActive = true;
	m_bEditModeEnabled = false;
	m_strStatus.clear();

	EnsureImagesAllocated();
	if (!m_bSessionDirty)
	{
		LoadImagesFromAssets();
	}

	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Standalone (component-less) session opened");
}

void Zenith_TerrainEditor::Close()
{
	if (!m_bActive)
	{
		return;
	}

	if (m_bStrokeActive)
	{
		EndStroke();
	}
	m_bErosionRunning = false;

	ClearHook();

	// With the hook gone, re-streaming an edited chunk reloads the BAKED data —
	// evict every session-dirty resident chunk so the visuals revert cleanly.
	ForceEvictSessionDirtyChunks();

	m_bActive = false;
	m_bEditModeEnabled = false;
	m_bCursorValid = false;
	m_bConsumedViewportInput = false;

	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Session closed (unbaked edits %s)",
		m_bSessionDirty ? "retained in memory" : "none");
}

//-----------------------------------------------------------------------------
// Target resolution — EntityID re-resolved every use, never cached.
//-----------------------------------------------------------------------------

Zenith_TerrainComponent* Zenith_TerrainEditor::ResolveTargetComponent() const
{
	if (m_uTargetEntity == INVALID_ENTITY_ID)
	{
		return nullptr;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(m_uTargetEntity);
	if (!xEntity.IsValid())
	{
		return nullptr;
	}
	return xEntity.TryGetComponent<Zenith_TerrainComponent>();
}

Flux_TerrainStreamingState* Zenith_TerrainEditor::ResolveTargetState() const
{
	Zenith_TerrainComponent* pxTerrain = ResolveTargetComponent();
	return pxTerrain ? pxTerrain->m_pxStreamingState : nullptr;
}

void Zenith_TerrainEditor::RegisterHook()
{
	Flux_TerrainStreamingState* pxState = ResolveTargetState();
	if (pxState == nullptr)
	{
		return;
	}
	if (pxState->m_pfnChunkVertexHook == &Zenith_TerrainEditor::ChunkVertexHook &&
		pxState->m_pChunkVertexHookUser == this)
	{
		return;
	}
	if (pxState->m_pfnChunkVertexHook != nullptr)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Overriding an existing terrain vertex hook for the editing session");
	}
	// Publish the user pointer before the function pointer (the engine
	// null-checks the fn).
	pxState->m_pChunkVertexHookUser = this;
	pxState->m_pfnChunkVertexHook = &Zenith_TerrainEditor::ChunkVertexHook;
}

void Zenith_TerrainEditor::ClearHook()
{
	Flux_TerrainStreamingState* pxState = ResolveTargetState();
	if (pxState == nullptr)
	{
		return;
	}
	if (pxState->m_pfnChunkVertexHook == &Zenith_TerrainEditor::ChunkVertexHook)
	{
		pxState->m_pfnChunkVertexHook = nullptr;
		pxState->m_pChunkVertexHookUser = nullptr;
	}
}

//-----------------------------------------------------------------------------
// CPU image management
//-----------------------------------------------------------------------------

void Zenith_TerrainEditor::EnsureImagesAllocated()
{
	if (m_xHeightfield.IsEmpty())
	{
		m_xHeightfield = Zenith_Image(uHEIGHTFIELD_SIZE, uHEIGHTFIELD_SIZE);
	}
	if (m_xSplatmap.GetSize() == 0)
	{
		const u_int uBytes = uSPLATMAP_SIZE * uSPLATMAP_SIZE * 4;
		m_xSplatmap.Reserve(uBytes);
		for (u_int u = 0; u < uBytes; u += 4)
		{
			// Default: full weight on layer 0.
			m_xSplatmap.PushBack(255);
			m_xSplatmap.PushBack(0);
			m_xSplatmap.PushBack(0);
			m_xSplatmap.PushBack(0);
		}
	}
	if (m_xGrassDensity.IsEmpty())
	{
		// Default 0 — grass is painted IN (Unity detail-painting convention).
		// A default of 1 would ask the grass system for ~800M blades over the
		// 4096m terrain and slam into its 2M instance cap on the first corner.
		m_xGrassDensity = Zenith_Image(uGRASS_DENSITY_SIZE, uGRASS_DENSITY_SIZE);
	}
}

void Zenith_TerrainEditor::FillImagesWithDefaults()
{
	EnsureImagesAllocated();

	memset(m_xHeightfield.Row(0), 0,
		static_cast<size_t>(uHEIGHTFIELD_SIZE) * uHEIGHTFIELD_SIZE * sizeof(float));

	u_int8* pSplat = m_xSplatmap.GetDataPointer();
	const u_int uSplatTexels = uSPLATMAP_SIZE * uSPLATMAP_SIZE;
	for (u_int u = 0; u < uSplatTexels; u++)
	{
		pSplat[u * 4 + 0] = 255;
		pSplat[u * 4 + 1] = 0;
		pSplat[u * 4 + 2] = 0;
		pSplat[u * 4 + 3] = 0;
	}

	memset(m_xGrassDensity.Row(0), 0,
		static_cast<size_t>(uGRASS_DENSITY_SIZE) * uGRASS_DENSITY_SIZE * sizeof(float));
}

// Read a .ztxtr (DataStream layout: i32 w, i32 h, i32 depth, TextureFormat,
// u64 size, pixels). Returns false when missing or mismatched.
static bool LoadZtxtrRaw(const std::string& strPath, int32_t iExpectWidth, int32_t iExpectHeight,
	TextureFormat eExpectFormat, Zenith_Vector<u_int8>& xPixelsOut)
{
	if (!std::filesystem::exists(strPath))
	{
		return false;
	}

	// Go through the single .ztxtr parser (no GPU upload) rather than hand-parsing.
	// These splat/grass maps are single-mip; xBytes is their full pixel payload.
	Flux_SurfaceInfo xInfo;
	Zenith_Vector<uint8_t> xBytes;
	if (!Zenith_TextureAsset::LoadCPUData(strPath, xInfo, xBytes).IsOk())
	{
		return false;
	}

	if (static_cast<int32_t>(xInfo.m_uWidth) != iExpectWidth || static_cast<int32_t>(xInfo.m_uHeight) != iExpectHeight ||
		xInfo.m_eFormat != eExpectFormat || xBytes.GetSize() == 0)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] %s: unexpected layout (%ux%u fmt %d) - using defaults",
			strPath.c_str(), xInfo.m_uWidth, xInfo.m_uHeight, static_cast<int>(xInfo.m_eFormat));
		return false;
	}

	xPixelsOut.Clear();
	xPixelsOut.Reserve(xBytes.GetSize());
	for (u_int u = 0; u < xBytes.GetSize(); u++)
	{
		xPixelsOut.PushBack(xBytes.Get(u));
	}
	return true;
}

void Zenith_TerrainEditor::LoadImagesFromAssets()
{
	// Every load starts clean. Missing or malformed files therefore retain the
	// map's documented default instead of leaking pixels from a prior set.
	FillImagesWithDefaults();

	const std::string strTexDir = GetTextureAssetDirectory();

	// Heightfield <- Height.ztxtr (R32, 4096^2) via the same loader the export
	// pipeline uses (handles R32/R16/RGBA8 + PNG fallbacks).
	const std::string strHeightPath = strTexDir + "Height" + ZENITH_TEXTURE_EXT;
	if (std::filesystem::exists(strHeightPath))
	{
		Zenith_Image xLoaded = Zenith_Tools_LoadHeightmapAuto(strHeightPath);
		if (xLoaded.GetWidth() == uHEIGHTFIELD_SIZE && xLoaded.GetHeight() == uHEIGHTFIELD_SIZE)
		{
			m_xHeightfield = std::move(xLoaded);
			Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Heightfield seeded from %s", strHeightPath.c_str());
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] %s is %ux%u (want %ux%u) - keeping flat heightfield",
				strHeightPath.c_str(), xLoaded.GetWidth(), xLoaded.GetHeight(), uHEIGHTFIELD_SIZE, uHEIGHTFIELD_SIZE);
		}
	}

	// Splatmap <- Splatmap_RGBA.ztxtr (RGBA8, 2048^2).
	Zenith_Vector<u_int8> xSplatPixels;
	if (LoadZtxtrRaw(strTexDir + "Splatmap_RGBA" + ZENITH_TEXTURE_EXT,
		static_cast<int32_t>(uSPLATMAP_SIZE), static_cast<int32_t>(uSPLATMAP_SIZE),
		TEXTURE_FORMAT_RGBA8_UNORM, xSplatPixels))
	{
		if (xSplatPixels.GetSize() == m_xSplatmap.GetSize())
		{
			memcpy(m_xSplatmap.GetDataPointer(), xSplatPixels.GetDataPointer(), xSplatPixels.GetSize());
			Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Splatmap seeded from Splatmap_RGBA%s", ZENITH_TEXTURE_EXT);
		}
	}

	// Grass density <- GrassDensity.ztxtr (R32, 1024^2).
	Zenith_Vector<u_int8> xGrassPixels;
	if (LoadZtxtrRaw(strTexDir + "GrassDensity" + ZENITH_TEXTURE_EXT,
		static_cast<int32_t>(uGRASS_DENSITY_SIZE), static_cast<int32_t>(uGRASS_DENSITY_SIZE),
		TEXTURE_FORMAT_R32_SFLOAT, xGrassPixels))
	{
		const u_int uExpectBytes = uGRASS_DENSITY_SIZE * uGRASS_DENSITY_SIZE * sizeof(float);
		if (xGrassPixels.GetSize() == uExpectBytes)
		{
			memcpy(m_xGrassDensity.Row(0), xGrassPixels.GetDataPointer(), uExpectBytes);
			Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Grass density seeded from GrassDensity%s", ZENITH_TEXTURE_EXT);
		}
	}
}

//-----------------------------------------------------------------------------
// Per-frame update
//-----------------------------------------------------------------------------

void Zenith_TerrainEditor::ServiceUpdate()
{
	// Painted trees sway via VAT time that the component only advances through
	// its OnUpdate hook — which the ECS dispatches in Playing mode only. Keep
	// the wind alive in Stopped/Paused from here (runs every editor frame,
	// independent of whether a terrain session is open).
	if (g_xEngine.Editor().GetEditorMode() != EditorMode::Playing)
	{
		TickTreeSway(g_xEngine.Frame().GetDt());
	}

	if (!m_bActive || IsStandalone())
	{
		return;
	}

	Zenith_TerrainComponent* pxTerrain = ResolveTargetComponent();
	if (pxTerrain == nullptr)
	{
		// Target died (scene change / entity deletion): suspend the session.
		// CPU images + session-dirty bits survive for a later reopen.
		Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Terrain target no longer resolves - suspending session");
		m_bStrokeActive = false;
		m_bErosionRunning = false;
		m_bActive = false;
		m_bEditModeEnabled = false;
		return;
	}

	// The streaming state can be recreated under us (e.g. a bake re-inits the
	// render resources) — keep the hook pinned every frame.
	RegisterHook();

	if (m_bErosionRunning)
	{
		StepErosionSlice();
	}

	ProcessPendingEvictions();

	// Live splatmap paint feedback: one full-image staged re-upload per dirty
	// frame at most. Race-free — see Zenith_Vulkan_MemoryManager::UpdateTextureVRAM.
	if (m_bSplatGPUDirty && !Zenith_CommandLine::IsHeadless())
	{
		Zenith_TextureAsset* pxSplatTex = pxTerrain->GetSplatmapTexture();
		if (pxSplatTex != nullptr && pxSplatTex->IsValid() &&
			pxSplatTex->GetWidth() == uSPLATMAP_SIZE && pxSplatTex->GetHeight() == uSPLATMAP_SIZE &&
			pxSplatTex->GetFormat() == TEXTURE_FORMAT_RGBA8_UNORM)
		{
			g_xEngine.FluxMemory().UpdateTextureVRAM(pxSplatTex->GetVRAMHandle(),
				m_xSplatmap.GetDataPointer(), pxSplatTex->GetSurfaceInfo());
			m_bSplatGPUDirty = false;
		}
	}
}

void Zenith_TerrainEditor::UpdatePerFrame(const Zenith_TerrainEditorFrameContext& xCtx)
{
	m_bConsumedViewportInput = false;
	m_bCursorValid = false;

	if (!m_bActive || IsStandalone())
	{
		return;
	}

	// Interactive editing suspends whenever the editor leaves Stopped (the
	// play scene is a backup copy; ServiceUpdate keeps servicing it so unbaked
	// edits remain visible during Play).
	if (!xCtx.m_bEditorStopped)
	{
		if (m_bStrokeActive)
		{
			EndStroke();
		}
		return;
	}

	HandleViewportInput(xCtx);

	if (m_bCursorValid && !Zenith_CommandLine::IsHeadless())
	{
		DrawBrushIndicatorDecal();
	}
}

void Zenith_TerrainEditor::HandleViewportInput(const Zenith_TerrainEditorFrameContext& xCtx)
{
	if (!m_bEditModeEnabled)
	{
		if (m_bStrokeActive)
		{
			EndStroke();
		}
		return;
	}

	// Brush radius shortcuts ([ / ]) when the viewport has focus.
	if (xCtx.m_bViewportFocused)
	{
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_LEFT_BRACKET))
		{
			m_xBrush.m_fRadius = std::max(1.0f, m_xBrush.m_fRadius * 0.8f);
		}
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_RIGHT_BRACKET))
		{
			m_xBrush.m_fRadius = std::min(512.0f, m_xBrush.m_fRadius * 1.25f);
		}
	}

	// Cursor ray (viewport-relative mouse, matching HandleObjectPicking).
	Zenith_Maths::Vector2_64 xGlobalMousePos;
	g_xEngine.Input().GetMousePosition(xGlobalMousePos);
	Zenith_Maths::Vector2 xViewportMousePos = {
		static_cast<float>(xGlobalMousePos.x - xCtx.m_xViewportPos.x),
		static_cast<float>(xGlobalMousePos.y - xCtx.m_xViewportPos.y)
	};

	const bool bMouseInViewport = xCtx.m_bViewportHovered &&
		xViewportMousePos.x >= 0.0f && xViewportMousePos.x <= xCtx.m_xViewportSize.x &&
		xViewportMousePos.y >= 0.0f && xViewportMousePos.y <= xCtx.m_xViewportSize.y;

	if (bMouseInViewport)
	{
		const Zenith_Maths::Vector3 xRayDir = g_xEngine.Gizmo().ScreenToWorldRay(
			xViewportMousePos, { 0.0f, 0.0f }, xCtx.m_xViewportSize,
			xCtx.m_xViewMatrix, xCtx.m_xProjMatrix);
		m_bCursorValid = RaycastHeightfield(xCtx.m_xCameraPos, xRayDir, m_xCursorPos);
	}

	// The terrain editor owns viewport clicks while edit mode is armed over
	// the viewport (or mid-stroke) — gizmo + picking are skipped this frame.
	m_bConsumedViewportInput = bMouseInViewport || m_bStrokeActive;

	const bool bLMBHeld = g_xEngine.Input().IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
	const bool bLMBPressed = g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT);
	const bool bCtrl = g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT_CONTROL) || g_xEngine.Input().IsKeyDown(ZENITH_KEY_RIGHT_CONTROL);
	const bool bShift = g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT_SHIFT) || g_xEngine.Input().IsKeyDown(ZENITH_KEY_RIGHT_SHIFT);

	// Ctrl+click eyedroppers: sample the flatten/set-height target, or capture
	// the stamp buffer, from the terrain under the cursor.
	if (bLMBPressed && bCtrl && m_bCursorValid && !m_bStrokeActive)
	{
		if (m_xBrush.m_eTool == Zenith_TerrainBrushTool::Flatten ||
			m_xBrush.m_eTool == Zenith_TerrainBrushTool::SetHeight)
		{
			m_xBrush.m_fTargetHeight = m_xCursorPos.y;
		}
		else if (m_xBrush.m_eTool == Zenith_TerrainBrushTool::Stamp)
		{
			SampleStamp(m_xCursorPos.x, m_xCursorPos.z, m_xBrush.m_fRadius);
		}
		return;
	}

	if (bLMBHeld && !bCtrl && m_bCursorValid)
	{
		if (!m_bStrokeActive)
		{
			BeginStroke();
		}

		// Shift inverts the raise/lower pair (standard sculpting convention)
		// and switches TreePaint into erase.
		Zenith_TerrainBrushTool eTool = m_xBrush.m_eTool;
		if (bShift && eTool == Zenith_TerrainBrushTool::Raise) { eTool = Zenith_TerrainBrushTool::Lower; }
		else if (bShift && eTool == Zenith_TerrainBrushTool::Lower) { eTool = Zenith_TerrainBrushTool::Raise; }

		float fToolValue = 0.0f;
		switch (eTool)
		{
		case Zenith_TerrainBrushTool::Flatten:
		case Zenith_TerrainBrushTool::SetHeight:    fToolValue = m_xBrush.m_fTargetHeight; break;
		case Zenith_TerrainBrushTool::Noise:        fToolValue = m_xBrush.m_fNoiseAmount; break;
		case Zenith_TerrainBrushTool::Terrace:      fToolValue = m_xBrush.m_fTerraceStep; break;
		case Zenith_TerrainBrushTool::SplatPaint:   fToolValue = static_cast<float>(m_xBrush.m_uSplatLayer); break;
		case Zenith_TerrainBrushTool::GrassDensity: fToolValue = m_xBrush.m_fGrassDensity; break;
		case Zenith_TerrainBrushTool::TreePaint:    fToolValue = bShift ? 1.0f : 0.0f; break;
		default: break;
		}

		// Apply dabs at fixed spacing along the cursor path so stroke density
		// is framerate-independent.
		const Zenith_Maths::Vector2 xPos = { m_xCursorPos.x, m_xCursorPos.z };
		if (!m_bStrokeHasLastPos)
		{
			ApplyBrushDab(eTool, xPos.x, xPos.y, m_xBrush.m_fRadius, m_xBrush.m_fStrength, fToolValue);
			m_xStrokeLastPos = xPos;
			m_bStrokeHasLastPos = true;
		}
		else
		{
			const float fSpacing = std::max(m_xBrush.m_fRadius * 0.25f, 0.5f);
			Zenith_Maths::Vector2 xDelta = xPos - m_xStrokeLastPos;
			float fDistance = glm::length(xDelta);
			while (fDistance >= fSpacing)
			{
				m_xStrokeLastPos += (xDelta / fDistance) * fSpacing;
				ApplyBrushDab(eTool, m_xStrokeLastPos.x, m_xStrokeLastPos.y, m_xBrush.m_fRadius, m_xBrush.m_fStrength, fToolValue);
				xDelta = xPos - m_xStrokeLastPos;
				fDistance = glm::length(xDelta);
			}
		}
	}
	else if (m_bStrokeActive)
	{
		EndStroke();
	}
}

//-----------------------------------------------------------------------------
// Brush cursor — terrain-conforming line ring (debug primitives are G-buffer
// LIT, so flat filled shapes wash out; line segments keep their colour).
//-----------------------------------------------------------------------------

Zenith_Maths::Vector3 Zenith_TerrainEditor::GetToolColour() const
{
	switch (m_xBrush.m_eTool)
	{
	case Zenith_TerrainBrushTool::Raise:        return { 0.2f, 1.0f, 0.2f };
	case Zenith_TerrainBrushTool::Lower:        return { 1.0f, 0.25f, 0.2f };
	case Zenith_TerrainBrushTool::Smooth:       return { 1.0f, 1.0f, 0.3f };
	case Zenith_TerrainBrushTool::Flatten:
	case Zenith_TerrainBrushTool::SetHeight:    return { 0.3f, 0.8f, 1.0f };
	case Zenith_TerrainBrushTool::Noise:        return { 1.0f, 0.6f, 0.2f };
	case Zenith_TerrainBrushTool::Terrace:      return { 0.8f, 0.5f, 1.0f };
	case Zenith_TerrainBrushTool::Ramp:         return { 0.2f, 1.0f, 0.8f };
	case Zenith_TerrainBrushTool::Stamp:        return { 1.0f, 0.3f, 1.0f };
	case Zenith_TerrainBrushTool::SplatPaint:   return { 0.3f, 0.4f, 1.0f };
	case Zenith_TerrainBrushTool::GrassDensity: return { 0.5f, 1.0f, 0.4f };
	case Zenith_TerrainBrushTool::TreePaint:    return { 0.15f, 0.75f, 0.3f };
	default:                                    return { 1.0f, 1.0f, 1.0f };
	}
}

void Zenith_TerrainEditor::DrawBrushIndicatorDecal()
{
	// Lazy one-shot resolve: the file is regenerated at every editor boot
	// (RegenerateBrushTextures) before anything reaches this path.
	if (!m_bBrushIndicatorLoadAttempted)
	{
		m_bBrushIndicatorLoadAttempted = true;
		m_xBrushIndicatorTexture = Zenith_AssetRegistry::Acquire<Zenith_TextureAsset>(
			ENGINE_ASSETS_DIR "Textures/Brushes/BrushIndicator" ZENITH_TEXTURE_EXT);
		if (!m_xBrushIndicatorTexture.IsResolved())
		{
			Zenith_Warning(LOG_CATEGORY_EDITOR,
				"[TerrainEditor] Brush indicator texture missing — falling back to the line-ring cursor");
		}
	}

	if (!m_xBrushIndicatorTexture.IsResolved())
	{
		DrawBrushCursor();
		return;
	}

	// Bound the decal box vertically to the TERRAIN'S height envelope inside
	// the brush footprint (we own the authoritative heightfield, so the
	// range is exact). On flat ground this makes the box a thin slab hugging
	// the surface, so props resting ON the terrain (platform decks, cubes,
	// the player) fall outside the volume and the shader's existing box test
	// discards them; on slopes the envelope grows to cover the relief so the
	// indicator never clips on legitimate terrain.
	const float fRadius = m_xBrush.m_fRadius;
	float fMinNorm = 1.0f;
	float fMaxNorm = 0.0f;
	{
		const float fMaxPx = static_cast<float>(uHEIGHTFIELD_SIZE - 1);
		const u_int uX0 = static_cast<u_int>(std::clamp(m_xCursorPos.x - fRadius, 0.0f, fMaxPx));
		const u_int uX1 = static_cast<u_int>(std::clamp(m_xCursorPos.x + fRadius, 0.0f, fMaxPx));
		const u_int uZ0 = static_cast<u_int>(std::clamp(m_xCursorPos.z - fRadius, 0.0f, fMaxPx));
		const u_int uZ1 = static_cast<u_int>(std::clamp(m_xCursorPos.z + fRadius, 0.0f, fMaxPx));
		// Cap the scan at ~64x64 samples — ample accuracy for a visual bound.
		const u_int uStride = std::max(1u, (uX1 - uX0) / 64u);
		for (u_int uZ = uZ0; uZ <= uZ1; uZ += uStride)
		{
			for (u_int uX = uX0; uX <= uX1; uX += uStride)
			{
				const float fH = m_xHeightfield.At(uZ, uX);
				fMinNorm = std::min(fMinNorm, fH);
				fMaxNorm = std::max(fMaxNorm, fH);
			}
		}
	}
	// Pad absorbs the inter-sample relief the strided scan can miss plus the
	// baked-vertex epsilon; props sit >= ~0.5m proud so they stay excluded.
	constexpr float fENVELOPE_PAD = 0.3f;
	const float fMinH = fMinNorm * fTERRAIN_MAX_HEIGHT - fENVELOPE_PAD;
	const float fMaxH = fMaxNorm * fTERRAIN_MAX_HEIGHT + fENVELOPE_PAD;

	const Zenith_Maths::Vector3 xColour = GetToolColour();
	g_xEngine.Decals().SetEditorDecal(
		Zenith_Maths::Vector3(m_xCursorPos.x, (fMinH + fMaxH) * 0.5f, m_xCursorPos.z),
		fRadius * 2.0f,              // SetEditorDecal takes the end-to-end diameter
		fMaxH - fMinH,
		Zenith_Maths::Vector4(xColour, 0.85f),
		m_xBrushIndicatorTexture.GetDirect());
}

void Zenith_TerrainEditor::DrawBrushCursor() const
{
	static constexpr u_int uSEGMENTS = 64;
	static constexpr float fY_OFFSET = 0.4f;

	const Zenith_Maths::Vector3 xColor = GetToolColour();

	const float fRadius = m_xBrush.m_fRadius;
	Zenith_Maths::Vector3 xPrevOuter;
	Zenith_Maths::Vector3 xPrevInner;
	for (u_int u = 0; u <= uSEGMENTS; u++)
	{
		const float fAngle = (static_cast<float>(u) / uSEGMENTS) * 2.0f * 3.14159265f;
		const float fCos = cosf(fAngle);
		const float fSin = sinf(fAngle);

		const float fOuterX = m_xCursorPos.x + fCos * fRadius;
		const float fOuterZ = m_xCursorPos.z + fSin * fRadius;
		const Zenith_Maths::Vector3 xOuter = { fOuterX, SampleHeightWorld(fOuterX, fOuterZ) + fY_OFFSET, fOuterZ };

		const float fInnerX = m_xCursorPos.x + fCos * fRadius * 0.5f;
		const float fInnerZ = m_xCursorPos.z + fSin * fRadius * 0.5f;
		const Zenith_Maths::Vector3 xInner = { fInnerX, SampleHeightWorld(fInnerX, fInnerZ) + fY_OFFSET, fInnerZ };

		if (u > 0)
		{
			g_xEngine.Primitives().AddLine(xPrevOuter, xOuter, xColor, 0.15f);
			g_xEngine.Primitives().AddLine(xPrevInner, xInner, xColor * 0.6f, 0.08f);
		}
		xPrevOuter = xOuter;
		xPrevInner = xInner;
	}

	// Centre marker.
	const Zenith_Maths::Vector3 xCentre = { m_xCursorPos.x, m_xCursorPos.y + fY_OFFSET, m_xCursorPos.z };
	g_xEngine.Primitives().AddLine(xCentre, xCentre + Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f), xColor, 0.15f);
}

//-----------------------------------------------------------------------------
// Height sampling + heightfield raycast
//-----------------------------------------------------------------------------

float Zenith_TerrainEditor::SampleHeightNorm(float fPixelX, float fPixelZ) const
{
	if (m_xHeightfield.IsEmpty())
	{
		return 0.0f;
	}
	const float fMax = static_cast<float>(uHEIGHTFIELD_SIZE - 1);
	fPixelX = std::max(0.0f, std::min(fPixelX, fMax));
	fPixelZ = std::max(0.0f, std::min(fPixelZ, fMax));

	const u_int uX0 = static_cast<u_int>(fPixelX);
	const u_int uZ0 = static_cast<u_int>(fPixelZ);
	const u_int uX1 = std::min(uX0 + 1, uHEIGHTFIELD_SIZE - 1);
	const u_int uZ1 = std::min(uZ0 + 1, uHEIGHTFIELD_SIZE - 1);
	const float fTX = fPixelX - static_cast<float>(uX0);
	const float fTZ = fPixelZ - static_cast<float>(uZ0);

	const float fTop = m_xHeightfield.At(uZ0, uX0) * (1.0f - fTX) + m_xHeightfield.At(uZ0, uX1) * fTX;
	const float fBottom = m_xHeightfield.At(uZ1, uX0) * (1.0f - fTX) + m_xHeightfield.At(uZ1, uX1) * fTX;
	return fTop * (1.0f - fTZ) + fBottom * fTZ;
}

float Zenith_TerrainEditor::SampleHeightWorld(float fWorldX, float fWorldZ) const
{
	// World X/Z == heightmap pixel coordinates (TERRAIN_SCALE == 1).
	return SampleHeightNorm(fWorldX, fWorldZ) * fTERRAIN_MAX_HEIGHT;
}

bool Zenith_TerrainEditor::RaycastHeightfield(const Zenith_Maths::Vector3& xOrigin,
	const Zenith_Maths::Vector3& xDir, Zenith_Maths::Vector3& xHitOut) const
{
	if (m_xHeightfield.IsEmpty())
	{
		return false;
	}

	// Clip the ray to the terrain bounds (a little Y slack on both sides).
	float fTMin = 0.0f;
	float fTMax = 16384.0f;
	const float afBoxMin[3] = { 0.0f, -32.0f, 0.0f };
	const float afBoxMax[3] = { fTERRAIN_WORLD_SIZE, fTERRAIN_MAX_HEIGHT + 32.0f, fTERRAIN_WORLD_SIZE };
	const float afOrigin[3] = { xOrigin.x, xOrigin.y, xOrigin.z };
	const float afDir[3] = { xDir.x, xDir.y, xDir.z };
	for (u_int u = 0; u < 3; u++)
	{
		if (fabsf(afDir[u]) < 1.0e-8f)
		{
			if (afOrigin[u] < afBoxMin[u] || afOrigin[u] > afBoxMax[u])
			{
				return false;
			}
			continue;
		}
		float fT0 = (afBoxMin[u] - afOrigin[u]) / afDir[u];
		float fT1 = (afBoxMax[u] - afOrigin[u]) / afDir[u];
		if (fT0 > fT1) { std::swap(fT0, fT1); }
		fTMin = std::max(fTMin, fT0);
		fTMax = std::min(fTMax, fT1);
		if (fTMin > fTMax)
		{
			return false;
		}
	}

	// Fixed-step march, then bisection refine on the bracketing interval.
	static constexpr float fSTEP = 2.0f;
	float fPrevT = fTMin;
	Zenith_Maths::Vector3 xPrev = xOrigin + xDir * fPrevT;
	float fPrevDelta = xPrev.y - SampleHeightWorld(xPrev.x, xPrev.z);
	if (fPrevDelta < 0.0f)
	{
		// Starting under the surface — return the entry point.
		xHitOut = xPrev;
		return true;
	}

	for (float fT = fTMin + fSTEP; fT <= fTMax + fSTEP; fT += fSTEP)
	{
		const float fClampedT = std::min(fT, fTMax);
		Zenith_Maths::Vector3 xPoint = xOrigin + xDir * fClampedT;
		const float fDelta = xPoint.y - SampleHeightWorld(xPoint.x, xPoint.z);
		if (fDelta < 0.0f)
		{
			// Bracketed: bisect [fPrevT, fClampedT].
			float fLow = fPrevT;
			float fHigh = fClampedT;
			for (u_int u = 0; u < 24; u++)
			{
				const float fMid = (fLow + fHigh) * 0.5f;
				const Zenith_Maths::Vector3 xMid = xOrigin + xDir * fMid;
				if (xMid.y - SampleHeightWorld(xMid.x, xMid.z) < 0.0f)
				{
					fHigh = fMid;
				}
				else
				{
					fLow = fMid;
				}
			}
			const float fHitT = (fLow + fHigh) * 0.5f;
			xHitOut = xOrigin + xDir * fHitT;
			xHitOut.y = SampleHeightWorld(xHitOut.x, xHitOut.z);
			return true;
		}
		fPrevT = fClampedT;
		fPrevDelta = fDelta;
		if (fClampedT >= fTMax)
		{
			break;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Dirty-chunk tracking + budgeted eviction (the race-free GPU update path)
//-----------------------------------------------------------------------------

void Zenith_TerrainEditor::MarkHeightRegionDirty(float fMinPxX, float fMinPxZ, float fMaxPxX, float fMaxPxZ)
{
	// 2px margin: covers the chunk-border verts shared with neighbours and the
	// hook's 1px normal taps.
	const float fMax = static_cast<float>(uHEIGHTFIELD_SIZE - 1);
	fMinPxX = std::max(0.0f, fMinPxX - 2.0f);
	fMinPxZ = std::max(0.0f, fMinPxZ - 2.0f);
	fMaxPxX = std::min(fMax, fMaxPxX + 2.0f);
	fMaxPxZ = std::min(fMax, fMaxPxZ + 2.0f);
	if (fMinPxX > fMaxPxX || fMinPxZ > fMaxPxZ)
	{
		return;
	}

	const float fChunkSize = fTERRAIN_WORLD_SIZE / static_cast<float>(uCHUNK_GRID);
	const u_int uCXMin = static_cast<u_int>(fMinPxX / fChunkSize);
	const u_int uCXMax = std::min(static_cast<u_int>(fMaxPxX / fChunkSize), uCHUNK_GRID - 1);
	const u_int uCZMin = static_cast<u_int>(fMinPxZ / fChunkSize);
	const u_int uCZMax = std::min(static_cast<u_int>(fMaxPxZ / fChunkSize), uCHUNK_GRID - 1);

	// One target resolve for the whole rect (standalone sessions resolve null
	// and just track the dirty bits).
	Flux_TerrainStreamingState* pxState = ResolveTargetState();

	for (u_int uCX = uCXMin; uCX <= uCXMax; uCX++)
	{
		for (u_int uCZ = uCZMin; uCZ <= uCZMax; uCZ++)
		{
			const u_int uIdx = uCX * uCHUNK_GRID + uCZ;
			m_aulPendingEvictBits[uIdx >> 6] |= (1ull << (uIdx & 63));
			m_aulSessionDirtyBits[uIdx >> 6] |= (1ull << (uIdx & 63));
			if (pxState != nullptr)
			{
				UpdateChunkAABB(*pxState, uCX, uCZ);
			}
		}
	}

	m_bSessionDirty = true;

	if (pxState != nullptr)
	{
		pxState->m_bChunkDataDirty.store(true, std::memory_order_release);
	}
}

void Zenith_TerrainEditor::UpdateChunkAABB(Flux_TerrainStreamingState& xState, u_int uChunkX, u_int uChunkZ)
{
	const float fChunkSize = fTERRAIN_WORLD_SIZE / static_cast<float>(uCHUNK_GRID);
	const u_int uPx0 = static_cast<u_int>(uChunkX * fChunkSize);
	const u_int uPz0 = static_cast<u_int>(uChunkZ * fChunkSize);
	const u_int uPx1 = std::min(uPx0 + static_cast<u_int>(fChunkSize), uHEIGHTFIELD_SIZE - 1);
	const u_int uPz1 = std::min(uPz0 + static_cast<u_int>(fChunkSize), uHEIGHTFIELD_SIZE - 1);

	float fMinH = 1.0f;
	float fMaxH = 0.0f;
	for (u_int uZ = uPz0; uZ <= uPz1; uZ++)
	{
		const float* pfRow = m_xHeightfield.Row(uZ);
		for (u_int uX = uPx0; uX <= uPx1; uX++)
		{
			const float fH = pfRow[uX];
			fMinH = std::min(fMinH, fH);
			fMaxH = std::max(fMaxH, fH);
		}
	}

	// EXPAND-only during live editing: shrinking could frustum-cull a chunk
	// whose stale LOW LOD geometry still pokes above the new bounds. The bake
	// recomputes exact AABBs via the full re-init.
	const u_int uIdx = uChunkX * uCHUNK_GRID + uChunkZ;
	Zenith_AABB& xAABB = xState.m_axChunkAABBs[uIdx];
	xAABB.ExpandToInclude(Zenith_Maths::Vector3(static_cast<float>(uPx0), fMinH * fTERRAIN_MAX_HEIGHT - 1.0f, static_cast<float>(uPz0)));
	xAABB.ExpandToInclude(Zenith_Maths::Vector3(static_cast<float>(uPx1), fMaxH * fTERRAIN_MAX_HEIGHT + 1.0f, static_cast<float>(uPz1)));
}

void Zenith_TerrainEditor::ProcessPendingEvictions()
{
	Flux_TerrainStreamingState* pxState = ResolveTargetState();
	if (pxState == nullptr)
	{
		return;
	}

	// Same per-frame cap as the streaming manager's own eviction pass —
	// re-streaming is additionally bounded by MAX_UPLOADS_PER_FRAME, so a
	// large stroke trickles through without a streaming storm.
	u_int uBudget = Flux_TerrainConfig::MAX_EVICTIONS_PER_FRAME;
	for (u_int uWord = 0; uWord < uCHUNK_COUNT / 64 && uBudget > 0; uWord++)
	{
		if (m_aulPendingEvictBits[uWord] == 0)
		{
			continue;
		}
		for (u_int uBit = 0; uBit < 64 && uBudget > 0; uBit++)
		{
			if ((m_aulPendingEvictBits[uWord] & (1ull << uBit)) == 0)
			{
				continue;
			}
			const u_int uIdx = uWord * 64 + uBit;
			m_aulPendingEvictBits[uWord] &= ~(1ull << uBit);

			if (pxState->m_axChunkResidency[uIdx].m_aeStates[Flux_TerrainConfig::LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT)
			{
				g_xEngine.TerrainStreaming().EvictLOD(*pxState, uIdx, Flux_TerrainConfig::LOD_HIGH);
				uBudget--;
			}
			// Not resident: nothing to evict — the next stream-in already runs
			// the hook against the latest heightfield.
		}
	}
}

void Zenith_TerrainEditor::ForceEvictSessionDirtyChunks()
{
	Flux_TerrainStreamingState* pxState = ResolveTargetState();
	if (pxState == nullptr)
	{
		return;
	}

	for (u_int uIdx = 0; uIdx < uCHUNK_COUNT; uIdx++)
	{
		if (!IsChunkSessionDirty(uIdx))
		{
			continue;
		}
		if (pxState->m_axChunkResidency[uIdx].m_aeStates[Flux_TerrainConfig::LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT)
		{
			g_xEngine.TerrainStreaming().EvictLOD(*pxState, uIdx, Flux_TerrainConfig::LOD_HIGH);
		}
		// Pending-evict bit is redundant once force-evicted.
		m_aulPendingEvictBits[uIdx >> 6] &= ~(1ull << (uIdx & 63));
	}
	pxState->m_bChunkDataDirty.store(true, std::memory_order_release);
}

//-----------------------------------------------------------------------------
// Stream-in vertex hook (main thread, called from StreamInLOD after the baked
// chunk mesh loads and BEFORE the GPU upload to a deferred-safe slot).
//-----------------------------------------------------------------------------

void Zenith_TerrainEditor::ChunkVertexHook(void* pUser, uint32_t uChunkX, uint32_t uChunkY,
	void* pVerts, uint32_t uNumVerts, uint32_t uStride)
{
	Zenith_TerrainEditor* pxThis = static_cast<Zenith_TerrainEditor*>(pUser);
	if (pxThis == nullptr || pVerts == nullptr || uNumVerts == 0u || uStride < 28u)
	{
		return;
	}

	// Untouched chunks keep their baked bytes EXACTLY (positions, baked
	// triangle-accumulated normals, everything) — only session-dirty chunks
	// are re-shaped from the live heightfield.
	const u_int uIdx = uChunkX * uCHUNK_GRID + uChunkY;
	if (!pxThis->IsChunkSessionDirty(uIdx))
	{
		return;
	}

	u_int8* pBytes = static_cast<u_int8*>(pVerts);
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		u_int8* pVertex = pBytes + static_cast<size_t>(i) * uStride;
		float* pPos = reinterpret_cast<float*>(pVertex);
		const float* pUV = reinterpret_cast<const float*>(pVertex + 12);

		// Vertex UVs are GLOBAL heightmap pixel coordinates — shared border
		// verts carry identical UVs in both chunks, so absolute-Y rewrite is
		// crack-free by construction.
		const float fU = pUV[0];
		const float fV = pUV[1];

		pPos[1] = pxThis->SampleHeightNorm(fU, fV) * fTERRAIN_MAX_HEIGHT;

		// Recompute the packed normal/tangent from central differences (1px ==
		// 1m spacing) — lighting is the primary depth cue while sculpting.
		const float fHL = pxThis->SampleHeightNorm(fU - 1.0f, fV) * fTERRAIN_MAX_HEIGHT;
		const float fHR = pxThis->SampleHeightNorm(fU + 1.0f, fV) * fTERRAIN_MAX_HEIGHT;
		const float fHD = pxThis->SampleHeightNorm(fU, fV - 1.0f) * fTERRAIN_MAX_HEIGHT;
		const float fHU = pxThis->SampleHeightNorm(fU, fV + 1.0f) * fTERRAIN_MAX_HEIGHT;
		const float fDYDX = (fHR - fHL) * 0.5f;
		const float fDYDZ = (fHU - fHD) * 0.5f;

		const Zenith_Maths::Vector3 xNormal = glm::normalize(Zenith_Maths::Vector3(-fDYDX, 1.0f, -fDYDZ));
		const Zenith_Maths::Vector3 xTangent = glm::normalize(Zenith_Maths::Vector3(1.0f, fDYDX, 0.0f));
		const Zenith_Maths::Vector3 xBitangent = glm::normalize(Zenith_Maths::Vector3(0.0f, fDYDZ, 1.0f));
		const float fSign = glm::dot(glm::cross(xNormal, xTangent), xBitangent) > 0.0f ? 1.0f : -1.0f;

		uint32_t* pNormal = reinterpret_cast<uint32_t*>(pVertex + 20);
		*pNormal = PackSNORM10_10_10_2(xNormal.x, xNormal.y, xNormal.z, 0.0f);
		uint32_t* pTangent = reinterpret_cast<uint32_t*>(pVertex + 24);
		*pTangent = PackSNORM10_10_10_2(xTangent.x, xTangent.y, xTangent.z, fSign);
	}
}

//-----------------------------------------------------------------------------
// Undo region capture / restore
//-----------------------------------------------------------------------------

namespace
{
	// All three maps store 4 bytes per texel (float or RGBA8) — one helper
	// covers region copies for the lot.
	constexpr u_int uTEXEL_BYTES = 4;
	constexpr u_int uUNDO_TILE = 64;   // 64x64 texels = 16 KB per captured tile
}

static u_int GetMapWidth(Zenith_TerrainEditMap eMap)
{
	switch (eMap)
	{
	case Zenith_TerrainEditMap::Height:       return Zenith_TerrainEditor::uHEIGHTFIELD_SIZE;
	case Zenith_TerrainEditMap::Splat:        return Zenith_TerrainEditor::uSPLATMAP_SIZE;
	case Zenith_TerrainEditMap::GrassDensity: return Zenith_TerrainEditor::uGRASS_DENSITY_SIZE;
	default:                                  return 0;
	}
}

static u_int8* GetMapBasePtr(Zenith_TerrainEditor& xEditor, Zenith_TerrainEditMap eMap);

void Zenith_TerrainEditor::CopyMapRegion(Zenith_TerrainEditMap eMap, u_int uX0, u_int uY0,
	u_int uW, u_int uH, Zenith_Vector<u_int8>& xOut) const
{
	const u_int uMapW = GetMapWidth(eMap);
	const u_int8* pBase = GetMapBasePtr(const_cast<Zenith_TerrainEditor&>(*this), eMap);
	if (pBase == nullptr || uMapW == 0)
	{
		return;
	}

	xOut.Clear();
	const u_int uRowBytes = uW * uTEXEL_BYTES;
	xOut.Reserve(uRowBytes * uH);
	for (u_int uRow = 0; uRow < uH; uRow++)
	{
		const u_int8* pSrc = pBase + (static_cast<size_t>(uY0 + uRow) * uMapW + uX0) * uTEXEL_BYTES;
		for (u_int u = 0; u < uRowBytes; u++)
		{
			xOut.PushBack(pSrc[u]);
		}
	}
}

void Zenith_TerrainEditor::WriteMapRegion(Zenith_TerrainEditMap eMap, u_int uX0, u_int uY0,
	u_int uW, u_int uH, const u_int8* pData)
{
	const u_int uMapW = GetMapWidth(eMap);
	u_int8* pBase = GetMapBasePtr(*this, eMap);
	if (pBase == nullptr || uMapW == 0 || pData == nullptr)
	{
		return;
	}

	const u_int uRowBytes = uW * uTEXEL_BYTES;
	for (u_int uRow = 0; uRow < uH; uRow++)
	{
		u_int8* pDst = pBase + (static_cast<size_t>(uY0 + uRow) * uMapW + uX0) * uTEXEL_BYTES;
		memcpy(pDst, pData + static_cast<size_t>(uRow) * uRowBytes, uRowBytes);
	}

	// Re-mark visuals dirty so undo/redo shows up.
	switch (eMap)
	{
	case Zenith_TerrainEditMap::Height:
		MarkHeightRegionDirty(static_cast<float>(uX0), static_cast<float>(uY0),
			static_cast<float>(uX0 + uW - 1), static_cast<float>(uY0 + uH - 1));
		break;
	case Zenith_TerrainEditMap::Splat:
		m_bSplatGPUDirty = true;
		m_bSessionDirty = true;
		break;
	case Zenith_TerrainEditMap::GrassDensity:
		m_bGrassDirty = true;
		m_bSessionDirty = true;
		break;
	}
}

static u_int8* GetMapBasePtr(Zenith_TerrainEditor& xEditor, Zenith_TerrainEditMap eMap)
{
	switch (eMap)
	{
	case Zenith_TerrainEditMap::Height:
		return reinterpret_cast<u_int8*>(const_cast<Zenith_Image&>(xEditor.GetHeightfield()).Row(0));
	case Zenith_TerrainEditMap::Splat:
		return const_cast<Zenith_Vector<u_int8>&>(xEditor.GetSplatmap()).GetDataPointer();
	case Zenith_TerrainEditMap::GrassDensity:
		return reinterpret_cast<u_int8*>(const_cast<Zenith_Image&>(xEditor.GetGrassDensity()).Row(0));
	default:
		return nullptr;
	}
}

void Zenith_TerrainEditor::BeginStroke()
{
	if (m_bStrokeActive)
	{
		return;
	}
	m_bStrokeActive = true;
	m_bStrokeHasLastPos = false;
	m_bStrokeStartCaptured = false;   // the stroke's first dab re-anchors the Ramp corridor
	for (u_int u = 0; u < 3; u++)
	{
		m_axStrokeUndo[u].m_bTouched = false;
		m_axStrokeUndo[u].m_xCapturedTileBits.Clear();
		m_axStrokeUndo[u].m_xTileIndices.Clear();
		m_axStrokeUndo[u].m_xTileData.Clear();
	}
}

void Zenith_TerrainEditor::AccumulateUndoRect(Zenith_TerrainEditMap eMap, u_int uX0, u_int uY0, u_int uX1, u_int uY1)
{
	const u_int uMapW = GetMapWidth(eMap);
	if (uMapW == 0)
	{
		return;
	}
	uX1 = std::min(uX1, uMapW - 1);
	uY1 = std::min(uY1, uMapW - 1);
	if (uX0 > uX1 || uY0 > uY1)
	{
		return;
	}

	StrokeUndoRegion& xRegion = m_axStrokeUndo[static_cast<u_int>(eMap)];

	// Lazily size the captured-tile bitset for this map.
	const u_int uTilesPerSide = uMapW / uUNDO_TILE;
	const u_int uBitWords = (uTilesPerSide * uTilesPerSide + 63) / 64;
	if (xRegion.m_xCapturedTileBits.GetSize() != uBitWords)
	{
		xRegion.m_xCapturedTileBits.Clear();
		for (u_int u = 0; u < uBitWords; u++)
		{
			xRegion.m_xCapturedTileBits.PushBack(0ull);
		}
	}

	// Capture the pre-stroke bytes of every newly-touched 64x64 tile (a tile
	// is captured at most once per stroke, BEFORE the kernel modifies it —
	// callers must accumulate before writing).
	const u_int uTX0 = uX0 / uUNDO_TILE;
	const u_int uTX1 = uX1 / uUNDO_TILE;
	const u_int uTY0 = uY0 / uUNDO_TILE;
	const u_int uTY1 = uY1 / uUNDO_TILE;
	const u_int8* pBase = GetMapBasePtr(*this, eMap);
	for (u_int uTY = uTY0; uTY <= uTY1; uTY++)
	{
		for (u_int uTX = uTX0; uTX <= uTX1; uTX++)
		{
			const u_int uTileIdx = uTY * uTilesPerSide + uTX;
			u_int64& ulWord = xRegion.m_xCapturedTileBits.Get(uTileIdx >> 6);
			const u_int64 ulBit = 1ull << (uTileIdx & 63);
			if (ulWord & ulBit)
			{
				continue;
			}
			ulWord |= ulBit;

			xRegion.m_xTileIndices.PushBack(uTileIdx);
			const u_int uTilePx = uTX * uUNDO_TILE;
			const u_int uTilePy = uTY * uUNDO_TILE;
			for (u_int uRow = 0; uRow < uUNDO_TILE; uRow++)
			{
				const u_int8* pSrc = pBase + (static_cast<size_t>(uTilePy + uRow) * uMapW + uTilePx) * uTEXEL_BYTES;
				for (u_int u = 0; u < uUNDO_TILE * uTEXEL_BYTES; u++)
				{
					xRegion.m_xTileData.PushBack(pSrc[u]);
				}
			}
		}
	}

	if (!xRegion.m_bTouched)
	{
		xRegion.m_bTouched = true;
		xRegion.m_uX0 = uX0; xRegion.m_uY0 = uY0;
		xRegion.m_uX1 = uX1; xRegion.m_uY1 = uY1;
	}
	else
	{
		xRegion.m_uX0 = std::min(xRegion.m_uX0, uX0);
		xRegion.m_uY0 = std::min(xRegion.m_uY0, uY0);
		xRegion.m_uX1 = std::max(xRegion.m_uX1, uX1);
		xRegion.m_uY1 = std::max(xRegion.m_uY1, uY1);
	}
}

void Zenith_TerrainEditor::PushStrokeUndoCommand()
{
	for (u_int uMap = 0; uMap < 3; uMap++)
	{
		StrokeUndoRegion& xRegion = m_axStrokeUndo[uMap];
		if (!xRegion.m_bTouched)
		{
			continue;
		}
		const Zenith_TerrainEditMap eMap = static_cast<Zenith_TerrainEditMap>(uMap);
		const u_int uMapW = GetMapWidth(eMap);
		const u_int uW = xRegion.m_uX1 - xRegion.m_uX0 + 1;
		const u_int uH = xRegion.m_uY1 - xRegion.m_uY0 + 1;

		// After = the current (post-stroke) bytes of the union rect.
		Zenith_Vector<u_int8> xAfter;
		CopyMapRegion(eMap, xRegion.m_uX0, xRegion.m_uY0, uW, uH, xAfter);

		// Before = after, with every captured tile's pre-stroke bytes pasted
		// over its intersection with the rect (texels inside the rect that no
		// dab touched were never modified, so current == before there).
		Zenith_Vector<u_int8> xBefore;
		xBefore.Reserve(xAfter.GetSize());
		for (u_int u = 0; u < xAfter.GetSize(); u++)
		{
			xBefore.PushBack(xAfter.Get(u));
		}
		const u_int uTilesPerSide = uMapW / uUNDO_TILE;
		const u_int uTileBytes = uUNDO_TILE * uUNDO_TILE * uTEXEL_BYTES;
		for (u_int uTile = 0; uTile < xRegion.m_xTileIndices.GetSize(); uTile++)
		{
			const u_int uTileIdx = xRegion.m_xTileIndices.Get(uTile);
			const u_int uTilePx = (uTileIdx % uTilesPerSide) * uUNDO_TILE;
			const u_int uTilePy = (uTileIdx / uTilesPerSide) * uUNDO_TILE;
			const u_int8* pTile = xRegion.m_xTileData.GetDataPointer() + static_cast<size_t>(uTile) * uTileBytes;

			const u_int uIX0 = std::max(uTilePx, xRegion.m_uX0);
			const u_int uIX1 = std::min(uTilePx + uUNDO_TILE - 1, xRegion.m_uX1);
			const u_int uIY0 = std::max(uTilePy, xRegion.m_uY0);
			const u_int uIY1 = std::min(uTilePy + uUNDO_TILE - 1, xRegion.m_uY1);
			for (u_int uY = uIY0; uY <= uIY1; uY++)
			{
				const u_int8* pSrc = pTile + (static_cast<size_t>(uY - uTilePy) * uUNDO_TILE + (uIX0 - uTilePx)) * uTEXEL_BYTES;
				u_int8* pDst = xBefore.GetDataPointer() + (static_cast<size_t>(uY - xRegion.m_uY0) * uW + (uIX0 - xRegion.m_uX0)) * uTEXEL_BYTES;
				memcpy(pDst, pSrc, static_cast<size_t>(uIX1 - uIX0 + 1) * uTEXEL_BYTES);
			}
		}

		// Live-budget enforcement: clear the stack (freeing commands returns
		// their bytes) rather than silently dropping history piecemeal.
		const u_int64 ulNewBytes = static_cast<u_int64>(xBefore.GetSize()) + xAfter.GetSize();
		if (m_ulUndoBytesLive + ulNewBytes > ulUNDO_BUDGET_BYTES)
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Terrain undo budget exceeded (%llu MB live) - clearing undo history",
				static_cast<unsigned long long>(m_ulUndoBytesLive >> 20));
			g_xEngine.UndoSystem().Clear();
		}

		Zenith_UndoCommand_TerrainEdit* pxCommand = new Zenith_UndoCommand_TerrainEdit(
			*this, eMap, xRegion.m_uX0, xRegion.m_uY0, uW, uH, std::move(xBefore), std::move(xAfter));
		g_xEngine.UndoSystem().Execute(pxCommand);

		xRegion.m_bTouched = false;
		xRegion.m_xCapturedTileBits.Clear();
		xRegion.m_xTileIndices.Clear();
		xRegion.m_xTileData.Clear();
	}
}

void Zenith_TerrainEditor::EndStroke()
{
	if (!m_bStrokeActive)
	{
		return;
	}
	m_bStrokeActive = false;
	m_bStrokeHasLastPos = false;
	m_bStrokeStartCaptured = false;
	PushStrokeUndoCommand();

	// Grass placement rebuild is a few hundred ms — stroke-end cadence, never
	// per dab.
	if (m_bGrassDirty)
	{
		RebuildGrass();
	}
}

void Zenith_TerrainEditor::ResetImagesToDefaults()
{
	// Undo commands hold region snapshots of these maps — invalid after reset.
	g_xEngine.UndoSystem().Clear();
	FillImagesWithDefaults();

	MarkHeightRegionDirty(0.0f, 0.0f,
		static_cast<float>(uHEIGHTFIELD_SIZE - 1), static_cast<float>(uHEIGHTFIELD_SIZE - 1));
	m_bSplatGPUDirty = true;
	m_bGrassDirty = true;

	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Session images reset to defaults");
}

//-----------------------------------------------------------------------------
// Stamp capture
//-----------------------------------------------------------------------------

void Zenith_TerrainEditor::SampleStamp(float fCentreX, float fCentreZ, float fRadius)
{
	const u_int uSize = std::max(8u, std::min(static_cast<u_int>(fRadius * 2.0f), 1024u));
	m_xStampData.Clear();
	m_xStampData.Reserve(uSize * uSize);

	const float fStep = (fRadius * 2.0f) / static_cast<float>(uSize - 1);
	const float fX0 = fCentreX - fRadius;
	const float fZ0 = fCentreZ - fRadius;
	float fMinHeight = 1.0f;
	for (u_int uZ = 0; uZ < uSize; uZ++)
	{
		for (u_int uX = 0; uX < uSize; uX++)
		{
			const float fH = SampleHeightNorm(fX0 + uX * fStep, fZ0 + uZ * fStep);
			fMinHeight = std::min(fMinHeight, fH);
			m_xStampData.PushBack(fH);
		}
	}
	m_uStampSize = uSize;
	m_fStampReferenceHeight = fMinHeight;
	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Stamp captured (%ux%u from r=%.1fm)", uSize, uSize, fRadius);
}

void Zenith_TerrainEditor::ClearStamp()
{
	m_xStampData.Clear();
	m_uStampSize = 0;
}

//-----------------------------------------------------------------------------
// Auto-splat rules accessors (RunAutoSplat lives in _Procedural.cpp)
//-----------------------------------------------------------------------------

void Zenith_TerrainEditor::SetAutoSplatRule(u_int uSlot, const Zenith_TerrainAutoSplatRule& xRule)
{
	Zenith_Assert(uSlot < 4, "Auto-splat slot out of range");
	m_axAutoSplatRules[uSlot] = xRule;
}

const Zenith_TerrainAutoSplatRule& Zenith_TerrainEditor::GetAutoSplatRule(u_int uSlot) const
{
	Zenith_Assert(uSlot < 4, "Auto-splat slot out of range");
	return m_axAutoSplatRules[uSlot];
}

float Zenith_TerrainEditor::GetErosionProgress() const
{
	if (!m_bErosionRunning)
	{
		return 1.0f;
	}
	const u_int uTotal = m_xActiveErosion.m_uHydraulicDroplets + m_uErosionThermalRowsTotal;
	const u_int uDone = m_uErosionDropletsDone + m_uErosionThermalRowsDone;
	return uTotal > 0 ? static_cast<float>(uDone) / static_cast<float>(uTotal) : 1.0f;
}

#include "Editor/TerrainEditor/Zenith_TerrainEditor.Tests.inl"

#endif // ZENITH_TOOLS
