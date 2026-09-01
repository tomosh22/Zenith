#pragma once

#ifdef ZENITH_TOOLS

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"   // Matrix4 / Quat return types on the euler-authoring helpers
#include <string>

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
class Zenith_TerrainEditor;
class Zenith_UnitTests;
class Zenith_Profiling;

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
	SET_TRANSFORM_ROTATION_QUAT,    // verbatim quaternion; the ONLY bit-exact rotation step

	// Light field edits
	SET_LIGHT_INTENSITY,
	SET_LIGHT_RANGE,
	SET_LIGHT_COLOR,
	SET_LIGHT_POSITION_OFFSET,

	// Sun-authority field edits (geometry only; there is intentionally no
	// colour/intensity action).
	SET_SUN_DIRECTION,
	SET_SUN_TIME_OF_DAY,

	// UI element creation and field edits. The whole UI range, from
	// CREATE_UI_TEXT through SET_UI_VIRTUAL_BUTTON_HIT_SLOP below, must stay
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
	SET_UI_SCROLL_VIEW_CONTENT_SIZE,

	// UI on-screen controls (B9). A NEW UI ACTION GOES HERE, at the END of the
	// block: the router compares against the block's FIRST and LAST member, so
	// appending is free while inserting anywhere else silently routes the new
	// action into a neighbouring executor's default: assert.
	CREATE_UI_VIRTUAL_STICK,
	SET_UI_VIRTUAL_STICK_ACTION,
	SET_UI_VIRTUAL_STICK_MODE,
	SET_UI_VIRTUAL_STICK_RADIUS,
	SET_UI_VIRTUAL_STICK_DEADZONE,
	SET_UI_VIRTUAL_STICK_ACTIVATION_SLOP,
	CREATE_UI_VIRTUAL_BUTTON,
	SET_UI_VIRTUAL_BUTTON_ACTION,
	SET_UI_VIRTUAL_BUTTON_HIT_SLOP,	// END of the contiguous UI range (see CREATE_UI_TEXT)

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
	GRAPH_SET_NODE_PARAM_BOOL,
	GRAPH_CONNECT,
	GRAPH_ADD_VARIABLE,
	GRAPH_SAVE,
	GRAPH_CLOSE,
	// Programmatic authoring: builds a whole .bgraph through Zenith_GraphBuilder
	// (the conversion program's bulk path - no simulated editor clicks), saves it
	// through the asset registry, and queues hot reload. The click-step verbs
	// above stay for editor-coverage tests.
	GRAPH_BUILD,

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
	TERRAIN_EDITOR_SET_ASSET_SET,
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
	TERRAIN_EDITOR_EXPORT_CHUNKS_RECT,
	// APPENDED AT THE END of the terrain block on purpose: the block is routed by
	// a pair of range comparisons against its FIRST and LAST member, so a new
	// action inserted in the middle silently joins the block while one appended
	// here requires the comparison below to move with it.
	TERRAIN_EDITOR_SET_DIMENSIONS,

	// Grass-type authoring (Zenith_TerrainEditor's WORKING copy of the
	// Flux_GrassTypeTable — the same object the terrain editor panel edits).
	// Parameters are addressed BY NAME through Flux_GrassTypeParams' one
	// name->field mapping, exactly as the material verbs address the material
	// param table. Every step is CPU + disk only, so the family is headless-safe.
	// NOTE: this block must stay CONTIGUOUS — ranges are compared, never
	// numbered (ExecuteAction routes the whole range to ExecuteGrassTypeAction).
	GRASS_TYPES_CREATE,
	GRASS_TYPES_SET_COUNT,
	GRASS_TYPES_SET_NAME,
	GRASS_TYPES_SET_PARAM_FLOAT,
	GRASS_TYPES_SET_PARAM_COLOR,
	GRASS_TYPES_SAVE,	// END of the contiguous GRASS_TYPES range (see GRASS_TYPES_CREATE)

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

	// NavMesh. Deliberately NOT appended to the Terrain block above, which is
	// routed by a range comparison: a standalone action sits outside every
	// range and reaches ExecuteAction's own switch, which is what a
	// single-verb family wants.
	SET_NAVMESH_ASSET,

	// Scene loading
	LOAD_INITIAL_SCENE,                 // Combined: registers the initial-scene-load callback,
	                                    // then invokes it once under a lifecycle-deferral guard.
	                                    // Replaces the SET_LOADING_SCENE(true) + CUSTOM +
	                                    // SET_LOADING_SCENE(false) triplet plus the separate
	                                    // SET_INITIAL_SCENE_LOAD_CALLBACK step.

	// Custom step (game-specific logic as function pointer)
	CUSTOM_STEP,
};

// ExecuteAction routes SUB-RANGES to sub-executors with a pair of `>=` / `<=`
// comparisons against a block's first and last member — so a value inserted
// into the middle of a block is free, and one inserted BETWEEN two members of
// the same block silently joins it. Nothing else in this enum is pinned, so
// this pins the youngest block: GRASS_TYPES_SAVE must stay exactly five past
// GRASS_TYPES_CREATE, which fails the build the moment a new action type is
// added inside the range instead of after it.
static_assert(static_cast<int>(Zenith_EditorActionType::GRASS_TYPES_SAVE) -
	static_cast<int>(Zenith_EditorActionType::GRASS_TYPES_CREATE) == 5,
	"the GRASS_TYPES block must stay CONTIGUOUS and six wide — ExecuteAction routes it by range");
// The same pin for the TERRAIN_EDITOR block, whose last member moved when
// SET_DIMENSIONS was appended: TryRouteTerrainEditorAction compares against
// SET_DIMENSIONS as the upper bound, so an action added after it that forgets to
// move that comparison would never be routed at all -- it would fall through to
// the generic executor and assert at runtime rather than at build time.
static_assert(static_cast<int>(Zenith_EditorActionType::TERRAIN_EDITOR_SET_DIMENSIONS) -
	static_cast<int>(Zenith_EditorActionType::TERRAIN_EDITOR_SET_ASSET_SET) == 12,
	"the TERRAIN_EDITOR block must stay CONTIGUOUS and thirteen wide — ExecuteAction routes it by range");

//-----------------------------------------------------------------------------
// Action Data
//-----------------------------------------------------------------------------
struct Zenith_EditorAction
{
	Zenith_EditorActionType m_eType = Zenith_EditorActionType::CUSTOM_STEP;
	// Owned copies: AddStep_* callers may pass string-literal pointers OR
	// pointers into transient storage (e.g. a stack buffer built in a loop) —
	// the action queue is only drained much later (boot-time), so the struct
	// must not merely alias caller-owned memory. Zenith_Vector<T> move/copy
	// constructs elements via placement-new (see Zenith_Vector::Reserve), so
	// std::string members here relocate safely across queue growth.
	// m_szArg3/4/5 exist only for the handful of action types needing more
	// than two strings (SET_UI_NAVIGATION's up/down/left/right, prefab-variant
	// save path / property name) — most steps leave them empty.
	std::string m_szArg1;
	std::string m_szArg2;
	std::string m_szArg3;
	std::string m_szArg4;
	std::string m_szArg5;
	// Up to 10 floats: most steps use <=4; INSTANTIATE_PREFAB packs a full
	// transform here as pos[0..2], quat[3..6] (wxyz), scale[7..9].
	float m_afArgs[10] = {};
	// Up to four signed integers; bounded terrain export owns all four bounds.
	int m_aiArgs[4] = {};
	bool m_bArg = false;
	void* m_pArg = nullptr;   // Type determined by m_eType (e.g. Flux_ParticleEmitterConfig*, Flux_MeshGeometry*)
	void* m_pArg2 = nullptr;  // Type determined by m_eType (e.g. Zenith_MaterialAsset*)
	void (*m_pfnFunc)() = nullptr;
	void (*m_pfnGraphBuild)(class Zenith_GraphBuilder&) = nullptr;	// GRAPH_BUILD only
	// Optional human name for the boot/tail attribution tables. Empty means "unnamed",
	// which reports as the step's index + action-type id. Only worth setting on steps
	// heavy enough to matter (a bake, a big export) — the whole point is to make a
	// long pole legible without naming two hundred trivial field edits.
	std::string m_szStepName;
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
	// bProductionTail marks THE ONE session that drains the game's real authoring
	// queue (Zenith_Engine::InitialiseProject). Everything attribution-related — per
	// step timing, slow-step logs, the completion summary, the AutomationQueueDrained
	// milestone, the .tail.txt artifact — is gated on it, because unit tests drive
	// this same global object during boot and their queues complete BEFORE the
	// production queue is even registered. Without the gate the tail report would be
	// a mix of test fixtures and real work.
	//
	// pxProfiling is INJECTED rather than reached via g_xEngine so this TU gains no
	// new engine-singleton reference. Null is fine: milestones are simply skipped.
void Begin(bool bProductionTail = false, Zenith_Profiling* pxProfiling = nullptr);
bool IsRunning();
bool IsComplete();
void ExecuteNextStep();
void Reset();

	// Per-step wall-clock, in execution order, for the production session only.
	struct StepTiming
	{
		std::string m_strName;
		double      m_fMilliseconds = 0.0;
		u_int       m_uIndex = 0;
	};

	// Steps executed SO FAR, oldest first (production session only; empty otherwise).
	const Zenith_Vector<StepTiming>& GetStepTimings() const { return m_xStepTimings; }

	// Attribution tables. WriteStepsSoFar is what the boot-profile dump embeds while
	// the queue is still draining; WriteTailReport is the completion artifact.
	void WriteStepsSoFar(FILE* pxFile) const;
	void WriteTailReport(FILE* pxFile) const;

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

	// ★ THE ONLY BIT-EXACT ROTATION STEP. The two steps above BUILD a quaternion
	// at authoring time -- yaw runs glm::angleAxis (sin/cos of the half angle),
	// euler runs BuildEulerRotation -- and those are libm calls whose results
	// differ by 1-2 ULP between build configurations. An entity authored through
	// them therefore serializes DIFFERENT BYTES from a Debug and a Release tools
	// build, so a tracked .zscen ping-pongs between two values in git forever.
	// That is not hypothetical: it is the defect ZM-D-183 fixed for Zenithmon's
	// Npc_RivalVesper (Games/Zenithmon/Docs/DecisionLog.md).
	//
	// This step performs NO MATH -- the components go straight to
	// Zenith_TransformComponent::SetRotation, which stores them verbatim (it does
	// not normalize). Use it for EVERY authored entity whose rotation lands in a
	// COMMITTED scene file; the yaw/euler steps remain fine for a transient or
	// gitignored one, where a 1-ULP difference has nowhere to show up.
	//
	// Argument order is x, y, z, w -- the SERIALIZED order (Zenith_DataStream
	// writes the quaternion in that order), deliberately NOT glm::quat's (w,x,y,z)
	// constructor order, so a caller freezing bytes read out of a .zscen types
	// them in the order they appear in the file.
void AddStep_SetTransformRotationQuat(float fX, float fY, float fZ, float fW);

	// Light component field edits. Apply to the selected entity's
	// Zenith_LightComponent — set after AddStep_AddComponent("Light").
void AddStep_SetLightIntensity(float fLumens);
void AddStep_SetLightRange(float fMetres);
void AddStep_SetLightColor(float fR, float fG, float fB);
	// ★ THE OFFSET IS IN THE MODEL'S LOCAL SPACE, not world -- it is scaled and
	// rotated by the entity's own transform before it is applied
	// (Zenith_LightComponent::GetWorldPosition). That is what lets a light share
	// an entity with a MODEL and sit at a named point ON it: a bulb inside a lamp
	// post's lantern head, measured off the mesh once and still correct after the
	// prop fit rescales the asset or the placement turns it.
	//
	// Setting it also ENABLES the offset -- an offset authored and left switched
	// off is a light silently at its entity origin, which is the failure this
	// verb exists to make impossible to author by halves.
void AddStep_SetLightPositionOffset(float fX, float fY, float fZ);

	// Sun component field edits. Apply after AddStep_AddComponent("Sun").
void AddStep_SetSunDirection(float fX, float fY, float fZ);
void AddStep_SetSunTimeOfDay(float fAngleDegrees, float fOrbitAzimuthDegrees);

	// NavMesh component field edit. Apply after AddStep_AddComponent("NavMesh").
	//
	// ★ THIS WAS THE MISSING HALF OF AUTHORED NAVMESHES. Zenith_NavMeshComponent
	// documents an "AUTHORED scenes" recipe -- add the component, set its ref,
	// the ref serializes -- but there was no automation verb for the second
	// step, so a recipe could add the component and never populate it. The ref
	// goes through Zenith_NavMeshComponent::SetAssetRef, which resolves a
	// `game:`/`engine:` prefix through Zenith_AssetRegistry and LOADS
	// IMMEDIATELY (so a following step can already query the mesh).
void AddStep_SetNavMeshAsset(const char* szAssetRef);

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
	// UI On-Screen Control Step Helpers (B9)
	//--------------------------------------------------------------------------
	// The action NAME is what a control targets; the named action's own VIRTUAL
	// binding row decides which source id it publishes to, so a rebind stays a
	// binding-table edit. Radius / deadzone / slop are LOGICAL pixels and
	// fractions — the display scale is applied at use time, never at authoring
	// time, or the authored scene would bake one panel's density into itself.
void AddStep_CreateUIVirtualStick(const char* szName);
void AddStep_SetUIVirtualStickAction(const char* szElement, const char* szActionName);
	// iMode: 0 = FIXED (base pinned to the rect centre), 1 = FLOATING (base
	// recentres at the down position).
void AddStep_SetUIVirtualStickMode(const char* szElement, int iMode);
void AddStep_SetUIVirtualStickRadius(const char* szElement, float fLogicalPx);
void AddStep_SetUIVirtualStickDeadzone(const char* szElement, float fFraction);
void AddStep_SetUIVirtualStickActivationSlop(const char* szElement, float fLogicalPx);

void AddStep_CreateUIVirtualButton(const char* szName);
void AddStep_SetUIVirtualButtonAction(const char* szElement, const char* szActionName);
void AddStep_SetUIVirtualButtonHitSlop(const char* szElement, float fLogicalPx);

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
void AddStep_GraphSetNodeParamBool(const char* szPropertyName, bool bValue);
void AddStep_GraphConnect(const char* szSrcTypeName, int iSrcOccurrence, int iSrcPin, const char* szDstTypeName, int iDstOccurrence);
void AddStep_GraphAddVariable(const char* szName, const char* szTypeName, float fDefaultNumeric);
void AddStep_GraphSave();
void AddStep_GraphClose();

	// Programmatic graph authoring: pfnBuild receives a Zenith_GraphBuilder over
	// a fresh definition; the step Build()s it (asserting on authoring errors),
	// saves the asset to szAssetPath, and queues Zenith_GraphReload. This is the
	// bulk boot-authoring path for the behaviour-graph conversion program.
void AddStep_GraphBuild(const char* szAssetPath, void (*pfnBuild)(class Zenith_GraphBuilder&));

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
	// Grass-type step helpers (drive Zenith_TerrainEditor's WORKING copy of the
	// Flux_GrassTypeTable — the same object the terrain editor panel edits, so
	// an authored recipe and a human produce identical tables).
	//
	// Parameter names are the stable strings from Flux_GrassTypeParams' one
	// name->field mapping ("HeightMax", "Density", "WindResponse", ...); colour
	// names are "BaseColour" / "TipColour". An unknown name asserts at boot
	// rather than silently no-op'ing. A typical authoring sequence:
	//   GrassTypesCreate() -> GrassTypesSetCount(5) ->
	//   GrassTypesSetName(4, "Reeds") ->
	//   GrassTypesSetParamFloat(4, "HeightMax", 2.0f) ->
	//   GrassTypesSetParamColor(4, "BaseColour", 0.1f, 0.3f, 0.05f) ->
	//   GrassTypesSave().
	//--------------------------------------------------------------------------
void AddStep_GrassTypesCreate();	// working copy = the four built-in types
void AddStep_GrassTypesSetCount(int iCount);	// clamped to [1, uFLUX_GRASS_MAX_TYPES]
void AddStep_GrassTypesSetName(int iType, const char* szName);
void AddStep_GrassTypesSetParamFloat(int iType, const char* szParam, float fValue);
void AddStep_GrassTypesSetParamColor(int iType, const char* szParam, float fR, float fG, float fB);
void AddStep_GrassTypesSave();	// writes game:Vegetation/GrassTypes.zdata, then applies

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
	// Select the validated terrain set used by subsequent texture/chunk
	// persistence. The queued action owns a copy of szSet; empty selects legacy.
	// A selected fresh Terrain component is stamped for a following SaveScene;
	// an initialized component with a different set must use BakeFull instead.
void AddStep_TerrainSetAssetSet(const char* szSet);

	// Stage the terrain's DIMENSIONS on the editor session, and stamp them onto
	// a selected FRESH terrain component so a following SaveScene persists them.
	// Refuses an already-initialised terrain for the same reason
	// AddStep_TerrainSetAssetSet does: its chunks were decoded against the
	// current quantisation box, and moving the box would silently relocate them.
	//
	// fVertexSpacingMetres is metres BETWEEN VERTICES; it is converted to the
	// stored quads-per-chunk-edge, which must be a power of two in [4, 256].
void AddStep_TerrainSetDimensions(float fChunkSizeMetres, float fVertexSpacingMetres,
	int iGridChunksX, int iGridChunksZ);

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

	// Persist Height/Splatmap_RGBA/GrassDensity .ztxtr to the selected terrain
	// set (or the legacy Textures/Terrain/ path when the set is empty).
void AddStep_TerrainSaveTextures();

	// Export every terrain chunk mesh from the live heightfield
	// into the selected terrain set. Takes minutes.
void AddStep_TerrainExportChunks();

	// Export only the inclusive chunk rectangle. Bounds are validated when the
	// action executes, before a standalone editor session is opened. Every valid
	// rectangle includes the hard-required anchor chunk (0, 0).
void AddStep_TerrainExportChunksRect(int iMinX, int iMinY, int iMaxX, int iMaxY);

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
	// Named overload: the step shows up under szStepName in the tail attribution
	// tables instead of a bare index. Use it for the steps that are actually
	// expensive — a navmesh bake, a terrain export — so a long pole is legible.
void AddStep_Custom(void (*pfnFunc)(), const char* szStepName);

public:
	// ===== Data members (was Zenith_EditorAutomation) =====
	Zenith_Vector<Zenith_EditorAction> m_axActions;
	uint32_t                           m_uCurrentAction = 0;
	bool                               m_bRunning       = false;
	bool                               m_bComplete      = false;

	// ===== Production-tail attribution (see Begin) =====
	// Bounded like every other capture in this system: a runaway queue truncates and
	// SAYS SO rather than silently reporting a partial table as complete.
	static constexpr u_int uMAX_TRACKED_STEPS = 4096;
	bool                      m_bProductionTail   = false;
	Zenith_Profiling*         m_pxProfiling       = nullptr;
	Zenith_Vector<StepTiming> m_xStepTimings;
	double                    m_fTotalStepMs      = 0.0;
	u_int                     m_uUntrackedSteps   = 0;   // executed past uMAX_TRACKED_STEPS

private:
	friend class Zenith_UnitTests;
	// Shared completion path for both places ExecuteNextStep can drain the queue.
	void FinishSession();
	void RecordStepTiming(const Zenith_EditorAction& xAction, const u_int uIndex, const double fStepMs);
	static std::string DescribeStep(const Zenith_EditorAction& xAction, const u_int uIndex);
	// Narrow injected seam for rect-export preflight. It traverses the same
	// contiguous range router and executor as production, but stops immediately
	// before the expensive physical export.
	static bool TryPreflightTerrainExportChunksRectAction(
		const Zenith_EditorAction& xAction, Zenith_TerrainEditor& xTerrainEditor);
	void ExecuteAction(const Zenith_EditorAction& xAction);
};

#endif // ZENITH_TOOLS
