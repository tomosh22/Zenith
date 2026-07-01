#pragma once

#ifdef ZENITH_TOOLS

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"   // Matrix4 / Quat return types on the euler-authoring helpers

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
// LoadInitialScene) call Zenith_SceneSystem (g_xEngine.Scenes()) directly.
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

	// Bone attachment: add a Zenith_AttachmentComponent to the SELECTED entity and
	// bind it to a named bone of another entity (resolved by name in the same scene).
	ATTACH_TO_BONE,

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
	SET_TRANSFORM_ROTATION,         // full XYZ euler (degrees); composes Ry * Rx * Rz

	// Light field edits
	SET_LIGHT_INTENSITY,
	SET_LIGHT_RANGE,
	SET_LIGHT_COLOR,

	// UI element creation and field edits. The whole UI range, from
	// CREATE_UI_TEXT through SET_UI_SCROLL_VIEW_CONTENT_SIZE below, must stay
	// CONTIGUOUS (ExecuteAction routes the range to ExecuteUIAction).
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
	SET_UI_SCROLL_VIEW_CONTENT_SIZE,	// END of the contiguous UI range (see CREATE_UI_TEXT)

	// Behaviour Graph (via Zenith_Editor::AttachGraphToSelected)
	ATTACH_GRAPH,

	// Material editor authoring (via Zenith_MaterialEditorPanel's atomic editor
	// actions - each step performs the exact operation a human's UI gesture
	// runs: create/open a .zmtrl, set a parameter row, swap a texture slot,
	// pick a parent, toggle an override, switch the preview mesh/light, Save).
	// This block must stay CONTIGUOUS (ExecuteAction routes the whole range to
	// ExecuteMaterialAction).
	MATERIAL_CREATE,
	MATERIAL_OPEN,
	MATERIAL_SET_PARAM_FLOAT,
	MATERIAL_SET_PARAM_COLOR,
	MATERIAL_SET_PARAM_INT,
	MATERIAL_SET_TEXTURE,
	MATERIAL_SET_PARENT,
	MATERIAL_SET_OVERRIDE,
	MATERIAL_SET_PREVIEW_MESH,
	MATERIAL_SET_PREVIEW_LIGHT,
	MATERIAL_SAVE,	// END of the contiguous MATERIAL range (see MATERIAL_CREATE)

	// Behaviour Graph authoring (via Zenith_GraphEditorPanel's atomic editor
	// actions - each step performs the exact operation a human's UI gesture
	// runs: open the editor, click a palette entry, drag a pin connection,
	// click-select a node, edit a property row, add a variable, Save, close).
	GRAPH_OPEN_FRESH,
	GRAPH_ADD_NODE,
	GRAPH_SELECT_NODE,
	GRAPH_SET_NODE_PARAM_FLOAT,
	GRAPH_SET_NODE_PARAM_STRING,
	GRAPH_SET_NODE_PARAM_VEC3,
	GRAPH_SET_NODE_PARAM_INT,
	GRAPH_CONNECT,
	GRAPH_ADD_VARIABLE,
	GRAPH_SAVE,
	GRAPH_CLOSE,

	// Particles
	SET_PARTICLE_CONFIG,
	SET_PARTICLE_CONFIG_BY_NAME,
	SET_PARTICLE_EMITTING,

	// Collider
	ADD_COLLIDER_SHAPE,
	ADD_CAPSULE_COLLIDER,

	// Model
	ADD_MESH_ENTRY,
	LOAD_MODEL,
	SET_MODEL_MATERIAL,

	// Terrain
	SET_TERRAIN_MATERIAL,
	SET_TERRAIN_SPLATMAP_PATH,

	// Terrain-editor authoring (Zenith_TerrainEditor). All operate on the
	// engine terrain editor's CPU images + disk — opening a standalone
	// (component-less) session on demand, so they can run BEFORE any terrain
	// entity exists and are headless-safe. NOTE: this block must stay
	// CONTIGUOUS (ExecuteAction routes the whole range to a sub-executor).
	TERRAIN_EDITOR_RESET,
	TERRAIN_EDITOR_GENERATE_PROCEDURAL,
	TERRAIN_EDITOR_BRUSH_STROKE,
	TERRAIN_EDITOR_SAMPLE_STAMP,
	TERRAIN_EDITOR_AUTO_SPLAT_RULE,
	TERRAIN_EDITOR_RUN_AUTO_SPLAT,
	TERRAIN_EDITOR_ERODE,
	TERRAIN_EDITOR_SET_TREE_BRUSH,
	TERRAIN_EDITOR_SAVE_TEXTURES,
	TERRAIN_EDITOR_EXPORT_CHUNKS,

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
	// Up to 10 floats: most steps use <=4; INSTANTIATE_PREFAB packs a full
	// transform here as pos[0..2], quat[3..6] (wxyz), scale[7..9].
	float m_afArgs[10] = {};
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
void AddStep_AddParticleEmitter() { AddStep_AddComponent("ParticleEmitter"); }
void AddStep_AddCollider() { AddStep_AddComponent("Collider"); }
void AddStep_AddModel() { AddStep_AddComponent("Model"); }
void AddStep_AddAnimator() { AddStep_AddComponent("Animator"); }

	// Add a Zenith_AttachmentComponent to the SELECTED entity and bind it to szBone
	// of szTargetEntityName (resolved by name within the selected entity's scene).
	// The mount offset is built from the position + XYZ euler (degrees) exactly like
	// RT_BuildJetpackMount: M = T(pos) * Ry(eulerY) * Rx(eulerX) * Rz(eulerZ). Author
	// the target entity BEFORE this step so the name resolves.
void AddStep_AttachToBone(const char* szTargetEntityName, const char* szBone,
	float fPosX, float fPosY, float fPosZ,
	float fEulerXDeg, float fEulerYDeg, float fEulerZDeg);

	// Pure authoring-math helpers (also used by ATTACH_TO_BONE / SET_TRANSFORM_ROTATION
	// executors). Composition order matches RT_BuildJetpackMount: rotation = Ry * Rx * Rz
	// (degrees); the offset matrix is T(pos) * that rotation. Exposed static so unit
	// tests can assert the composition order directly.
	static Zenith_Maths::Quat    BuildEulerRotation(float fEulerXDeg, float fEulerYDeg, float fEulerZDeg);
	static Zenith_Maths::Matrix4 BuildEulerOffsetMatrix(float fPosX, float fPosY, float fPosZ,
		float fEulerXDeg, float fEulerYDeg, float fEulerZDeg);

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

	// Full XYZ rotation (degrees). Composes Ry(eulerY) * Rx(eulerX) * Rz(eulerZ) — the
	// rotation half of the AttachToBone mount convention. Use for the guns' 90deg Z
	// rest pose where yaw-only is insufficient.
void AddStep_SetTransformRotationEuler(float fEulerXDeg, float fEulerYDeg, float fEulerZDeg);

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
	// Graph Step Helpers
	//--------------------------------------------------------------------------
	// Attaches a Behaviour Graph (.bgraph asset path, e.g. "game:Graphs/Door.bgraph")
	// to the selected entity's Zenith_GraphComponent (added if absent).
void AddStep_AttachGraph(const char* szGraphAssetPath);

	// Graph AUTHORING steps - boot-time .bgraph creation through the graph
	// editor's atomic actions (the same operations a human's UI gestures run).
	// Nodes are addressed by type name + occurrence in creation order. The
	// authoring sequence for one graph is:
	//   GraphOpenFresh -> GraphAddNode... -> GraphSelectNode + param edits...
	//   -> GraphConnect... -> GraphAddVariable... -> GraphSave -> GraphClose.
	// GraphOpenFresh resets the definition so each boot re-authors the asset
	// from scratch (the graph analogue of scene authoring overwriting scenes).
void AddStep_GraphOpenFresh(const char* szAssetPath);
void AddStep_GraphAddNode(const char* szTypeName);
void AddStep_GraphSelectNode(const char* szTypeName, int iOccurrence);
void AddStep_GraphSetNodeParamFloat(const char* szPropertyName, float fValue);
void AddStep_GraphSetNodeParamString(const char* szPropertyName, const char* szValue);
void AddStep_GraphSetNodeParamVec3(const char* szPropertyName, float fX, float fY, float fZ);
void AddStep_GraphSetNodeParamInt(const char* szPropertyName, int iValue);
void AddStep_GraphConnect(const char* szSrcTypeName, int iSrcOccurrence, int iSrcPin, const char* szDstTypeName, int iDstOccurrence);
void AddStep_GraphAddVariable(const char* szName, const char* szTypeName, float fDefaultNumeric);
void AddStep_GraphSave();
void AddStep_GraphClose();

	//--------------------------------------------------------------------------
	// Material editor step helpers (drive Zenith_MaterialEditorPanel's atomic
	// Action_* verbs — the same operations a human editing a material runs).
	// Parameter / texture-slot names are the stable strings from the
	// Zenith_MaterialParamTable reflection table ("Roughness", "BaseColor",
	// "Normal", ...). A typical authoring sequence:
	//   MaterialCreate("game:Materials/foo.zmtrl") ->
	//   MaterialSetParamFloat("Roughness", 0.2f) ->
	//   MaterialSetTexture("BaseColor", "game:Textures/albedo.ztxtr") ->
	//   MaterialSave().
	//--------------------------------------------------------------------------
void AddStep_MaterialCreate(const char* szAssetPath);
void AddStep_MaterialOpen(const char* szAssetPath);
void AddStep_MaterialSetParamFloat(const char* szParamName, float fValue);
void AddStep_MaterialSetParamColor(const char* szParamName, float fR, float fG, float fB, float fA);
void AddStep_MaterialSetParamInt(const char* szParamName, int iValue);
void AddStep_MaterialSetTexture(const char* szSlotName, const char* szTexturePath);
void AddStep_MaterialSetParent(const char* szParentAssetPath);	// nullptr/"" clears the parent
void AddStep_MaterialSetOverride(const char* szParamName, bool bOverridden);
void AddStep_MaterialSetPreviewMesh(int iMesh);
void AddStep_MaterialSetPreviewLight(float fYaw, float fPitch);
void AddStep_MaterialSave(const char* szAssetPath);	// nullptr/"" saves to the current path

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

	// Add a CAPSULE collider with EXPLICIT dimensions (radius + cylinder half-height,
	// metres) to the selected entity's ColliderComponent. Unlike AddColliderShape's
	// scale-derived capsule (which degenerates to a sphere under a uniform scale), the
	// explicit form fits a capsule MESH exactly — for a unit-capsule mesh (radius 0.5,
	// total height 2.0) scaled uniformly by s, pass fRadius = fHalfHeight = 0.5*s.
	void AddStep_AddCapsuleCollider(float fRadius, float fHalfHeight, int iBodyType);

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
	// Terrain-Editor Authoring Step Helpers
	//
	// Drive the engine terrain editor (Zenith_TerrainEditor) — the same code
	// path the Terrain Editor panel uses. A standalone (component-less)
	// session is opened on demand, so these can run before the terrain entity
	// exists; they touch only CPU images + disk (headless-safe). Determinism:
	// fixed seeds + integer-hash noise => byte-identical outputs per run.
	//--------------------------------------------------------------------------
	// Reset the session's CPU maps to defaults. From-scratch recipes run this
	// FIRST so regeneration is byte-identical even when a previous bake's
	// textures exist on disk (the session seeds from them on open).
void AddStep_TerrainResetSession();

	// Whole-field seeded procedural generation (FBM/ridged blend).
void AddStep_TerrainGenerateProcedural(int iSeed, float fBaseHeight, float fAmplitude,
	float fFrequency, int iOctaves, float fLacunarity, float fGain, float fRidgedBlend);

	// One brush dab. iTool casts to Zenith_TerrainBrushTool; fToolValue is the
	// tool's parameter (target height for Flatten/SetHeight, displacement for
	// Noise, step for Terrace, splat layer for SplatPaint, density for
	// GrassDensity).
void AddStep_TerrainBrushStroke(int iTool, float fWorldX, float fWorldZ,
	float fRadius, float fStrength, float fToolValue);

	// Capture the copy/stamp buffer from a heightfield disc (the Stamp tool
	// then stamps it via AddStep_TerrainBrushStroke).
void AddStep_TerrainSampleStamp(float fWorldX, float fWorldZ, float fRadius);

	// Configure one auto-splat slope/height rule slot, then run the classifier.
void AddStep_TerrainAutoSplatRule(int iSlot, float fHeightMin, float fHeightMax,
	float fSlopeMinDeg, float fSlopeMaxDeg, float fWeight, float fJitter);
void AddStep_TerrainRunAutoSplat();

	// Synchronous hydraulic + thermal erosion.
void AddStep_TerrainErode(int iHydraulicDroplets, int iThermalIterations, int iSeed);

	// Configure the TreePaint brush before tree dabs: attempts-per-dab (scaled by
	// stroke strength), uniform scale range, minimum trunk spacing (m), and the
	// max slope (deg) trees will sit on. iSeed re-seeds the scatter RNG so a
	// re-authored scene paints byte-identically (0 => fixed default seed).
void AddStep_TerrainSetTreeBrush(int iTreesPerDab, float fScaleMin, float fScaleMax,
	float fSpacing, float fMaxSlopeDeg, int iSeed);

	// Persist Height/Splatmap_RGBA/GrassDensity .ztxtr to the game assets dir.
void AddStep_TerrainSaveTextures();

	// Export every terrain chunk mesh from the live heightfield
	// (ExportHeightmapFromMat into <GAME_ASSETS>/Terrain/). Takes minutes.
void AddStep_TerrainExportChunks();

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
	// it into the active scene at the given transform. The new entity is selected
	// so subsequent transform/component steps target it. Pass an empty entity name
	// to fall back to the prefab's own name. Transform defaults to origin /
	// identity / (1,1,1); rotation is a quaternion in wxyz order.
void AddStep_InstantiatePrefab(const char* szPrefabPath, const char* szEntityName,
		float fPosX = 0.0f, float fPosY = 0.0f, float fPosZ = 0.0f,
		float fRotW = 1.0f, float fRotX = 0.0f, float fRotY = 0.0f, float fRotZ = 0.0f,
		float fScaleX = 1.0f, float fScaleY = 1.0f, float fScaleZ = 1.0f);

	//--------------------------------------------------------------------------
	// Scene Loading Step Helpers
	//--------------------------------------------------------------------------

	// Initial-scene-load step. Invokes pfnCallback under a lifecycle-deferral
	// guard so entity creation during the load defers OnAwake/OnEnable until
	// DispatchFullLifecycleInit fires.
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
