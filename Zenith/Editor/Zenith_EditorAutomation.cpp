#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "UI/Zenith_UI.h"
#include "UI/Zenith_UIButton.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "AssetHandling/Zenith_MaterialAsset.h"

//=============================================================================
// Static member definitions
//=============================================================================
Zenith_Vector<Zenith_EditorAction> Zenith_EditorAutomation::s_axActions;
uint32_t Zenith_EditorAutomation::s_uCurrentAction = 0;
bool Zenith_EditorAutomation::s_bRunning = false;
bool Zenith_EditorAutomation::s_bComplete = false;

//=============================================================================
// Execution
//=============================================================================

void Zenith_EditorAutomation::Begin()
{
	s_uCurrentAction = 0;
	s_bRunning = true;
	s_bComplete = false;
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Begin: %u steps queued", s_axActions.GetSize());
}

void Zenith_EditorAutomation::ExecuteNextStep()
{
	if (!s_bRunning || s_bComplete)
		return;

	if (s_uCurrentAction >= s_axActions.GetSize())
	{
		s_bRunning = false;
		s_bComplete = true;
		Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Complete: all %u steps executed", s_axActions.GetSize());
		s_axActions.Clear();
		return;
	}

	const Zenith_EditorAction& xAction = s_axActions.Get(s_uCurrentAction);
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Step %u/%u", s_uCurrentAction + 1, s_axActions.GetSize());

	ExecuteAction(xAction);
	s_uCurrentAction++;

	// Detect completion immediately after executing the last step
	if (s_uCurrentAction >= s_axActions.GetSize())
	{
		s_bRunning = false;
		s_bComplete = true;
		Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Complete: all %u steps executed", s_axActions.GetSize());
		s_axActions.Clear();
	}
}

void Zenith_EditorAutomation::Reset()
{
	s_axActions.Clear();
	s_uCurrentAction = 0;
	s_bRunning = false;
	s_bComplete = false;
}

//=============================================================================
// Step Helpers
//=============================================================================

// -- Scene --

void Zenith_EditorAutomation::AddStep_CreateScene(const char* szName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::CREATE_SCENE;
	xAction.m_szArg1 = szName;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SaveScene(const char* szPath)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SAVE_SCENE;
	xAction.m_szArg1 = szPath;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_UnloadScene()
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::UNLOAD_SCENE;
	s_axActions.PushBack(xAction);
}

// -- Entity --

void Zenith_EditorAutomation::AddStep_CreateEntity(const char* szName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::CREATE_ENTITY;
	xAction.m_szArg1 = szName;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SelectEntity(const char* szName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SELECT_ENTITY;
	xAction.m_szArg1 = szName;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetEntityTransient(bool bTransient)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_ENTITY_TRANSIENT;
	xAction.m_bArg = bTransient;
	s_axActions.PushBack(xAction);
}

// -- Component --

void Zenith_EditorAutomation::AddStep_AddComponent(const char* szDisplayName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::ADD_COMPONENT;
	xAction.m_szArg1 = szDisplayName;
	s_axActions.PushBack(xAction);
}

// -- Camera --

void Zenith_EditorAutomation::AddStep_SetCameraPosition(float fX, float fY, float fZ)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_CAMERA_POSITION;
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	xAction.m_afArgs[2] = fZ;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetCameraPitch(float fPitch)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_CAMERA_PITCH;
	xAction.m_afArgs[0] = fPitch;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetCameraYaw(float fYaw)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_CAMERA_YAW;
	xAction.m_afArgs[0] = fYaw;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetCameraFOV(float fFOV)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_CAMERA_FOV;
	xAction.m_afArgs[0] = fFOV;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetCameraNear(float fNear)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_CAMERA_NEAR;
	xAction.m_afArgs[0] = fNear;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetCameraFar(float fFar)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_CAMERA_FAR;
	xAction.m_afArgs[0] = fFar;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetCameraAspect(float fAspect)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_CAMERA_ASPECT;
	xAction.m_afArgs[0] = fAspect;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetAsMainCamera()
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_MAIN_CAMERA;
	s_axActions.PushBack(xAction);
}

// -- Transform --

void Zenith_EditorAutomation::AddStep_SetTransformPosition(float fX, float fY, float fZ)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_TRANSFORM_POSITION;
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	xAction.m_afArgs[2] = fZ;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetTransformScale(float fX, float fY, float fZ)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_TRANSFORM_SCALE;
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	xAction.m_afArgs[2] = fZ;
	s_axActions.PushBack(xAction);
}

// -- UI --

void Zenith_EditorAutomation::AddStep_CreateUIText(const char* szName, const char* szText)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::CREATE_UI_TEXT;
	xAction.m_szArg1 = szName;
	xAction.m_szArg2 = szText;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_CreateUIButton(const char* szName, const char* szText)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::CREATE_UI_BUTTON;
	xAction.m_szArg1 = szName;
	xAction.m_szArg2 = szText;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_CreateUIRect(const char* szName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::CREATE_UI_RECT;
	xAction.m_szArg1 = szName;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetUIAnchor(const char* szElement, int iPreset)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_ANCHOR;
	xAction.m_szArg1 = szElement;
	xAction.m_aiArgs[0] = iPreset;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetUIPosition(const char* szElement, float fX, float fY)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_POSITION;
	xAction.m_szArg1 = szElement;
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetUISize(const char* szElement, float fW, float fH)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_SIZE;
	xAction.m_szArg1 = szElement;
	xAction.m_afArgs[0] = fW;
	xAction.m_afArgs[1] = fH;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetUIFontSize(const char* szElement, float fSize)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_FONT_SIZE;
	xAction.m_szArg1 = szElement;
	xAction.m_afArgs[0] = fSize;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetUIColor(const char* szElement, float fR, float fG, float fB, float fA)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_COLOR;
	xAction.m_szArg1 = szElement;
	xAction.m_afArgs[0] = fR;
	xAction.m_afArgs[1] = fG;
	xAction.m_afArgs[2] = fB;
	xAction.m_afArgs[3] = fA;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetUIAlignment(const char* szElement, int iAlignment)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_ALIGNMENT;
	xAction.m_szArg1 = szElement;
	xAction.m_aiArgs[0] = iAlignment;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetUIVisible(const char* szElement, bool bVisible)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_VISIBLE;
	xAction.m_szArg1 = szElement;
	xAction.m_bArg = bVisible;
	s_axActions.PushBack(xAction);
}

// -- UI Button --

void Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor(const char* szElement, float fR, float fG, float fB, float fA)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_BUTTON_NORMAL_COLOR;
	xAction.m_szArg1 = szElement;
	xAction.m_afArgs[0] = fR;
	xAction.m_afArgs[1] = fG;
	xAction.m_afArgs[2] = fB;
	xAction.m_afArgs[3] = fA;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor(const char* szElement, float fR, float fG, float fB, float fA)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_BUTTON_HOVER_COLOR;
	xAction.m_szArg1 = szElement;
	xAction.m_afArgs[0] = fR;
	xAction.m_afArgs[1] = fG;
	xAction.m_afArgs[2] = fB;
	xAction.m_afArgs[3] = fA;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor(const char* szElement, float fR, float fG, float fB, float fA)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_BUTTON_PRESSED_COLOR;
	xAction.m_szArg1 = szElement;
	xAction.m_afArgs[0] = fR;
	xAction.m_afArgs[1] = fG;
	xAction.m_afArgs[2] = fB;
	xAction.m_afArgs[3] = fA;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetUIButtonFontSize(const char* szElement, float fSize)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_BUTTON_FONT_SIZE;
	xAction.m_szArg1 = szElement;
	xAction.m_afArgs[0] = fSize;
	s_axActions.PushBack(xAction);
}

// -- Script --

void Zenith_EditorAutomation::AddStep_SetBehaviour(const char* szBehaviourName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_BEHAVIOUR;
	xAction.m_szArg1 = szBehaviourName;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization(const char* szBehaviourName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_BEHAVIOUR_FOR_SERIALIZATION;
	xAction.m_szArg1 = szBehaviourName;
	s_axActions.PushBack(xAction);
}

// -- Particles --

void Zenith_EditorAutomation::AddStep_SetParticleConfig(Flux_ParticleEmitterConfig* pxConfig)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_PARTICLE_CONFIG;
	xAction.m_pArg = pxConfig;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetParticleEmitting(bool bEmitting)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_PARTICLE_EMITTING;
	xAction.m_bArg = bEmitting;
	s_axActions.PushBack(xAction);
}

// -- Collider --

void Zenith_EditorAutomation::AddStep_AddColliderShape(int iVolumeType, int iBodyType)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::ADD_COLLIDER_SHAPE;
	xAction.m_aiArgs[0] = iVolumeType;
	xAction.m_aiArgs[1] = iBodyType;
	s_axActions.PushBack(xAction);
}

// -- Model --

void Zenith_EditorAutomation::AddStep_AddMeshEntry(Flux_MeshGeometry* pxGeometry, Zenith_MaterialAsset* pxMaterial)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::ADD_MESH_ENTRY;
	xAction.m_pArg = pxGeometry;
	xAction.m_pArg2 = pxMaterial;
	s_axActions.PushBack(xAction);
}

// -- Scene Loading --

void Zenith_EditorAutomation::AddStep_SetLoadingScene(bool bLoading)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_LOADING_SCENE;
	xAction.m_bArg = bLoading;
	s_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_SetInitialSceneLoadCallback(void (*pfnCallback)())
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_INITIAL_SCENE_LOAD_CALLBACK;
	xAction.m_pfnFunc = pfnCallback;
	s_axActions.PushBack(xAction);
}

// -- Custom --

void Zenith_EditorAutomation::AddStep_Custom(void (*pfnFunc)())
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::CUSTOM_STEP;
	xAction.m_pfnFunc = pfnFunc;
	s_axActions.PushBack(xAction);
}

//=============================================================================
// Action Execution
//=============================================================================

void Zenith_EditorAutomation::ExecuteAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	//--------------------------------------------------------------------------
	// Scene operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_SCENE:
		Zenith_Editor::CreateNewScene(xAction.m_szArg1);
		break;

	case Zenith_EditorActionType::SAVE_SCENE:
		Zenith_Editor::SaveActiveScene(xAction.m_szArg1);
		break;

	case Zenith_EditorActionType::UNLOAD_SCENE:
		Zenith_Editor::UnloadActiveScene();
		break;

	//--------------------------------------------------------------------------
	// Entity operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_ENTITY:
		Zenith_Editor::CreateEntity(xAction.m_szArg1);
		break;

	case Zenith_EditorActionType::SELECT_ENTITY:
		Zenith_Editor::SelectEntityByName(xAction.m_szArg1);
		break;

	case Zenith_EditorActionType::SET_ENTITY_TRANSIENT:
		Zenith_Editor::SetSelectedEntityTransient(xAction.m_bArg);
		break;

	//--------------------------------------------------------------------------
	// Component operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::ADD_COMPONENT:
		Zenith_Editor::AddComponentToSelected(xAction.m_szArg1);
		break;

	//--------------------------------------------------------------------------
	// Camera field edits
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_CAMERA_POSITION:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_POSITION");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetPosition(
			{xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]});
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_PITCH:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_PITCH");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetPitch(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_YAW:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_YAW");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetYaw(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_FOV:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_FOV");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetFOV(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_NEAR:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_NEAR");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetNearPlane(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_FAR:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_FAR");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetFarPlane(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_ASPECT:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_ASPECT");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetAspectRatio(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_MAIN_CAMERA:
		Zenith_Editor::SetSelectedAsMainCamera();
		break;

	//--------------------------------------------------------------------------
	// Transform field edits
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_TRANSFORM_POSITION:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_TRANSFORM_POSITION");
		pxEntity->GetComponent<Zenith_TransformComponent>().SetPosition(
			{xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]});
		break;
	}

	case Zenith_EditorActionType::SET_TRANSFORM_SCALE:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_TRANSFORM_SCALE");
		pxEntity->GetComponent<Zenith_TransformComponent>().SetScale(
			{xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]});
		break;
	}

	//--------------------------------------------------------------------------
	// UI element creation and field edits
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_UI_TEXT:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_TEXT");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateText(xAction.m_szArg1, xAction.m_szArg2);
		break;
	}

	case Zenith_EditorActionType::CREATE_UI_BUTTON:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_BUTTON");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateButton(xAction.m_szArg1, xAction.m_szArg2);
		break;
	}

	case Zenith_EditorActionType::CREATE_UI_RECT:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_RECT");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateRect(xAction.m_szArg1);
		break;
	}

	case Zenith_EditorActionType::SET_UI_ANCHOR:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_ANCHOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetAnchorAndPivot(static_cast<Zenith_UI::AnchorPreset>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_POSITION:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_POSITION");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetPosition(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_SIZE:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetSize(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_FONT_SIZE:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_FONT_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(xAction.m_szArg1);
		Zenith_Assert(pxText, "UI text element not found: %s", xAction.m_szArg1);
		pxText->SetFontSize(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_COLOR:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetColor(Zenith_Maths::Vector4(
			xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_ALIGNMENT:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_ALIGNMENT");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(xAction.m_szArg1);
		Zenith_Assert(pxText, "UI text element not found: %s", xAction.m_szArg1);
		pxText->SetAlignment(static_cast<Zenith_UI::TextAlignment>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_VISIBLE:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_VISIBLE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetVisible(xAction.m_bArg);
		break;
	}

	//--------------------------------------------------------------------------
	// UI button field edits
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_UI_BUTTON_NORMAL_COLOR:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_NORMAL_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetNormalColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_HOVER_COLOR:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_HOVER_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetHoverColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_PRESSED_COLOR:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_PRESSED_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetPressedColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_FONT_SIZE:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_FONT_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetFontSize(xAction.m_afArgs[0]);
		break;
	}

	//--------------------------------------------------------------------------
	// Script operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_BEHAVIOUR:
		Zenith_Editor::SetBehaviourOnSelected(xAction.m_szArg1);
		break;

	case Zenith_EditorActionType::SET_BEHAVIOUR_FOR_SERIALIZATION:
		Zenith_Editor::SetBehaviourForSerializationOnSelected(xAction.m_szArg1);
		break;

	//--------------------------------------------------------------------------
	// Particle operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_PARTICLE_CONFIG:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_PARTICLE_CONFIG");
		Zenith_Assert(pxEntity->HasComponent<Zenith_ParticleEmitterComponent>(), "Selected entity has no ParticleEmitterComponent");
		pxEntity->GetComponent<Zenith_ParticleEmitterComponent>().SetConfig(
			static_cast<Flux_ParticleEmitterConfig*>(xAction.m_pArg));
		break;
	}

	case Zenith_EditorActionType::SET_PARTICLE_EMITTING:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_PARTICLE_EMITTING");
		Zenith_Assert(pxEntity->HasComponent<Zenith_ParticleEmitterComponent>(), "Selected entity has no ParticleEmitterComponent");
		pxEntity->GetComponent<Zenith_ParticleEmitterComponent>().SetEmitting(xAction.m_bArg);
		break;
	}

	//--------------------------------------------------------------------------
	// Collider operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::ADD_COLLIDER_SHAPE:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for ADD_COLLIDER_SHAPE");
		Zenith_Assert(pxEntity->HasComponent<Zenith_ColliderComponent>(), "Selected entity has no ColliderComponent");
		pxEntity->GetComponent<Zenith_ColliderComponent>().AddCollider(
			static_cast<CollisionVolumeType>(xAction.m_aiArgs[0]),
			static_cast<RigidBodyType>(xAction.m_aiArgs[1]));
		break;
	}

	//--------------------------------------------------------------------------
	// Model operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::ADD_MESH_ENTRY:
	{
		Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for ADD_MESH_ENTRY");
		Zenith_Assert(pxEntity->HasComponent<Zenith_ModelComponent>(), "Selected entity has no ModelComponent");
		Flux_MeshGeometry* pxGeometry = static_cast<Flux_MeshGeometry*>(xAction.m_pArg);
		Zenith_MaterialAsset* pxMaterial = static_cast<Zenith_MaterialAsset*>(xAction.m_pArg2);
		Zenith_Assert(pxGeometry, "Null geometry for ADD_MESH_ENTRY");
		Zenith_Assert(pxMaterial, "Null material for ADD_MESH_ENTRY");
		pxEntity->GetComponent<Zenith_ModelComponent>().AddMeshEntry(*pxGeometry, *pxMaterial);
		break;
	}

	//--------------------------------------------------------------------------
	// Scene loading operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_LOADING_SCENE:
		Zenith_SceneManager::SetLoadingScene(xAction.m_bArg);
		break;

	case Zenith_EditorActionType::SET_INITIAL_SCENE_LOAD_CALLBACK:
	{
		Zenith_SceneManager::SetInitialSceneLoadCallback(xAction.m_pfnFunc);
		break;
	}

	//--------------------------------------------------------------------------
	// Custom step
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CUSTOM_STEP:
	{
		Zenith_Assert(xAction.m_pfnFunc, "Null function pointer for CUSTOM_STEP");
		xAction.m_pfnFunc();
		break;
	}

	default:
		Zenith_Assert(false, "Unknown Zenith_EditorActionType: %d", static_cast<int>(xAction.m_eType));
		break;
	}
}

#endif // ZENITH_TOOLS
