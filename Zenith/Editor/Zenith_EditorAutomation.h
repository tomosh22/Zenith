#pragma once

#ifdef ZENITH_TOOLS

#include "Collections/Zenith_Vector.h"

//=============================================================================
// Editor Automation System
//
// Replaces Project_CreateScenes() with a sequence of atomic editor actions.
// Each step simulates a single user interaction (button click, field edit).
// Execution is driven by g_xEngine.Editor().Update() — one step per frame with
// full frame ticking (rendering, physics, scene updates) between steps.
//
// High-level operations (scene create/save/unload, entity create/select,
// component add, main camera set, behaviour set) route through Zenith_Editor
// methods, ensuring identical code paths to ImGui panels.
// Field-level edits (camera, transform, UI, particles, colliders, models)
// access component setters directly — matching what the properties panel
// does after ImGui widget interaction. Scene-level operations that have
// no ImGui UI equivalent (RegisterSceneBuildIndex, LoadSceneByIndex,
// LoadInitialScene) call Zenith_SceneManager directly.
//=============================================================================

// Forward declarations
class Flux_ParticleEmitterConfig;
class Zenith_MaterialAsset;
class Flux_MeshGeometry;

//-----------------------------------------------------------------------------
// Action Types
//-----------------------------------------------------------------------------
enum class Zenith_EditorActionType
{
	// Scene operations (via Zenith_Editor scene methods)
	CREATE_SCENE,
	SAVE_SCENE,
	UNLOAD_SCENE,

	// Entity operations (via Zenith_Editor entity methods)
	CREATE_ENTITY,
	SELECT_ENTITY,
	SET_ENTITY_TRANSIENT,

	// Component addition (via Zenith_Editor::AddComponentToSelected)
	ADD_COMPONENT,

	// Camera field edits
	SET_CAMERA_POSITION,
	SET_CAMERA_PITCH,
	SET_CAMERA_YAW,
	SET_CAMERA_FOV,
	SET_CAMERA_NEAR,
	SET_CAMERA_FAR,
	SET_CAMERA_ASPECT,
	SET_MAIN_CAMERA,

	// Transform field edits
	SET_TRANSFORM_POSITION,
	SET_TRANSFORM_SCALE,
	SET_TRANSFORM_ROTATION_YAW,

	// Light field edits
	SET_LIGHT_INTENSITY,
	SET_LIGHT_RANGE,
	SET_LIGHT_COLOR,

	// UI element creation and field edits
	CREATE_UI_TEXT,
	CREATE_UI_BUTTON,
	CREATE_UI_RECT,
	CREATE_UI_IMAGE,
	SET_UI_IMAGE_TEXTURE_PATH,
	SET_UI_ANCHOR,
	SET_UI_POSITION,
	SET_UI_SIZE,
	SET_UI_FONT_SIZE,
	SET_UI_COLOR,
	SET_UI_ALIGNMENT,
	SET_UI_VISIBLE,

	// UI layout group creation and field edits
	CREATE_UI_LAYOUT_GROUP,
	ADD_UI_CHILD,
	SET_UI_LAYOUT_DIRECTION,
	SET_UI_LAYOUT_SPACING,
	SET_UI_LAYOUT_CHILD_ALIGNMENT,
	SET_UI_LAYOUT_PADDING,
	SET_UI_LAYOUT_FIT_TO_CONTENT,
	SET_UI_LAYOUT_CHILD_FORCE_EXPAND,
	SET_UI_LAYOUT_REVERSE,

	// UI button-specific field edits
	SET_UI_BUTTON_NORMAL_COLOR,
	SET_UI_BUTTON_HOVER_COLOR,
	SET_UI_BUTTON_PRESSED_COLOR,
	SET_UI_BUTTON_FONT_SIZE,

	// UI Button icon
	SET_UI_BUTTON_ICON,
	SET_UI_BUTTON_ICON_SIZE,
	SET_UI_BUTTON_ICON_PLACEMENT,

	// UIRect styling
	SET_UI_CORNER_RADIUS,
	SET_UI_GRADIENT_COLOR,
	SET_UI_SHADOW,
	SET_UI_SHADOW_COLOR,
	SET_UI_RECT_BORDER,

	// UIText shadow
	SET_UI_TEXT_SHADOW,
	SET_UI_TEXT_SHADOW_COLOR,

	// UIElement background
	SET_UI_BACKGROUND_COLOR,
	SET_UI_BACKGROUND_CORNER_RADIUS,
	SET_UI_BACKGROUND_BORDER,

	// UIButton styling
	SET_UI_BUTTON_CORNER_RADIUS,
	SET_UI_BUTTON_SHADOW,
	SET_UI_BUTTON_SHADOW_COLOR,
	SET_UI_BUTTON_GRADIENT_COLOR,
	SET_UI_BUTTON_BORDER_COLOR,
	SET_UI_BUTTON_BORDER_THICKNESS,
	SET_UI_BUTTON_TRANSITION_DURATION,
	SET_UI_BUTTON_TEXT_SHADOW,
	SET_UI_BUTTON_TEXT_SHADOW_COLOR,

	// UI Toggle
	CREATE_UI_TOGGLE,
	SET_UI_TOGGLE_ON_COLOR,
	SET_UI_TOGGLE_OFF_COLOR,

	// UI Overlay
	CREATE_UI_OVERLAY,
	SET_UI_OVERLAY_DIM_COLOR,
	SET_UI_OVERLAY_CONTENT_SIZE,

	// UI Focus Navigation
	SET_UI_NAVIGATION,

	// UI ScrollView
	CREATE_UI_SCROLL_VIEW,
	SET_UI_SCROLL_VIEW_CONTENT_SIZE,

	// Script (via Zenith_Editor::AttachScriptForSerializationToSelected)
	ATTACH_SCRIPT,

	// Particles
	SET_PARTICLE_CONFIG,
	SET_PARTICLE_CONFIG_BY_NAME,
	SET_PARTICLE_EMITTING,

	// Collider
	ADD_COLLIDER_SHAPE,

	// Model
	ADD_MESH_ENTRY,
	LOAD_MODEL,
	SET_MODEL_MATERIAL,

	// Terrain
	SET_TERRAIN_MATERIAL,
	SET_TERRAIN_SPLATMAP_PATH,

	// Prefab variant authoring (Phase 3 of the readability plan).
	// CREATE_PREFAB_FROM_SELECTED captures the currently-selected entity into a
	// new Zenith_Prefab and writes it to disk. CREATE_PREFAB_VARIANT loads a base
	// prefab through the asset registry and writes a derived variant that
	// inherits from it. ADD_PREFAB_VARIANT_OVERRIDE_VEC3 appends a single
	// Vector3 override to an on-disk variant. INSTANTIATE_PREFAB reads a prefab
	// from disk and instantiates it into the active scene, selecting the result.
	CREATE_PREFAB_FROM_SELECTED,
	CREATE_PREFAB_VARIANT,
	ADD_PREFAB_VARIANT_OVERRIDE_VEC3,
	INSTANTIATE_PREFAB,

	// Scene loading
	LOAD_INITIAL_SCENE,                 // Combined: registers the initial-scene-load callback,
	                                    // then invokes it once under a lifecycle-deferral guard.
	                                    // Replaces the SET_LOADING_SCENE(true) + CUSTOM +
	                                    // SET_LOADING_SCENE(false) triplet plus the separate
	                                    // SET_INITIAL_SCENE_LOAD_CALLBACK step.

	// Custom step (game-specific logic as function pointer)
	CUSTOM_STEP,
};

//-----------------------------------------------------------------------------
// Action Data
//-----------------------------------------------------------------------------
struct Zenith_EditorAction
{
	Zenith_EditorActionType m_eType = Zenith_EditorActionType::CUSTOM_STEP;
	// IMPORTANT: String pointers must point to static storage (string literals,
	// static const arrays) that outlives the action queue. Do NOT pass
	// std::string::c_str() or stack buffers — they will be dangling when executed.
	const char* m_szArg1 = nullptr;
	const char* m_szArg2 = nullptr;
	float m_afArgs[4] = {};
	int m_aiArgs[2] = {};
	bool m_bArg = false;
	void* m_pArg = nullptr;   // Type determined by m_eType (e.g. Flux_ParticleEmitterConfig*, Flux_MeshGeometry*)
	void* m_pArg2 = nullptr;  // Type determined by m_eType (e.g. Zenith_MaterialAsset*)
	void (*m_pfnFunc)() = nullptr;
};

//-----------------------------------------------------------------------------
// Automation Class
//-----------------------------------------------------------------------------
class Zenith_EditorAutomation
{
public:
	//--------------------------------------------------------------------------
	// Execution
	//--------------------------------------------------------------------------
void Begin();
bool IsRunning();
bool IsComplete();
void ExecuteNextStep();
void Reset();

	//--------------------------------------------------------------------------
	// Scene Step Helpers
	//--------------------------------------------------------------------------
void AddStep_CreateScene(const char* szName);
void AddStep_SaveScene(const char* szPath);
void AddStep_UnloadScene();

	//--------------------------------------------------------------------------
	// Entity Step Helpers
	//--------------------------------------------------------------------------
void AddStep_CreateEntity(const char* szName);
void AddStep_SelectEntity(const char* szName);
void AddStep_SetEntityTransient(bool bTransient);

	//--------------------------------------------------------------------------
	// Component Step Helpers
	//--------------------------------------------------------------------------
void AddStep_AddComponent(const char* szDisplayName);

	// Convenience wrappers for common components
void AddStep_AddCamera() { AddStep_AddComponent("Camera"); }
void AddStep_AddUI() { AddStep_AddComponent("UI"); }
	// AddStep_AddScript removed - use AddStep_AttachScript("Foo_Behaviour") instead.
	// AttachScript implicitly adds the ScriptComponent if missing.
void AddStep_AddParticleEmitter() { AddStep_AddComponent("ParticleEmitter"); }
void AddStep_AddCollider() { AddStep_AddComponent("Collider"); }
void AddStep_AddModel() { AddStep_AddComponent("Model"); }
void AddStep_AddAnimator() { AddStep_AddComponent("Animator"); }

	//--------------------------------------------------------------------------
	// Camera Step Helpers
	//--------------------------------------------------------------------------
void AddStep_SetCameraPosition(float fX, float fY, float fZ);
void AddStep_SetCameraPitch(float fPitch);
void AddStep_SetCameraYaw(float fYaw);
void AddStep_SetCameraFOV(float fFOV);
void AddStep_SetCameraNear(float fNear);
void AddStep_SetCameraFar(float fFar);
void AddStep_SetCameraAspect(float fAspect);
void AddStep_SetAsMainCamera();

	//--------------------------------------------------------------------------
	// Transform Step Helpers
	//--------------------------------------------------------------------------
void AddStep_SetTransformPosition(float fX, float fY, float fZ);
void AddStep_SetTransformScale(float fX, float fY, float fZ);
	// Yaw-only rotation (radians) around the Y axis. Sufficient for the
	// common "place an actor flat on the ground at angle θ" pattern that
	// dominates DP scene authoring (UE author rotations imported as yaw).
void AddStep_SetTransformYaw(float fYawRadians);

	// Light component field edits. Apply to the selected entity's
	// Zenith_LightComponent — set after AddStep_AddComponent("Light").
void AddStep_SetLightIntensity(float fLumens);
void AddStep_SetLightRange(float fMetres);
void AddStep_SetLightColor(float fR, float fG, float fB);

	//--------------------------------------------------------------------------
	// UI Step Helpers
	//--------------------------------------------------------------------------
void AddStep_CreateUIText(const char* szName, const char* szText);
void AddStep_CreateUIButton(const char* szName, const char* szText);
void AddStep_CreateUIRect(const char* szName);
void AddStep_CreateUIImage(const char* szName);
void AddStep_SetUIImageTexturePath(const char* szElement, const char* szTexturePath);
void AddStep_SetUIAnchor(const char* szElement, int iPreset);
void AddStep_SetUIPosition(const char* szElement, float fX, float fY);
void AddStep_SetUISize(const char* szElement, float fW, float fH);
void AddStep_SetUIFontSize(const char* szElement, float fSize);
void AddStep_SetUIColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIAlignment(const char* szElement, int iAlignment);
void AddStep_SetUIVisible(const char* szElement, bool bVisible);

	//--------------------------------------------------------------------------
	// UI Layout Group Step Helpers
	//--------------------------------------------------------------------------
void AddStep_CreateUILayoutGroup(const char* szName);
void AddStep_AddUIChild(const char* szParent, const char* szChild);
void AddStep_SetUILayoutDirection(const char* szElement, int iDirection);
void AddStep_SetUILayoutSpacing(const char* szElement, float fSpacing);
void AddStep_SetUILayoutChildAlignment(const char* szElement, int iAlignment);
void AddStep_SetUILayoutPadding(const char* szElement, float fL, float fT, float fR, float fB);
void AddStep_SetUILayoutFitToContent(const char* szElement, bool bFit);
void AddStep_SetUILayoutChildForceExpand(const char* szElement, bool bWidth, bool bHeight);
void AddStep_SetUILayoutReverse(const char* szElement, bool bReverse);

	//--------------------------------------------------------------------------
	// UI Toggle Step Helpers
	//--------------------------------------------------------------------------
void AddStep_CreateUIToggle(const char* szName, const char* szText);
void AddStep_SetUIToggleOnColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIToggleOffColor(const char* szElement, float fR, float fG, float fB, float fA);

	//--------------------------------------------------------------------------
	// UI Overlay Step Helpers
	//--------------------------------------------------------------------------
void AddStep_CreateUIOverlay(const char* szName);
void AddStep_SetUIOverlayDimColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIOverlayContentSize(const char* szElement, float fW, float fH);

	//--------------------------------------------------------------------------
	// UI Focus Navigation Step Helpers
	//--------------------------------------------------------------------------
void AddStep_SetUINavigation(const char* szElement, const char* szUp, const char* szDown, const char* szLeft, const char* szRight);

	//--------------------------------------------------------------------------
	// UI ScrollView Step Helpers
	//--------------------------------------------------------------------------
void AddStep_CreateUIScrollView(const char* szName);
void AddStep_SetUIScrollViewContentSize(const char* szElement, float fW, float fH);

	//--------------------------------------------------------------------------
	// UI Button Step Helpers
	//--------------------------------------------------------------------------
void AddStep_SetUIButtonNormalColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIButtonHoverColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIButtonPressedColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIButtonFontSize(const char* szElement, float fSize);
void AddStep_SetUIButtonIcon(const char* szElement, const char* szTexturePath);
void AddStep_SetUIButtonIconSize(const char* szElement, float fW, float fH);
void AddStep_SetUIButtonIconPlacement(const char* szElement, int iPlacement);

	//--------------------------------------------------------------------------
	// UIElement Background Step Helpers
	//--------------------------------------------------------------------------
void AddStep_SetUIBackgroundColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIBackgroundCornerRadius(const char* szElement, float fRadius);
void AddStep_SetUIBackgroundBorder(const char* szElement, float fR, float fG, float fB, float fThickness);

	//--------------------------------------------------------------------------
	// UIRect Styling Step Helpers
	//--------------------------------------------------------------------------
void AddStep_SetUICornerRadius(const char* szElement, float fRadius);
void AddStep_SetUIGradientColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIShadow(const char* szElement, float fOffX, float fOffY, float fSpread, bool bEnabled);
void AddStep_SetUIShadowColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIRectBorder(const char* szElement, float fR, float fG, float fB, float fThickness);

	//--------------------------------------------------------------------------
	// UIText Shadow Step Helpers
	//--------------------------------------------------------------------------
void AddStep_SetUITextShadow(const char* szElement, float fOffX, float fOffY, bool bEnabled);
void AddStep_SetUITextShadowColor(const char* szElement, float fR, float fG, float fB, float fA);

	//--------------------------------------------------------------------------
	// UIButton Styling Step Helpers
	//--------------------------------------------------------------------------
void AddStep_SetUIButtonCornerRadius(const char* szElement, float fRadius);
void AddStep_SetUIButtonShadow(const char* szElement, float fOffX, float fOffY, float fSpread, bool bEnabled);
void AddStep_SetUIButtonShadowColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIButtonGradientColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIButtonBorderColor(const char* szElement, float fR, float fG, float fB, float fA);
void AddStep_SetUIButtonBorderThickness(const char* szElement, float fThickness);
void AddStep_SetUIButtonTransitionDuration(const char* szElement, float fDuration);
void AddStep_SetUIButtonTextShadow(const char* szElement, float fOffX, float fOffY, bool bEnabled);
void AddStep_SetUIButtonTextShadowColor(const char* szElement, float fR, float fG, float fB, float fA);

	//--------------------------------------------------------------------------
	// Script Step Helpers
	//--------------------------------------------------------------------------
	// AttachScript adds a script slot (and the ScriptComponent if missing) to the selected
	// entity. The behaviour type name resolves deterministically to "game:Scripts/<TypeName>.zscript".
	// Like the old AddStep_SetBehaviourForSerialization, this does NOT call OnAwake -
	// lifecycle is dispatched on first frame / Play mode entry.
void AddStep_AttachScript(const char* szBehaviourTypeName);

	//--------------------------------------------------------------------------
	// Particle Step Helpers
	//--------------------------------------------------------------------------
void AddStep_SetParticleConfig(Flux_ParticleEmitterConfig* pxConfig);
void AddStep_SetParticleConfigByName(const char* szConfigName);
void AddStep_SetParticleEmitting(bool bEmitting);

	//--------------------------------------------------------------------------
	// Collider Step Helpers
	//--------------------------------------------------------------------------
void AddStep_AddColliderShape(int iVolumeType, int iBodyType);

	//--------------------------------------------------------------------------
	// Model Step Helpers
	//--------------------------------------------------------------------------
void AddStep_AddMeshEntry(Flux_MeshGeometry* pxGeometry, Zenith_MaterialAsset* pxMaterial);

	// Load a .zmodel into the selected entity's ModelComponent. Survives
	// SaveScene/LoadScene because serialization writes the model GUID/path.
	// szPath must point to static storage — same lifetime contract as every
	// other const char* automation arg.
void AddStep_LoadModel(const char* szPath);

	// Override the material at slot iIndex on the selected entity's loaded
	// ModelInstance. Apply AFTER AddStep_LoadModel so the slot exists.
void AddStep_SetModelMaterial(int iIndex, Zenith_MaterialAsset* pxMaterial);

	//--------------------------------------------------------------------------
	// Terrain Step Helpers
	//--------------------------------------------------------------------------
	// Set one of the four terrain material slots on the selected entity's
	// Zenith_TerrainComponent. Slot must be in [0, 4).
void AddStep_SetTerrainMaterial(int iSlot, Zenith_MaterialAsset* pxMaterial);

	// Set the splatmap texture path on the selected entity's Zenith_TerrainComponent.
	// szPath must point to static storage.
void AddStep_SetTerrainSplatmapPath(const char* szPath);

	//--------------------------------------------------------------------------
	// Prefab Variant Step Helpers
	//
	// All path arguments must point to static storage (string literals or static
	// const arrays) — same lifetime contract as every other AddStep_* string.
	// These four steps cover the full variant authoring loop:
	//
	//   1. AddStep_CreatePrefabFromSelected — capture selected entity to .zpfb
	//   2. AddStep_CreatePrefabVariant       — derive a variant from a base path
	//   3. AddStep_AddPrefabVariantOverrideVec3 — append a Vector3 override
	//   4. AddStep_InstantiatePrefab         — load + instantiate into scene
	//
	// Steps 1-3 read/write through the asset registry, so saves/loads stay
	// consistent with editor and runtime code paths.
	//--------------------------------------------------------------------------

	// Capture the currently-selected entity into a prefab and save it to disk.
	// The prefab's logical name (used by Instantiate when no override is given)
	// is szPrefabName; the file is written to szSavePath.
void AddStep_CreatePrefabFromSelected(const char* szPrefabName, const char* szSavePath);

	// Create a new variant prefab inheriting from szBasePath and save it to
	// szSavePath. The base must already exist on disk (typically created by a
	// preceding AddStep_CreatePrefabFromSelected). Variant authoring failures
	// (cycle detection, missing base) assert.
void AddStep_CreatePrefabVariant(
		const char* szVariantName,
		const char* szBasePath,
		const char* szSavePath);

	// Append a Vector3 property override to the variant prefab at szPrefabPath
	// and save the file back to disk. Reuses Zenith_ComponentMetaRegistry's
	// flat-name property reflection — see ComponentMeta.h for the supported
	// property names (currently "Position", "Rotation", "Scale" on Transform,
	// "Color"/"Intensity"/etc. on Light, and so on).
void AddStep_AddPrefabVariantOverrideVec3(
		const char* szPrefabPath,
		const char* szComponentName,
		const char* szPropertyName,
		float fX, float fY, float fZ);

	// Load the prefab at szPrefabPath through the asset registry and instantiate
	// it into the active scene. The new entity is selected so subsequent
	// transform/component steps target it. Pass an empty entity name to fall
	// back to the prefab's own name.
void AddStep_InstantiatePrefab(const char* szPrefabPath, const char* szEntityName);

	//--------------------------------------------------------------------------
	// Scene Loading Step Helpers
	//--------------------------------------------------------------------------

	// Combined initial-scene-load step. Registers pfnCallback as the
	// initial-scene-load callback (used by editor restart), then invokes the
	// callback under a lifecycle-deferral guard so entity creation during the
	// load defers OnAwake/OnEnable until DispatchFullLifecycleInit fires.
	// Replaces the SetInitialSceneLoadCallback + SetLoadingScene(true) +
	// Custom + SetLoadingScene(false) sequence.
void AddStep_LoadInitialScene(void (*pfnCallback)());

	//--------------------------------------------------------------------------
	// Custom Step (for game-specific operations)
	//--------------------------------------------------------------------------
void AddStep_Custom(void (*pfnFunc)());

public:
	// ===== Data members (was Zenith_EditorAutomation) =====
	Zenith_Vector<Zenith_EditorAction> m_axActions;
	uint32_t                           m_uCurrentAction = 0;
	bool                               m_bRunning       = false;
	bool                               m_bComplete      = false;

private:
	void ExecuteAction(const Zenith_EditorAction& xAction);
};

#endif // ZENITH_TOOLS
