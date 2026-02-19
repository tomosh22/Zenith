#pragma once

#ifdef ZENITH_TOOLS

#include "Collections/Zenith_Vector.h"

//=============================================================================
// Editor Automation System
//
// Replaces Project_CreateScenes() with a sequence of atomic editor actions.
// Each step simulates a single user interaction (button click, field edit).
// Execution is driven by Zenith_Editor::Update() — one step per frame with
// full frame ticking (rendering, physics, scene updates) between steps.
//
// High-level operations (scene create/save/unload, entity create/select,
// component add, main camera set, behaviour set) route through Zenith_Editor
// methods, ensuring identical code paths to ImGui panels.
// Field-level edits (camera, transform, UI, particles, colliders, models)
// access component setters directly — matching what the properties panel
// does after ImGui widget interaction. Scene-level operations that have
// no ImGui UI equivalent (RegisterSceneBuildIndex, LoadSceneByIndex,
// SetLoadingScene, SetInitialSceneLoadCallback) call Zenith_SceneManager
// directly.
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

	// UI element creation and field edits
	CREATE_UI_TEXT,
	CREATE_UI_BUTTON,
	CREATE_UI_RECT,
	SET_UI_ANCHOR,
	SET_UI_POSITION,
	SET_UI_SIZE,
	SET_UI_FONT_SIZE,
	SET_UI_COLOR,
	SET_UI_ALIGNMENT,
	SET_UI_VISIBLE,

	// UI button-specific field edits
	SET_UI_BUTTON_NORMAL_COLOR,
	SET_UI_BUTTON_HOVER_COLOR,
	SET_UI_BUTTON_PRESSED_COLOR,
	SET_UI_BUTTON_FONT_SIZE,

	// Script (via Zenith_Editor::SetBehaviourOnSelected / SetBehaviourForSerializationOnSelected)
	SET_BEHAVIOUR,
	SET_BEHAVIOUR_FOR_SERIALIZATION,

	// Particles
	SET_PARTICLE_CONFIG,
	SET_PARTICLE_EMITTING,

	// Collider
	ADD_COLLIDER_SHAPE,

	// Model
	ADD_MESH_ENTRY,

	// Scene loading
	SET_LOADING_SCENE,
	SET_INITIAL_SCENE_LOAD_CALLBACK,

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
	static void Begin();
	static bool IsRunning() { return s_bRunning; }
	static bool IsComplete() { return s_bComplete; }
	static void ExecuteNextStep();
	static void Reset();

	//--------------------------------------------------------------------------
	// Scene Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_CreateScene(const char* szName);
	static void AddStep_SaveScene(const char* szPath);
	static void AddStep_UnloadScene();

	//--------------------------------------------------------------------------
	// Entity Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_CreateEntity(const char* szName);
	static void AddStep_SelectEntity(const char* szName);
	static void AddStep_SetEntityTransient(bool bTransient);

	//--------------------------------------------------------------------------
	// Component Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_AddComponent(const char* szDisplayName);

	// Convenience wrappers for common components
	static void AddStep_AddCamera() { AddStep_AddComponent("Camera"); }
	static void AddStep_AddUI() { AddStep_AddComponent("UI"); }
	static void AddStep_AddScript() { AddStep_AddComponent("Script"); }
	static void AddStep_AddParticleEmitter() { AddStep_AddComponent("Particle Emitter"); }
	static void AddStep_AddCollider() { AddStep_AddComponent("Collider"); }
	static void AddStep_AddModel() { AddStep_AddComponent("Model"); }
	static void AddStep_AddAnimator() { AddStep_AddComponent("Animator"); }

	//--------------------------------------------------------------------------
	// Camera Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_SetCameraPosition(float fX, float fY, float fZ);
	static void AddStep_SetCameraPitch(float fPitch);
	static void AddStep_SetCameraYaw(float fYaw);
	static void AddStep_SetCameraFOV(float fFOV);
	static void AddStep_SetCameraNear(float fNear);
	static void AddStep_SetCameraFar(float fFar);
	static void AddStep_SetCameraAspect(float fAspect);
	static void AddStep_SetAsMainCamera();

	//--------------------------------------------------------------------------
	// Transform Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_SetTransformPosition(float fX, float fY, float fZ);
	static void AddStep_SetTransformScale(float fX, float fY, float fZ);

	//--------------------------------------------------------------------------
	// UI Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_CreateUIText(const char* szName, const char* szText);
	static void AddStep_CreateUIButton(const char* szName, const char* szText);
	static void AddStep_CreateUIRect(const char* szName);
	static void AddStep_SetUIAnchor(const char* szElement, int iPreset);
	static void AddStep_SetUIPosition(const char* szElement, float fX, float fY);
	static void AddStep_SetUISize(const char* szElement, float fW, float fH);
	static void AddStep_SetUIFontSize(const char* szElement, float fSize);
	static void AddStep_SetUIColor(const char* szElement, float fR, float fG, float fB, float fA);
	static void AddStep_SetUIAlignment(const char* szElement, int iAlignment);
	static void AddStep_SetUIVisible(const char* szElement, bool bVisible);

	//--------------------------------------------------------------------------
	// UI Button Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_SetUIButtonNormalColor(const char* szElement, float fR, float fG, float fB, float fA);
	static void AddStep_SetUIButtonHoverColor(const char* szElement, float fR, float fG, float fB, float fA);
	static void AddStep_SetUIButtonPressedColor(const char* szElement, float fR, float fG, float fB, float fA);
	static void AddStep_SetUIButtonFontSize(const char* szElement, float fSize);

	//--------------------------------------------------------------------------
	// Script Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_SetBehaviour(const char* szBehaviourName);
	static void AddStep_SetBehaviourForSerialization(const char* szBehaviourName);

	//--------------------------------------------------------------------------
	// Particle Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_SetParticleConfig(Flux_ParticleEmitterConfig* pxConfig);
	static void AddStep_SetParticleEmitting(bool bEmitting);

	//--------------------------------------------------------------------------
	// Collider Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_AddColliderShape(int iVolumeType, int iBodyType);

	//--------------------------------------------------------------------------
	// Model Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_AddMeshEntry(Flux_MeshGeometry* pxGeometry, Zenith_MaterialAsset* pxMaterial);

	//--------------------------------------------------------------------------
	// Scene Loading Step Helpers
	//--------------------------------------------------------------------------
	static void AddStep_SetLoadingScene(bool bLoading);
	static void AddStep_SetInitialSceneLoadCallback(void (*pfnCallback)());

	//--------------------------------------------------------------------------
	// Custom Step (for game-specific operations)
	//--------------------------------------------------------------------------
	static void AddStep_Custom(void (*pfnFunc)());

private:
	static void ExecuteAction(const Zenith_EditorAction& xAction);

	static Zenith_Vector<Zenith_EditorAction> s_axActions;
	static uint32_t s_uCurrentAction;
	static bool s_bRunning;
	static bool s_bComplete;
};

#endif // ZENITH_TOOLS
