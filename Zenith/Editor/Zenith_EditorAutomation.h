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

	// Animation dope-sheet authoring (WU-3.4). Each verb performs EXACTLY the
	// operation one of Zenith_EditorPanel_Animation's Action_* twins performs —
	// the same call the panel's own mouse handler ends in — so an authored
	// recipe and a human's gesture cannot diverge. The two EXPECT_* verbs are
	// ASSERTIONS rather than mutations: they are what makes a recipe fail at the
	// step that is wrong instead of somewhere downstream.
	//
	// ★ THIS BLOCK IS APPENDED AFTER THE PREFAB RANGE ON PURPOSE. Placing it
	// between GRASS_TYPES_SAVE and CREATE_PREFAB_FROM_SELECTED would have moved
	// the prefab block, whose START is pinned by
	// `Automation, GrassTypesEnumBlockIsContiguous`; appending here moves
	// nothing that anything else measures. NOTE: this block must stay
	// CONTIGUOUS (ExecuteAction routes the whole range to ExecuteAnimationAction
	// by a pair of comparisons against its first and last member).
	ANIM_OPEN_CLIP,
	ANIM_SELECT_KEY,
	ANIM_BOX_SELECT,
	ANIM_MOVE_SELECTION,
	ANIM_DELETE_SELECTION,
	ANIM_DUPLICATE_SELECTION,
	ANIM_COPY_SELECTION,
	ANIM_PASTE_TO_BONE,
	ANIM_RIPPLE_RETIME,
	ANIM_SCRUB,
	ANIM_SET_DURATION,
	ANIM_UNDO,
	ANIM_REDO,
	ANIM_CLOSE_CLIP,
	ANIM_EXPECT_KEY_TIME,
	ANIM_EXPECT_SELECTED_COUNT,	// END of the contiguous ANIM range (see ANIM_OPEN_CLIP)

	// Animation POSE authoring (WU-4.3). Its own block rather than four more
	// members of the one above, because appending into that block would move
	// ANIM_EXPECT_SELECTED_COUNT — which is the upper bound BOTH the router's
	// range test and the header's static_assert compare against, and which the
	// `Automation, AnimEnumBlockIsContiguous` unit pins by position. A second
	// contiguous block costs one more range test and moves nothing.
	//
	// NOTE: this block must stay CONTIGUOUS (ExecuteAction routes the whole
	// range to ExecuteAnimationPoseAction by a pair of comparisons against its
	// first and last member).
	ANIM_POSE_SELECT_BONE,
	ANIM_POSE_ROTATE_SELECTED_BONE_WORLD,
	ANIM_POSE_SET_KEY_FOR_SELECTED_BONE,
	ANIM_POSE_SET_AUTO_KEY,
	ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION,	// END of the contiguous ANIM_POSE range (see ANIM_POSE_SELECT_BONE)

	// Animator-controller STATE MACHINE authoring (WU-6.5). A THIRD animation
	// block rather than more members of either above, for the reason the second
	// one exists: appending into ANIM_POSE would move
	// ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION, which is the upper bound BOTH the
	// router's range test and the header's static_assert compare against, and
	// which `Automation, AnimPoseEnumBlockIsContiguous` pins by position.
	//
	// Each verb performs EXACTLY what one of Zenith_EditorPanel_AnimStateMachine's
	// Action_* twins performs — the same call the panel's own mouse handler ends
	// in — so an authored recipe and a human's gesture cannot diverge. The two
	// EXPECT_* verbs are ASSERTIONS rather than mutations: they are what makes a
	// recipe fail at the step that is wrong instead of somewhere downstream.
	//
	// NOTE: this block must stay CONTIGUOUS (ExecuteAction routes the whole range
	// to ExecuteAnimStateMachineAction by a pair of comparisons against its first
	// and last member).
	ANIM_SM_OPEN,
	ANIM_SM_OPEN_FRESH,
	ANIM_SM_CLOSE,
	ANIM_SM_SELECT_LAYER,
	ANIM_SM_ADD_CLIP_PATH,
	ANIM_SM_ADD_STATE,
	ANIM_SM_REMOVE_STATE,
	ANIM_SM_RENAME_STATE,
	ANIM_SM_SET_DEFAULT_STATE,
	ANIM_SM_SET_STATE_CLIP,
	ANIM_SM_ADD_TRANSITION,
	ANIM_SM_REMOVE_TRANSITION,
	ANIM_SM_SET_TRANSITION_DURATION,
	ANIM_SM_SET_TRANSITION_EXIT_TIME,
	ANIM_SM_SET_TRANSITION_INTERRUPTIBLE,
	ANIM_SM_ADD_CONDITION,
	ANIM_SM_REMOVE_CONDITION,
	ANIM_SM_ADD_PARAMETER,
	ANIM_SM_REMOVE_PARAMETER,
	ANIM_SM_UNDO,
	ANIM_SM_REDO,
	ANIM_SM_SAVE,
	ANIM_SM_APPLY,
	ANIM_SM_EXPECT_STATE_COUNT,
	ANIM_SM_EXPECT_DEFAULT_STATE,	// END of the contiguous ANIM_SM range (see ANIM_SM_OPEN)

	// BONE MASK authoring (WU-7.1). A FOURTH animation block, appended for the
	// reason the second and third exist: adding a verb INSIDE the block above
	// would move ANIM_SM_EXPECT_DEFAULT_STATE, which is the upper bound BOTH the
	// router's range test and the header's static_assert compare against, and
	// which `Automation, AnimSmEnumBlockIsContiguous` pins by position. A new
	// contiguous block costs one more range test and moves nothing.
	//
	// Each verb performs EXACTLY what one of Zenith_EditorPanel_Animation's
	// Action_Mask* twins performs — the same call the "Bone Masks" section's own
	// slider handler ends in — so an authored recipe and a human's gesture cannot
	// diverge. ANIM_MASK_EXPECT_WEIGHT is an ASSERTION rather than a mutation: it
	// is what makes a recipe fail at the step that is wrong instead of somewhere
	// downstream.
	//
	// ★ THE SUBTREE VERB NEEDS A PREVIEWED RIG, and that is not a quirk of the
	// automation. A mask names BONES and a SUBTREE needs a HIERARCHY, which only
	// the dope sheet's session skeleton supplies — so a recipe does an
	// AnimOpenClip on a clip whose rig resolves BEFORE any AnimMaskSetSubtree,
	// and the checked wrapper asserts on that step if it did not.
	//
	// NOTE: this block must stay CONTIGUOUS (ExecuteAction routes the whole range
	// to ExecuteAnimMaskAction by a pair of comparisons against its first and
	// last member).
	ANIM_MASK_OPEN,
	ANIM_MASK_OPEN_FRESH,
	ANIM_MASK_CLOSE,
	ANIM_MASK_SET_WEIGHT,
	ANIM_MASK_SET_SUBTREE,
	ANIM_MASK_SET_HAS_AVATAR,
	ANIM_MASK_UNDO,
	ANIM_MASK_REDO,
	ANIM_MASK_SAVE,
	ANIM_MASK_EXPECT_WEIGHT,	// END of the contiguous ANIM_MASK range (see ANIM_MASK_OPEN)

	// ANIMATOR LAYER authoring (WU-7.2). A FIFTH animation block, appended for
	// the reason the second, third and fourth exist: adding a verb INSIDE the
	// block above would move ANIM_MASK_EXPECT_WEIGHT, which is the upper bound
	// BOTH the router's range test and the header's static_assert compare
	// against, and which `Automation, AnimMaskEnumBlockIsContiguous` pins by
	// position. A new contiguous block costs one more range test and moves
	// nothing.
	//
	// Each verb performs EXACTLY what one of
	// Zenith_EditorPanel_AnimStateMachine's layer Action_* twins performs — the
	// same call the "Layers" strip's own handler ends in — so an authored recipe
	// and a human's gesture cannot diverge. ANIM_LAYER_EXPECT_ORDER is an
	// ASSERTION rather than a mutation: the blend ORDER is what this family is
	// about, so it is what a recipe has to be able to state.
	//
	// ★ A LAYER IS ADDRESSED BY ITS STABLE ID, NEVER BY ITS INDEX (D43), and the
	// single index in the family is ANIM_LAYER_MOVE's DESTINATION, which is a
	// position by definition. Ids are minted 0, 1, 2 … by the def's monotonic
	// counter, so a recipe that AnimSmOpenFresh'es and then adds its layers in
	// order knows them; a wrong one fails at BOOT under the checked wrapper
	// rather than editing a different layer in silence.
	//
	// NOTE: this block must stay CONTIGUOUS (ExecuteAction routes the whole range
	// to ExecuteAnimLayerAction by a pair of comparisons against its first and
	// last member).
	ANIM_LAYER_ADD,
	ANIM_LAYER_REMOVE,
	ANIM_LAYER_RENAME,
	ANIM_LAYER_SET_WEIGHT,
	ANIM_LAYER_SET_BLEND_MODE,
	ANIM_LAYER_SET_EMIT_EVENTS,
	ANIM_LAYER_SET_MASK_PATH,
	ANIM_LAYER_MOVE,
	ANIM_LAYER_SELECT,
	ANIM_LAYER_EXPECT_ORDER,	// END of the contiguous ANIM_LAYER range (see ANIM_LAYER_ADD)

	// BLEND-TREE authoring (WU-7.3). A SIXTH animation block, appended for the
	// reason the second through fifth exist: adding a verb INSIDE the block above
	// would move ANIM_LAYER_EXPECT_ORDER, which is the upper bound BOTH the
	// router's range test and the header's static_assert compare against, and
	// which `Automation, AnimLayerEnumBlockIsContiguous` pins by position. A new
	// contiguous block costs one more range test and moves nothing.
	//
	// Each verb performs EXACTLY what one of Zenith_EditorPanel_AnimStateMachine's
	// blend Action_* twins performs — the same call the "Blend Tree" strip's own
	// handler ends in — so an authored recipe and a human's gesture cannot
	// diverge. The two EXPECT_* verbs are ASSERTIONS rather than mutations.
	//
	// ★ A BLEND POINT IS ADDRESSED BY INDEX, AND THAT IS NOT THE D43 MISTAKE. A
	// point has no identity to address it by — it is a struct in a vector and its
	// child is an owned raw pointer with no id — so an index is the only handle
	// there is. What follows from that is that a 1D SET_POINT_POSITION can
	// RENUMBER (the list is kept sorted, because Evaluate blends between ADJACENT
	// points), so a recipe that moves a point past another and then names the old
	// index is naming a different point. Author the positions in ascending order,
	// and state the result with EXPECT_POINT_POSITION.
	//
	// A typical authoring sequence:
	//   AnimSmOpenFresh("game:Anim/Player.zanimctrl") ->
	//   AnimSmAddClipPath(walk) -> AnimSmAddClipPath(run) ->
	//   AnimSmAddParameter("Speed", Float, 0) -> AnimSmAddState("Locomotion") ->
	//   AnimBlendSetTreeKind("Locomotion", 1 /* 1D */) ->
	//   AnimBlendSetParameter("Locomotion", 0 /* X */, "Speed") ->
	//   AnimBlendAddPoint("Locomotion", "Walk", 0, 0) ->
	//   AnimBlendAddPoint("Locomotion", "Run", 4, 0) ->
	//   AnimBlendExpectPointCount("Locomotion", 2) -> AnimSmSave().
	//
	// NOTE: this block must stay CONTIGUOUS (ExecuteAction routes the whole range
	// to ExecuteAnimBlendAction by a pair of comparisons against its first and
	// last member).
	ANIM_BLEND_SET_TREE_KIND,
	ANIM_BLEND_SET_PARAMETER,
	ANIM_BLEND_ADD_POINT,
	ANIM_BLEND_REMOVE_POINT,
	ANIM_BLEND_SET_POINT_CLIP,
	ANIM_BLEND_SET_POINT_POSITION,
	ANIM_BLEND_SELECT_POINT,
	ANIM_BLEND_EXPECT_POINT_COUNT,
	ANIM_BLEND_EXPECT_POINT_POSITION,	// END of the contiguous ANIM_BLEND range (see ANIM_BLEND_SET_TREE_KIND)

	// CURVE-EDITOR authoring (WU-8.2). A SEVENTH animation block, appended for
	// the reason the second through sixth exist: adding a verb INSIDE the block
	// above would move ANIM_BLEND_EXPECT_POINT_POSITION, which is the upper
	// bound BOTH the router's range test and the header's static_assert compare
	// against, and which `Automation, AnimBlendEnumBlockIsContiguous` pins by
	// position. A new contiguous block costs one more range test and moves
	// nothing.
	//
	// Each verb performs EXACTLY what one of Zenith_EditorPanel_Animation's
	// curve Action_* twins performs — the same call the curve view's own pointer
	// handler ends in — so an authored recipe and a human's gesture cannot
	// diverge. ANIM_CURVE_EXPECT_KEY_TANGENT is an ASSERTION rather than a
	// mutation: a tangent is a number nothing else in a recipe can state.
	//
	// ★ A KEY IS NAMED BY (bone, track, INDEX) AND THE EXECUTOR RESOLVES THE
	// STABLE ID, exactly as the ANIM_* block does and for its reason — a recipe
	// is written against a clip a human can see, where "the second key on Hip's
	// rotation track" is the only address that can be typed.
	//
	// ★ AND A ROOT-MOTION TRACK IS REFUSED HERE, WHICH IS NOT A GAP. Root motion
	// carries no tangent array (D17) and is still sampled linearly after WU-8.1,
	// so an empty szBone on one of these steps asserts at BOOT rather than
	// authoring a derivative nothing would ever read.
	//
	// A typical authoring sequence:
	//   AnimOpenClip("game:Animations/Sway.zanim") -> AnimCurveSetView(true) ->
	//   AnimSelectKey("Hip", 0, 1, 0) -> AnimCurveSetSelectionAuto() ->
	//   AnimCurveExpectKeyTangent("Hip", 0, 1, false, 0, 1, 0, 1e-3f).
	//
	// NOTE: this block must stay CONTIGUOUS (ExecuteAction routes the whole
	// range to ExecuteAnimCurveAction by a pair of comparisons against its first
	// and last member).
	ANIM_CURVE_SET_VIEW,
	ANIM_CURVE_SET_UNIFIED,
	ANIM_CURVE_SET_KEY_TANGENTS,
	ANIM_CURVE_SET_SELECTION_AUTO,
	ANIM_CURVE_SET_SELECTION_LINEAR,
	ANIM_CURVE_DRAG_HANDLE_TO_PIXEL,
	ANIM_CURVE_FIT_TO_SELECTION,
	ANIM_CURVE_EXPECT_KEY_TANGENT,	// END of the contiguous ANIM_CURVE range (see ANIM_CURVE_SET_VIEW)

	// TANGENT-MODE authoring (B3). An EIGHTH animation block with its OWN prefix,
	// appended for the reason the second through seventh exist: adding a verb
	// INSIDE the ANIM_CURVE block above would move ANIM_CURVE_EXPECT_KEY_TANGENT,
	// which is the upper bound BOTH the router's range test and the header's
	// static_assert compare against, and which
	// `Automation, AnimCurveEnumBlockIsContiguous` pins by position.
	//
	// ★ ITS OWN PREFIX RATHER THAN MORE ANIM_CURVE_*, because these verbs address
	// a different thing. An ANIM_CURVE_* step names a VECTOR (or a pixel); an
	// ANIM_TANGENT_* step names an END and a MODE — the four-valued
	// Flux_TangentMode the clip stores per end, on the wire since schema 3 (B2) —
	// and it never touches the other end.
	//
	// iEnd is a Zenith_AnimTangentEnd (0 = In, 1 = Out, 2 = Both) and iMode a
	// Flux_TangentMode (0 = Linear, 1 = Flat, 2 = Auto, 3 = Custom), both passed
	// as ints so this header needs neither the clip nor the document header. An
	// out-of-range value asserts at BOOT on the step that is wrong.
	//
	// A typical authoring sequence:
	//   AnimOpenClip("game:Animations/Sway.zanim") -> AnimCurveSetView(true) ->
	//   AnimSelectKey("Hip", 0, 1, 0) -> AnimTangentSetSelectionMode(2, 1) ->
	//   AnimTangentExpectKeyMode("Hip", 0, 1, 0, 1).
	//
	// NOTE: this block must stay CONTIGUOUS (ExecuteAction routes the whole range
	// to ExecuteAnimTangentAction by a pair of comparisons against its first and
	// last member).
	ANIM_TANGENT_SET_KEY_MODE,
	ANIM_TANGENT_SET_SELECTION_MODE,
	ANIM_TANGENT_EXPECT_KEY_MODE,	// END of the contiguous ANIM_TANGENT range (see ANIM_TANGENT_SET_KEY_MODE)

	// IK POSING (E1). A NINTH animation block, appended for the reason the
	// second through eighth exist: adding a verb INSIDE the block above would
	// move ANIM_TANGENT_EXPECT_KEY_MODE, which is the upper bound BOTH the
	// router's range test and the header's static_assert compare against, and
	// which `Automation, AnimTangentEnumBlockIsContiguous` pins by position.
	//
	// ★ ONE MEMBER TODAY, AND IT IS STILL A BLOCK. The router tests it as a range
	// like every other family, so a second IK verb is APPENDED here and costs
	// nothing; a one-off `case` in ExecuteAction's own switch would have to be
	// promoted to a range the first time that happened, moving the boundary this
	// file and two units pin.
	//
	// It performs EXACTLY what Zenith_EditorPanel_Animation::
	// Action_BakeIKForSelectedChain performs — the same call the preview pane's
	// own IK target drag ends in on release — so an authored recipe and a human's
	// gesture cannot diverge.
	//
	// ★ THE TARGET IS PASSED VERBATIM, WITH NO ARITHMETIC ANYWHERE ON THE PATH,
	// which is the *AUTHORED ROTATIONS THAT LAND IN A COMMITTED SCENE* rule
	// (Editor/CLAUDE.md) applied to a position rather than a rotation. What the
	// step packs is what the solver is handed. The rotations that reach the
	// .zanim are the SOLVER's and are not authored here, so nothing on this path
	// re-derives a value between the recipe and the file.
	//
	// A typical authoring sequence:
	//   AnimOpenClip("game:Animations/Reach.zanim") -> AnimSelectBone(7) ->
	//   AnimBakeIK(0.2f, 1.1f, 0.4f) ->
	//   AnimExpectBoneLocalRotation(7, x, y, z, w, 1e-3f).
	//
	// NOTE: this block must stay CONTIGUOUS (ExecuteAction routes the whole
	// range to ExecuteAnimIkAction by a pair of comparisons against its first
	// and last member — which are the same member while it is one wide).
	ANIM_IK_BAKE_TO_TARGET,	// END of the contiguous ANIM_IK range (and its start)

	// CONTIGUOUS: ANIM_EVENT_ADD .. ANIM_EVENT_SET_EMIT_ON_SCRUB; routed to ExecuteAnimEventAction.
	ANIM_EVENT_ADD,
	ANIM_EVENT_SELECT,
	ANIM_EVENT_MOVE_SELECTED,
	ANIM_EVENT_RENAME,
	ANIM_EVENT_SET_PAYLOAD,
	ANIM_EVENT_SET_EMIT_ON_SCRUB,

	// CONTIGUOUS: ANIM_CLIP_SAVE .. ANIM_CLIP_PROMOTE_TO_AUTHORED_OVERRIDE; routed to ExecuteAnimClipAction.
	ANIM_CLIP_SAVE,
	ANIM_CLIP_SAVE_AS,
	ANIM_CLIP_PROMOTE_TO_AUTHORED_OVERRIDE,

	// CONTIGUOUS: ANIM_POSE_CONTROL_SET_ANGLE_SNAP .. ANIM_POSE_CONTROL_SET_KEY_TRANSLATION_FOR_ROOT; routed to ExecuteAnimPoseControlAction.
	ANIM_POSE_CONTROL_SET_ANGLE_SNAP,
	ANIM_POSE_CONTROL_CLEAR_BONE_SELECTION,
	ANIM_POSE_CONTROL_SET_KEY_TRANSLATION_FOR_ROOT,

	// CONTIGUOUS: ANIM_SM_EDIT_SELECT_STATE .. ANIM_SM_EDIT_REMOVE_CLIP_PATH; routed to ExecuteAnimSmEditAction.
	ANIM_SM_EDIT_SELECT_STATE,
	ANIM_SM_EDIT_SELECT_TRANSITION,
	ANIM_SM_EDIT_SELECT_ANY_STATE,
	ANIM_SM_EDIT_CLEAR_SELECTION,
	ANIM_SM_EDIT_SET_STATE_POSITION,
	ANIM_SM_EDIT_REMOVE_CLIP_PATH,

	// CONTIGUOUS: ANIM_SM_PREVIEW_SET_ENABLED .. ANIM_SM_PREVIEW_EXPECT_STATE; routed to ExecuteAnimSmPreviewAction.
	ANIM_SM_PREVIEW_SET_ENABLED,
	ANIM_SM_PREVIEW_TICK,
	ANIM_SM_PREVIEW_SET_FLOAT,
	ANIM_SM_PREVIEW_SET_INT,
	ANIM_SM_PREVIEW_SET_BOOL,
	ANIM_SM_PREVIEW_SET_TRIGGER,
	ANIM_SM_PREVIEW_EXPECT_STATE,

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
// And the same pin for the ANIM block (WU-3.4), whose LAST member is the upper
// bound ExecuteAction's range test compares against: an ANIM verb appended after
// ANIM_EXPECT_SELECTED_COUNT without moving that comparison would never be
// routed at all — it would reach the generic executor and assert at runtime
// instead of failing the build here.
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_EXPECT_SELECTED_COUNT) -
	static_cast<int>(Zenith_EditorActionType::ANIM_OPEN_CLIP) == 15,
	"the ANIM block must stay CONTIGUOUS and sixteen wide — ExecuteAction routes it by range");
// And the same pin for the ANIM_POSE block (WU-4.3), which sits immediately
// after it and is routed by its own pair of comparisons. This is the WIDTH; the
// `Automation, AnimPoseEnumBlockIsContiguous` unit pins each member's POSITION,
// so a reorder that preserves the width fails there naming the member that moved
// rather than at boot inside ExecuteAnimationAction's `default:` assert.
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION) -
	static_cast<int>(Zenith_EditorActionType::ANIM_POSE_SELECT_BONE) == 4,
	"the ANIM_POSE block must stay CONTIGUOUS and five wide — ExecuteAction routes it by range");
// And the same pin for the ANIM_SM block (WU-6.5), the third animation range.
// Width here; the `Automation, AnimSmEnumBlockIsContiguous` unit pins each
// member's POSITION and both boundaries.
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_SM_EXPECT_DEFAULT_STATE) -
	static_cast<int>(Zenith_EditorActionType::ANIM_SM_OPEN) == 24,
	"the ANIM_SM block must stay CONTIGUOUS and twenty-five wide — ExecuteAction routes it by range");
// And the same pin for the ANIM_MASK block (WU-7.1), the FOURTH animation range.
// Width here; the `Automation, AnimMaskEnumBlockIsContiguous` unit pins each
// member's POSITION and both boundaries.
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_EXPECT_WEIGHT) -
	static_cast<int>(Zenith_EditorActionType::ANIM_MASK_OPEN) == 9,
	"the ANIM_MASK block must stay CONTIGUOUS and ten wide — ExecuteAction routes it by range");
// And the same pin for the ANIM_LAYER block (WU-7.2), the FIFTH animation range
// and now the youngest block in the enum. Width here; the
// `Automation, AnimLayerEnumBlockIsContiguous` unit pins each member's POSITION
// and both boundaries — including SET_NAVMESH_ASSET's "must stay outside every
// range", whose neighbour this block has become (it was ANIM_MASK's until this
// one was appended; that assertion has now been re-pointed four times, which is
// the mechanism working rather than a smell).
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_EXPECT_ORDER) -
	static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_ADD) == 9,
	"the ANIM_LAYER block must stay CONTIGUOUS and ten wide — ExecuteAction routes it by range");
// And the same pin for the ANIM_BLEND block (WU-7.3), the SIXTH animation range
// and now the youngest block in the enum. Width here; the
// `Automation, AnimBlendEnumBlockIsContiguous` unit pins each member's POSITION
// and both boundaries — including SET_NAVMESH_ASSET's "must stay outside every
// range", whose neighbour this block has become (it was ANIM_LAYER's until this
// one was appended; that assertion has now been re-pointed five times, which is
// the mechanism working rather than a smell).
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_EXPECT_POINT_POSITION) -
	static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_SET_TREE_KIND) == 8,
	"the ANIM_BLEND block must stay CONTIGUOUS and nine wide — ExecuteAction routes it by range");
// And the same pin for the ANIM_CURVE block (WU-8.2), the SEVENTH animation range.
// Width here; the `Automation, AnimCurveEnumBlockIsContiguous` unit pins each
// member's POSITION and both boundaries.
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_EXPECT_KEY_TANGENT) -
	static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_SET_VIEW) == 7,
	"the ANIM_CURVE block must stay CONTIGUOUS and eight wide — ExecuteAction routes it by range");
// And the same pin for the ANIM_TANGENT block (B3), the EIGHTH animation range.
// Width here; the `Automation, AnimTangentEnumBlockIsContiguous` unit pins each
// member's POSITION and both boundaries.
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_TANGENT_EXPECT_KEY_MODE) -
	static_cast<int>(Zenith_EditorActionType::ANIM_TANGENT_SET_KEY_MODE) == 2,
	"the ANIM_TANGENT block must stay CONTIGUOUS and three wide — ExecuteAction routes it by range");
// A one-member range pins its width against the next block.
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_ADD) -
	static_cast<int>(Zenith_EditorActionType::ANIM_IK_BAKE_TO_TARGET) == 1,
	"ANIM_IK stays one wide, immediately before ANIM_EVENT");
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_SET_EMIT_ON_SCRUB) -
	static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_ADD) == 5,
	"ANIM_EVENT_ must stay contiguous and 6 wide");
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_CLIP_PROMOTE_TO_AUTHORED_OVERRIDE) -
	static_cast<int>(Zenith_EditorActionType::ANIM_CLIP_SAVE) == 2,
	"ANIM_CLIP_ must stay contiguous and 3 wide");
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_CONTROL_SET_KEY_TRANSLATION_FOR_ROOT) -
	static_cast<int>(Zenith_EditorActionType::ANIM_POSE_CONTROL_SET_ANGLE_SNAP) == 2,
	"ANIM_POSE_CONTROL_ must stay contiguous and 3 wide");
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_REMOVE_CLIP_PATH) -
	static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_SELECT_STATE) == 5,
	"ANIM_SM_EDIT_ must stay contiguous and 6 wide");
static_assert(static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_EXPECT_STATE) -
	static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_ENABLED) == 6,
	"ANIM_SM_PREVIEW_ must stay contiguous and 7 wide");

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
	// Animation dope-sheet step helpers (WU-3.4).
	//
	// Each verb routes to the matching Zenith_EditorPanel_Animation::Action_*
	// through a CHECKED wrapper that asserts on `false`, exactly as the graph and
	// material families do — an authoring typo (a bone the clip has no channel
	// for, a key index past the end of a track, a move refused by a collision)
	// fires at BOOT, on the step that is wrong, instead of leaving a clip that is
	// quietly not what the recipe said.
	//
	// ★ THE STEPS ADDRESS A KEY BY INDEX; THE EXECUTOR RESOLVES THE STABLE ID.
	// A recipe is written against a clip a human can see, where "the second key
	// on Hip's rotation track" is the only address that can be typed — but the
	// panel and the document address keys by a STABLE ID (D24), because a retime
	// REORDERS a track and an index would start naming a different key mid-recipe.
	// So iKeyIndex is resolved through Zenith_AnimationDocument::GetKeyIdAtIndex
	// at EXECUTION time, against the track as it stands at that step, and the id
	// is what reaches the action. An index that no longer resolves asserts.
	//
	// iTrack is a Flux_AnimTrack (0 = Translation, 1 = Rotation, 2 = Scale) and
	// iSelectMode a Zenith_AnimSelectMode (0 = REPLACE, 1 = TOGGLE, 2 = ADD),
	// passed as ints so this header needs neither the clip nor the panel header.
	// A NULL or EMPTY szBone addresses the ROOT MOTION track of that kind (which
	// has no scale channel — D16 — so track 2 there is refused).
	//
	// A typical authoring sequence:
	//   AnimOpenClip("game:Animations/Sway.zanim") ->
	//   AnimSelectKey("Hip", 0, 1, 0) -> AnimMoveSelection(0.25f, true) ->
	//   AnimExpectKeyTime("Hip", 0, 1, 1.25f, 0.001f) -> AnimCloseClip().
	//--------------------------------------------------------------------------

	// Opens szAssetPath into the editor's single dope sheet, SHOWING the window
	// (the panel is hidden by default, and a hidden panel draws nothing and
	// records no rects — so every later step, and every human looking at the
	// run, would be working blind).
void AddStep_AnimOpenClip(const char* szAssetPath);
void AddStep_AnimCloseClip();	// forced close; unsaved edits are discarded

void AddStep_AnimSelectKey(const char* szBone, int iTrack, int iKeyIndex, int iSelectMode);
	// Absolute SCREEN coordinates, the space the panel records its rects in —
	// so this hit-tests exactly what was painted, and needs a rendered frame.
void AddStep_AnimBoxSelect(float fX0, float fY0, float fX1, float fY1, int iSelectMode);

void AddStep_AnimMoveSelection(float fDeltaSeconds, bool bSnap);
void AddStep_AnimDeleteSelection();
void AddStep_AnimDuplicateSelection();
void AddStep_AnimCopySelection();
void AddStep_AnimPasteToBone(const char* szBone, float fTimeOffsetSeconds);
void AddStep_AnimRippleRetime(float fFromSeconds, float fDeltaSeconds);
void AddStep_AnimScrub(float fTimeSeconds);
void AddStep_AnimSetDuration(float fDurationSeconds);
void AddStep_AnimUndo();
void AddStep_AnimRedo();

	// ---- assertion steps -----------------------------------------------------
	// These mutate NOTHING. They exist because every step above reports only a
	// bool, and a recipe that authored the wrong thing successfully is exactly
	// the failure the checked wrapper cannot catch.
void AddStep_AnimExpectKeyTime(const char* szBone, int iTrack, int iKeyIndex,
	float fExpectedSeconds, float fToleranceSeconds);
void AddStep_AnimExpectSelectedCount(int iExpectedCount);

	//--------------------------------------------------------------------------
	// Animation POSE authoring (WU-4.3), the ANIM_POSE_* block.
	//
	// The same shape as the dope-sheet verbs above and for the same reasons: one
	// step per atomic Zenith_EditorPanel_Animation Action_*, each routed through
	// the checked wrapper so an authoring typo (a bone index the rig does not
	// have, a rotate with nothing selected) fires at BOOT on the step that is
	// wrong rather than leaving a pose that is quietly not what the recipe said.
	//
	// A typical sequence:
	//   AnimOpenClip -> AnimSelectBone(1) -> AnimRotateSelectedBoneWorld(0,1,0, 30)
	//   -> AnimSetKeyForSelectedBone() -> AnimExpectBoneLocalRotation(1, x,y,z,w, 1e-4)
	//
	// ★ THE ROTATION AXIS MUST BE CARDINAL, and that is an FP-determinism rule
	// rather than a scope limit (Editor/CLAUDE.md, *AUTHORED ROTATIONS THAT LAND
	// IN A COMMITTED SCENE*). A pose authored at boot is SERIALIZED, and
	// glm::angleAxis is a header inline that takes its floating-point model from
	// its own definition point — so a Debug and a Release tools build disagree in
	// the last bit or two and the tracked .zanim ping-pongs in `git status`
	// forever, under every tolerance-based guard. Zenith_Maths::AuthoringRotationX
	// / Y / Z are single non-inline definitions compiled under the pinned model
	// and cover exactly the three cardinal axes, so a non-cardinal axis is
	// REFUSED by the executor rather than silently computed the other way.
	//--------------------------------------------------------------------------
void AddStep_AnimSelectBone(int iBoneIndex);
	// (fAxisX, fAxisY, fAxisZ) must be one of (1,0,0) / (0,1,0) / (0,0,1) — see
	// above. The angle is in DEGREES, converted with Zenith_Maths::AuthoringRadians.
void AddStep_AnimRotateSelectedBoneWorld(float fAxisX, float fAxisY, float fAxisZ, float fAngleDegrees);
void AddStep_AnimSetKeyForSelectedBone();
void AddStep_AnimSetAutoKey(bool bEnabled);
	// An ASSERTION step. (fX, fY, fZ, fW) are in SERIALIZED order, deliberately
	// not glm::quat's (w, x, y, z) constructor order, matching
	// AddStep_SetTransformRotationQuat — so a caller freezing a value read out of
	// a file types it in the order the file has it.
void AddStep_AnimExpectBoneLocalRotation(int iBoneIndex, float fX, float fY, float fZ, float fW,
	float fTolerance);

	//--------------------------------------------------------------------------
	// Animator-controller STATE MACHINE authoring (WU-6.5), the ANIM_SM_* block.
	//
	// One step per atomic Zenith_EditorPanel_AnimStateMachine Action_*, each
	// routed through a checked wrapper that asserts on `false` — an authoring
	// typo (a state name the machine does not have, a transition index past the
	// end, a condition naming an undeclared parameter) fires at BOOT on the step
	// that is wrong, rather than leaving a controller that is quietly not what
	// the recipe said.
	//
	// ★ A STATE IS ADDRESSED BY NAME AND A TRANSITION BY INDEX, which is the
	// opposite pairing to the dope-sheet verbs (bone NAME + key INDEX) and is not
	// an inconsistency. A state name IS the machine's key — it is what a
	// transition targets and what survives a delete-and-undo — while a transition
	// has no identity at all: it is a struct in a vector, and its index is the
	// only thing anyone can type. Everything that renumbers those indices (an
	// add, a remove) is a step of its own, so a recipe's indices are readable in
	// source order.
	//
	// ★ AN EMPTY szFromState ADDRESSES THE MACHINE'S ANY-STATE LIST, on every
	// transition verb. Dropping that case would leave any-state transitions
	// unreachable from a recipe, and they are the ones a "hit reaction" graph is
	// built out of.
	//
	// A typical authoring sequence:
	//   AnimSmOpenFresh("game:Anim/Player.zanimctrl") ->
	//   AnimSmAddClipPath("game:Anim/Idle.zanim") -> AnimSmAddClipPath(".../Walk.zanim") ->
	//   AnimSmAddParameter("Speed", 0, 0.0f) ->
	//   AnimSmAddState("Idle") -> AnimSmAddState("Walk") ->
	//   AnimSmSetStateClip("Idle", "Idle") -> AnimSmSetStateClip("Walk", "Walk") ->
	//   AnimSmSetDefaultState("Idle") -> AnimSmAddTransition("Idle", "Walk") ->
	//   AnimSmAddCondition("Idle", 0, "Speed", 2 /* Greater */, 0.1f) -> AnimSmSave().
	//--------------------------------------------------------------------------

	// Open an EXISTING .zanimctrl, SHOWING the window (a hidden panel draws
	// nothing and records no rects, so every later step would be working blind).
void AddStep_AnimSmOpen(const char* szAssetPath);
	// Open an EMPTY controller TARGETED at that path — the regenerate-from-
	// scratch entry point, the twin of AddStep_GraphOpenFresh. Deliberately NOT a
	// fallback inside AnimSmOpen: "the file was not there" and "the path was
	// typed wrong" are the same observation, and a silent fresh start on a typo
	// authors a whole controller into a path nothing reads.
void AddStep_AnimSmOpenFresh(const char* szAssetPath);
void AddStep_AnimSmClose();	// forced close; unsaved edits are discarded

	// iLayerId < 0 selects the def's TOP-LEVEL machine; otherwise it is a stable
	// LAYER ID (never an index — inserting a layer renumbers every index above
	// it, and WU-6.3 exists because of exactly that).
void AddStep_AnimSmSelectLayer(int iLayerId);

void AddStep_AnimSmAddClipPath(const char* szClipAssetPath);

void AddStep_AnimSmAddState(const char* szStateName);
void AddStep_AnimSmRemoveState(const char* szStateName);
void AddStep_AnimSmRenameState(const char* szOldName, const char* szNewName);
void AddStep_AnimSmSetDefaultState(const char* szStateName);
	// szClipName is the CLIP's NAME (what Flux_AnimationClipCollection keys on),
	// not its file path. An EMPTY name clears the state's tree.
void AddStep_AnimSmSetStateClip(const char* szStateName, const char* szClipName);

void AddStep_AnimSmAddTransition(const char* szFromState, const char* szToState);
void AddStep_AnimSmRemoveTransition(const char* szFromState, int iIndex);
void AddStep_AnimSmSetTransitionDuration(const char* szFromState, int iIndex, float fSeconds);
	// fNormalizedExitTime is a [0,1] fraction of the SOURCE state's clip, and is
	// ignored when bHasExitTime is false — the pair is one decision, because
	// m_fExitTime is only read when m_bHasExitTime.
void AddStep_AnimSmSetTransitionExitTime(const char* szFromState, int iIndex,
	bool bHasExitTime, float fNormalizedExitTime);
void AddStep_AnimSmSetTransitionInterruptible(const char* szFromState, int iIndex, bool bInterruptible);
	// iCompareOp is a Flux_TransitionCondition::CompareOp (0 Equal, 1 NotEqual,
	// 2 Greater, 3 Less, 4 GreaterEqual, 5 LessEqual), passed as an int so this
	// header needs neither the state-machine nor the panel header. The
	// condition's PARAMETER TYPE is taken from the DECLARATION, so the parameter
	// must already exist.
void AddStep_AnimSmAddCondition(const char* szFromState, int iIndex,
	const char* szParameterName, int iCompareOp, float fThreshold);
void AddStep_AnimSmRemoveCondition(const char* szFromState, int iIndex, int iConditionIndex);

	// iType is a Flux_AnimationParameters::ParamType (0 Float, 1 Int, 2 Bool,
	// 3 Trigger). fDefault is read as the declared type's default.
void AddStep_AnimSmAddParameter(const char* szName, int iType, float fDefault);
void AddStep_AnimSmRemoveParameter(const char* szName);

void AddStep_AnimSmUndo();
void AddStep_AnimSmRedo();
void AddStep_AnimSmSave();

	// Events use NORMALIZED time. Indices address the current sorted event list
	// at execution, then resolve to stable document IDs, just like key steps.
	// Selection modes are Zenith_AnimSelectMode. Payload is the full XYZW vector.
	void AddStep_AnimEventAdd(float fTime, const char* szName);
	void AddStep_AnimEventSelect(int iEventIndex, int iMode);
	void AddStep_AnimEventMoveSelected(float fDeltaNormalized, bool bSnap);
	void AddStep_AnimEventRename(int iEventIndex, const char* szName);
	void AddStep_AnimEventSetPayload(int iEventIndex, float fX, float fY, float fZ, float fW);
	void AddStep_AnimEventSetEmitEventsOnScrub(bool bEmit);

	// Save refuses external conflicts. SaveAs adopts a new path. Promotion uses
	// the document's Authored root (tests MUST set its scratch root override).
	void AddStep_AnimSave();
	void AddStep_AnimSaveAs(const char* szPath);
	void AddStep_AnimPromoteToAuthoredOverride(const char* szSourcePath);

	// Snap and clear are idempotent. Root translation keying requires a selected
	// root on a resolved rig and keys its translation AND rotation in one edit.
	void AddStep_AnimSetPoseAngleSnap(bool bEnabled);
	void AddStep_AnimClearBoneSelection();
	void AddStep_AnimSetKeyTranslationForRoot();

	// SM selection/layout: an empty transition source names the any-state list.
	// Clear is idempotent; positions are graph coordinates, clip paths are owned.
	void AddStep_AnimSmSelectState(const char* szState);
	void AddStep_AnimSmSelectTransition(const char* szFrom, int iIndex);
	void AddStep_AnimSmSelectAnyState();
	void AddStep_AnimSmClearSelection();
	void AddStep_AnimSmSetStatePosition(const char* szState, float fX, float fY);
	void AddStep_AnimSmRemoveClipPath(const char* szPath);

	// Preview parameters affect the live preview, not authored defaults. Tick is
	// in seconds. ExpectPreviewState observes the selected machine's highlight.
	void AddStep_AnimSmSetPreviewEnabled(bool bEnabled);
	void AddStep_AnimSmTickPreview(float fDtSeconds);
	void AddStep_AnimSmSetPreviewFloat(const char* szName, float fValue);
	void AddStep_AnimSmSetPreviewInt(const char* szName, int iValue);
	void AddStep_AnimSmSetPreviewBool(const char* szName, bool bValue);
	void AddStep_AnimSmSetPreviewTrigger(const char* szName);
	void AddStep_AnimSmExpectPreviewState(const char* szState);

	// Hands the working def to the preview controller through
	// ReloadFromControllerDef (D45), so the current state, the matched parameter
	// values and the layer weights survive the edit. Builds the preview when
	// there is not one yet.
void AddStep_AnimSmApply();

	// ---- assertion steps -----------------------------------------------------
void AddStep_AnimSmExpectStateCount(int iExpectedCount);
void AddStep_AnimSmExpectDefaultState(const char* szStateName);

	//--------------------------------------------------------------------------
	// BONE MASK authoring (WU-7.1), the ANIM_MASK_* block.
	//
	// One step per atomic Zenith_EditorPanel_Animation Action_Mask*, each routed
	// through a checked wrapper that asserts on `false` — an authoring typo (a
	// bone the rig does not carry, a subtree step before a rig is previewed, a
	// save onto a file something else changed) fires at BOOT on the step that is
	// wrong, rather than leaving a mask that is quietly not what the recipe said.
	//
	// ★ A BONE IS ADDRESSED BY NAME, ALWAYS, because that is what a `.zanimmask`
	// STORES (D47/D46). An index would be an index into whichever rig happened to
	// be previewed, and the whole point of the format is that the same mask means
	// the same thing on two rigs that number their bones differently.
	//
	// ★ THE SUBTREE STEP NEEDS A PREVIEWED RIG. A hierarchy comes from the dope
	// sheet's session skeleton and from nowhere else, so a recipe opens a clip
	// whose rig resolves first. Per-bone weights need no rig at all — they are
	// names.
	//
	// A typical authoring sequence:
	//   AnimOpenClip("engine:Meshes/StickFigure/Walk.zanim") ->
	//   AnimMaskOpenFresh("game:Anim/UpperBody.zanimmask") ->
	//   AnimMaskSetSubtree("Spine", 1.0f) -> AnimMaskSetWeight("Head", 0.5f) ->
	//   AnimMaskExpectWeight("Head", 0.5f, 1e-4f) -> AnimMaskSave().
	//--------------------------------------------------------------------------

	// Open an EXISTING .zanimmask, SHOWING the section (a collapsed one reports
	// its refusals to nobody).
void AddStep_AnimMaskOpen(const char* szAssetPath);
	// Open an EMPTY mask TARGETED at that path — the regenerate-from-scratch
	// entry point, and deliberately NOT a fallback inside AnimMaskOpen: "the file
	// was not there" and "the path was typed wrong" are the same observation, and
	// a silent fresh start on a typo authors a whole mask into a path nothing
	// reads.
void AddStep_AnimMaskOpenFresh(const char* szAssetPath);
void AddStep_AnimMaskClose();	// forced close; unsaved mask edits are discarded

	// fWeight is CLAMPED to [0,1] by the document. An ASSIGNMENT: re-stating a
	// weight a bone already carries SUCCEEDS and pushes no undo entry.
void AddStep_AnimMaskSetWeight(const char* szBoneName, float fWeight);
	// szBoneName AND every DESCENDANT of it, as ONE undo step. Needs a previewed
	// rig — see above.
void AddStep_AnimMaskSetSubtree(const char* szBoneName, float fWeight);
	// D47's explicit flag. TRUE by default on a fresh mask: a .zanimmask that
	// exists IS a mask, whatever its weights sum to.
void AddStep_AnimMaskSetHasAvatar(bool bHasAvatarMask);

void AddStep_AnimMaskUndo();
void AddStep_AnimMaskRedo();
void AddStep_AnimMaskSave();

	// ---- assertion step ------------------------------------------------------
	// A bone the mask does not name weighs 0, which is the same answer a resolved
	// Flux_BoneMask gives — so this asserts the WEIGHT and never "is there a row".
void AddStep_AnimMaskExpectWeight(const char* szBoneName, float fExpectedWeight, float fTolerance);

	//--------------------------------------------------------------------------
	// ANIMATOR LAYER authoring (WU-7.2), the ANIM_LAYER_* block.
	//
	// One step per atomic Zenith_EditorPanel_AnimStateMachine layer Action_*, each
	// routed through a checked wrapper that asserts on `false` — an authoring typo
	// (a layer id the def does not carry, a destination index past the end, a mask
	// path on an additive layer) fires at BOOT on the step that is wrong, rather
	// than leaving a controller that is quietly not what the recipe said.
	//
	// ★ THE STATE-MACHINE DOCUMENT MUST ALREADY BE OPEN. These edit the same
	// .zanimctrl the ANIM_SM_* family does, through the same panel, so a recipe
	// begins with AnimSmOpen / AnimSmOpenFresh and ends with AnimSmSave. There is
	// no separate layer document.
	//
	// ★ AND THE ORDER OF THE LIST IS THE BLEND ORDER, which is what
	// AnimLayerMove changes and what AnimLayerExpectOrder states. Index 0 is the
	// base; everything above composes on top of it.
	//
	// A typical authoring sequence:
	//   AnimSmOpenFresh("game:Anim/Player.zanimctrl") ->
	//   AnimLayerAdd("Base") -> AnimLayerAdd("Aim") ->
	//   AnimLayerSetWeight(1, 0.75f) -> AnimLayerSetBlendMode(1, 1 /* additive */) ->
	//   AnimLayerSetEmitEvents(1, false) ->
	//   AnimLayerSelect(0) -> AnimSmAddState("Idle") -> ... -> AnimSmSave().
	//--------------------------------------------------------------------------

	// Appends a layer at the END of the blend order and SELECTS it, so the
	// AnimSm* steps that follow author ITS machine. Its id is the def's next
	// minted one — 0 for the first layer of a fresh controller.
void AddStep_AnimLayerAdd(const char* szLayerName);
void AddStep_AnimLayerRemove(int iLayerId);
void AddStep_AnimLayerRename(int iLayerId, const char* szNewName);
	// CLAMPED to [0,1] by the document. An ASSIGNMENT: re-stating a weight a layer
	// already carries SUCCEEDS and pushes no undo entry.
void AddStep_AnimLayerSetWeight(int iLayerId, float fWeight);
	// iBlendMode is a Flux_LayerBlendMode (0 Override, 1 Additive), passed as an
	// int so this header needs neither the layer nor the panel header.
void AddStep_AnimLayerSetBlendMode(int iLayerId, int iBlendMode);
	// D36's per-layer event switch.
void AddStep_AnimLayerSetEmitEvents(int iLayerId, bool bEmitEvents);
	// ★ REFUSED ON AN ADDITIVE LAYER, which is a real failure and asserts: the
	// runtime never consults an additive layer's mask, so a step that succeeded
	// here would author an assignment nothing reads. An EMPTY path CLEARS the
	// assignment and is always allowed.
void AddStep_AnimLayerSetMaskPath(int iLayerId, const char* szMaskAssetPath);
	// iNewIndex is a POSITION IN THE BLEND ORDER. Past the end is refused rather
	// than clamped — clamping would turn "move it down" at the bottom into a
	// silent no-op reporting success.
void AddStep_AnimLayerMove(int iLayerId, int iNewIndex);
	// Selects the layer's MACHINE for the AnimSm* steps that follow, and pushes
	// its blend mode into the dope sheet's bone-mask sub-panel. The TOP-LEVEL
	// machine is not a layer: use AddStep_AnimSmSelectLayer(-1) for it.
void AddStep_AnimLayerSelect(int iLayerId);

	// ---- assertion step ------------------------------------------------------
	// The layer at iIndex in the BLEND ORDER is named szExpectedName. This is the
	// verb a reorder recipe is judged on.
void AddStep_AnimLayerExpectOrder(int iIndex, const char* szExpectedName);

	//--------------------------------------------------------------------------
	// BLEND-TREE authoring (WU-7.3), the ANIM_BLEND_* block.
	//
	// One step per atomic Zenith_EditorPanel_AnimStateMachine blend Action_*, each
	// routed through a checked wrapper that asserts on `false` — an authoring typo
	// (a state that is not a blend space, a parameter that is not a declared
	// Float, a point index past the end) fires at BOOT on the step that is wrong,
	// rather than leaving a space that is quietly not what the recipe said.
	//
	// ★ THE STATE-MACHINE DOCUMENT MUST ALREADY BE OPEN, and the STATE must
	// already exist. These edit the same .zanimctrl the ANIM_SM_* family does,
	// through the same panel, so a recipe begins with AnimSmOpen / AnimSmOpenFresh
	// plus an AnimSmAddState and ends with AnimSmSave.
	//
	// ★ AND A POINT PLAYS A CLIP BY **NAME**, resolved through the controller's
	// Flux_AnimationClipCollection — so the clip's PATH has to be in the def's
	// clip list (AnimSmAddClipPath) and the name here is the one the .zanim
	// carries, exactly as AnimSmSetStateClip's is.
	//--------------------------------------------------------------------------

	// iKind: 0 Single Clip, 1 Blend Space 1D, 2 Blend Space 2D — an int so this
	// header needs neither the document nor the panel header, the same choice
	// AddStep_AnimLayerSetBlendMode makes. CONVERTING CARRIES THE CLIPS ACROSS,
	// and an ASSIGNMENT to the kind already in place succeeds and edits nothing.
void AddStep_AnimBlendSetTreeKind(const char* szStateName, int iKind);
	// iAxis: 0 X, 1 Y. An EMPTY name UNBINDS. Refused for anything that is not a
	// DECLARED Float, and for a Y on a 1D space.
void AddStep_AnimBlendSetParameter(const char* szStateName, int iAxis, const char* szParameterName);
	// fY is IGNORED on a 1D space — one position shape for both, so this payload
	// and the panel's verb carry the same two floats whichever kind it is.
void AddStep_AnimBlendAddPoint(const char* szStateName, const char* szClipName, float fX, float fY);
void AddStep_AnimBlendRemovePoint(const char* szStateName, int iIndex);
void AddStep_AnimBlendSetPointClip(const char* szStateName, int iIndex, const char* szClipName);
	// ★ MAY RENUMBER on a 1D space — see the enum block's note.
void AddStep_AnimBlendSetPointPosition(const char* szStateName, int iIndex, float fX, float fY);
	// Panel selection only; -1 CLEARS it. Selecting a point the SELECTED state
	// does not have is a refusal, so a recipe selects the state first.
void AddStep_AnimBlendSelectPoint(int iIndex);

	// ---- assertion steps -----------------------------------------------------
void AddStep_AnimBlendExpectPointCount(const char* szStateName, int iExpectedCount);
	// The position AND the clip are both what a point is, but only the position
	// can be got wrong by a renumber — which is what this verb exists to catch.
void AddStep_AnimBlendExpectPointPosition(const char* szStateName, int iIndex, float fX, float fY,
	float fTolerance);

	//--------------------------------------------------------------------------
	// CURVE-EDITOR authoring (WU-8.2), the ANIM_CURVE_* block.
	//
	// One step per atomic Zenith_EditorPanel_Animation curve Action_*, each routed
	// through a checked wrapper that asserts on `false` — an authoring typo (a key
	// index past the end of a track, a root-motion track that cannot hold a
	// tangent, an Auto with nothing selected) fires at BOOT on the step that is
	// wrong, rather than leaving a clip whose curves are quietly not what the
	// recipe said.
	//
	// ★ THE CLIP DOCUMENT MUST ALREADY BE OPEN, and the SELECTION verbs need a
	// selection: these edit the same .zanim the ANIM_* family does, through the
	// same panel, so a recipe begins with AnimOpenClip and selects with
	// AnimSelectKey / AnimBoxSelect.
	//
	// ★ THE DRAG STEP NEEDS A RENDERED FRAME. It goes through the pixel mapping,
	// whose origin is the canvas geometry — which is exactly what makes it the
	// verb that proves a human's drag and a recipe's produce the same tangent.
	//
	// iTrack is a Flux_AnimTrack (0 = Translation, 1 = Rotation, 2 = Scale) and
	// iComponent is 0 = x, 1 = y, 2 = z, both passed as ints so this header needs
	// neither the clip nor the panel header.
	//--------------------------------------------------------------------------

	// ASSIGNMENTS: re-stating the state the view is already in SUCCEEDS and edits
	// nothing. Turning the view ON also queues a fit for the next rendered frame.
void AddStep_AnimCurveSetView(bool bShow);
	// Unified: a handle drag writes BOTH halves, keeping the key smooth.
void AddStep_AnimCurveSetUnified(bool bUnified);

	// Both tangents of one key, in value units per second (radians per second on a
	// rotation track — a rotation tangent is a body-frame ANGULAR VELOCITY).
void AddStep_AnimCurveSetKeyTangents(const char* szBone, int iTrack, int iKeyIndex,
	float fInX, float fInY, float fInZ, float fOutX, float fOutY, float fOutZ);

	// The SELECTION, one compound each, and BOTH ends of every key it names.
	// Auto stores the per-key Catmull-Rom vector AS Flux_TangentMode::AUTO, which
	// the document then MAINTAINS across a retime; Linear stores exact zeroes as
	// LINEAR — the sampler's linear branch, which is NOT a flat handle even though
	// a FLAT end carries the same zeroes (Flux/MeshAnimation/CLAUDE.md → *Tangent
	// sampling*). For ONE end, or for FLAT, use the ANIM_TANGENT_* verbs below.
void AddStep_AnimCurveSetSelectionAuto();
void AddStep_AnimCurveSetSelectionLinear();

	// Absolute SCREEN coordinates, the space the panel records its rects in — so
	// this drops the handle exactly where a click would, and needs a rendered
	// frame. bIn picks which end of the handle pair.
void AddStep_AnimCurveDragHandleToPixel(const char* szBone, int iTrack, int iKeyIndex, int iComponent,
	bool bIn, float fX, float fY);

	// Fit the VALUE axis to the sampled extent of what the view is drawing.
void AddStep_AnimCurveFitToSelection();

	// ---- assertion step ------------------------------------------------------
	// One END of one key's tangent pair. An all-zero expectation is how a recipe
	// states "this key is back to LINEAR", which is the thing an undo has to
	// restore exactly.
void AddStep_AnimCurveExpectKeyTangent(const char* szBone, int iTrack, int iKeyIndex, bool bIn,
	float fExpectedX, float fExpectedY, float fExpectedZ, float fTolerance);

	//--------------------------------------------------------------------------
	// TANGENT-MODE authoring (B3), the ANIM_TANGENT_* block.
	//
	// One step per Zenith_EditorPanel_Animation mode Action_*, each routed through
	// a checked wrapper that asserts on `false` — so an authoring typo (a key index
	// past the end of a track, a root-motion track, an Auto on a track that cannot
	// answer, a mode set with nothing selected) fires at BOOT on the step that is
	// wrong.
	//
	// ★ THESE NAME AN END AND A MODE, WHICH IS WHY THEY ARE NOT ANIM_CURVE_*
	// VERBS. The mode is what the clip STORES per end (schema 3, B2): LINEAR and
	// FLAT are the same six zero floats told apart by it, AUTO is provenance the
	// document maintains across a retime, and CUSTOM claims the vector already
	// there. A recipe that could only state vectors could not author any of that.
	//
	// iEnd is a Zenith_AnimTangentEnd (0 = In, 1 = Out, 2 = Both) and iMode a
	// Flux_TangentMode (0 = Linear, 1 = Flat, 2 = Auto, 3 = Custom).
	//--------------------------------------------------------------------------

	// ONE key, addressed by (bone, track, INDEX) like every other animation verb.
void AddStep_AnimTangentSetKeyMode(const char* szBone, int iTrack, int iKeyIndex, int iEnd, int iMode);
	// The SELECTION, as ONE compound. Root-motion keys in it are SKIPPED.
void AddStep_AnimTangentSetSelectionMode(int iEnd, int iMode);

	// ---- assertion step ------------------------------------------------------
	// The STORED mode of the named end. With iEnd = Both this asserts the two ends
	// agree AND carry the expected mode — a mixed key fails it, because "both ends
	// are Flat" is a different claim from "each end is something".
void AddStep_AnimTangentExpectKeyMode(const char* szBone, int iTrack, int iKeyIndex, int iEnd,
	int iExpectedMode);

	//--------------------------------------------------------------------------
	// IK POSING (E1), the one-verb ANIM_IK_* block.
	//
	// Solve a transient chain from the SELECTED bone (the effector) and up to two
	// of its ancestors to a MODEL-SPACE target, then bake the result down to
	// rotation keys — Zenith_EditorPanel_Animation::Action_BakeIKForSelectedChain,
	// which is the same call the preview pane's target drag ends in on release,
	// and which keys through Action_SetKeyForBones as ONE undo step.
	//
	// ★ THE CLIP MUST BE OPEN, THE RIG MUST HAVE RESOLVED AND A BONE MUST BE
	// SELECTED: begin with AnimOpenClip and AnimSelectBone. A ROOT effector is
	// refused (there is nothing above it to bend), as is a target the solver
	// cannot reach a finite rotation for — both fire at BOOT through the checked
	// wrapper, on the step that is wrong.
	//
	// ★ NO RENDERED FRAME IS NEEDED, unlike the curve drag: the target is stated
	// in model space rather than picked off the screen, so nothing here goes
	// through the pixel mapping.
	//
	// ★ THE THREE FLOATS ARE PASSED VERBATIM — no arithmetic between this call
	// and Zenith_AnimationPoseIK. That is deliberate and it is the enum block's
	// note in full: it is the strongest form of the FP-determinism rule
	// (*AUTHORED ROTATIONS THAT LAND IN A COMMITTED SCENE*, Editor/CLAUDE.md),
	// the same one AddStep_SetTransformRotationQuat takes.
	//--------------------------------------------------------------------------
void AddStep_AnimBakeIK(float fTargetModelX, float fTargetModelY, float fTargetModelZ);

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
