#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"

// Forward declarations
class Zenith_MaterialAsset;

//=============================================================================
// Zenith_MaterialEditorPanel - UE5/Unity-style material editor.
//
// A dockable ImGui window editing one .zmtrl asset at a time:
//   - a LIVE preview viewport (the material-preview RENDER VIEW: the full Flux
//     pipeline re-run for a second view containing only the preview mesh —
//     Flux_MaterialPreviewController in Flux/RenderViews/) with a mesh selector
//     (sphere/cube/plane/cylinder), orbit (LMB-drag), zoom (wheel), and a
//     rotatable per-view light (L+drag, UE convention),
//   - a parent slot (drag-drop a material to make this an instance),
//   - grouped, auto-generated property foldouts driven by the
//     Zenith_MaterialParamTable reflection table, with UE-style pin gating
//     (AlphaCutoff only under Masked, POM only with a height map, ...) and
//     per-row override checkboxes + reset arrows when parented,
//   - 9 texture slots with inline channel-packing hints + drag-drop,
//   - a stats line (texture VRAM, active feature flags, blend/shading mode),
//   - Save / Save As / Load / Reload.
//
// Follows the Zenith_GraphEditorPanel pattern: static API, atomic Action_*
// verbs (driven by Zenith_EditorAutomation's AddStep_Material* steps and by
// automated tests via simulated input), and ZENITH_TESTING screen-rect
// accessors so tests can click real coordinates. The "active material" is the
// editor's current selection (Zenith_Editor::GetSelectedMaterial).
//=============================================================================
class Zenith_MaterialEditorPanel
{
public:
	// Renders the window when shown. Called from Zenith_Editor::RenderMaterialEditorPanel.
	static void Render();

	//--------------------------------------------------------------------------
	// Atomic editor actions - the EXACT operations the panel's UI handlers run.
	// All return false (and change nothing) on bad input.
	//--------------------------------------------------------------------------

	// Create a new material, register it, and select it (becomes the active
	// material). szAssetPath may be empty for an unsaved procedural material.
	static bool Action_CreateMaterial(const char* szAssetPath);
	// Load an existing .zmtrl and select it.
	static bool Action_OpenMaterial(const char* szAssetPath);
	// Save the active material. szAssetPath==nullptr/"" saves to its current path.
	static bool Action_SaveMaterial(const char* szAssetPath);

	// Edit a parameter on the active material by its reflection-table name
	// ("Roughness", "BaseColor", ...). When the material is an instance, this
	// also marks the parameter's override bit (mirrors the typed setters).
	static bool Action_SetParamFloat(const char* szParamName, float fValue);
	static bool Action_SetParamColor(const char* szParamName, float fR, float fG, float fB, float fA);
	static bool Action_SetParamInt(const char* szParamName, int iValue);	// enums + bools
	// Convenience for the blend-mode meta-dropdown (== SetParamInt("BlendMode", i)).
	static bool Action_SetBlendMode(int iBlendMode);

	// Bind a texture by slot name ("BaseColor", "Normal", "Height", ...).
	// Empty path clears the slot.
	static bool Action_SetTexture(const char* szSlotName, const char* szTexturePath);

	// Instance authoring.
	static bool Action_SetParent(const char* szParentAssetPath);	// nullptr/"" clears
	static bool Action_SetOverride(const char* szParamName, bool bOverridden);

	// Preview controls.
	static bool Action_SetPreviewMesh(int iMesh);
	static bool Action_SetPreviewLight(float fYaw, float fPitch);

#ifdef ZENITH_TESTING
	// Live screen-rect accessors (recorded during the most recent Render).
	// Tests must query AFTER a rendered frame.
	static bool GetParamRowScreenRect(const char* szParamName, Zenith_Maths::Vector2& xOutMin, Zenith_Maths::Vector2& xOutMax);
	static bool GetTextureSlotScreenPos(const char* szSlotName, Zenith_Maths::Vector2& xOut);
	static bool GetToolbarButtonScreenPos(const char* szLabel, Zenith_Maths::Vector2& xOut);
	static bool GetPreviewImageScreenRect(Zenith_Maths::Vector2& xOutMin, Zenith_Maths::Vector2& xOutMax);
	static bool IsOpen();
	static const char* GetOpenMaterialPath();
	static bool IsDirty();
#endif
};

#endif // ZENITH_TOOLS
