#include "Zenith.h"
#ifdef ZENITH_TOOLS

#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "UI/Zenith_UI.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Prefab/Zenith_Prefab.h"
#include "Maths/Zenith_Maths.h"
#include "DataStream/Zenith_DataStream.h"

// Phase 5.5d: automation state lives on Zenith_EditorAutomation
// held by Zenith_Engine.

bool Zenith_EditorAutomation::IsRunning()  { return Zenith_EditorAutomation::m_bRunning; }
bool Zenith_EditorAutomation::IsComplete() { return Zenith_EditorAutomation::m_bComplete; }

//=============================================================================
// Execution
//=============================================================================

void Zenith_EditorAutomation::Begin()
{
	g_xEngine.EditorAutomation().m_uCurrentAction = 0;
	g_xEngine.EditorAutomation().m_bRunning = true;
	g_xEngine.EditorAutomation().m_bComplete = false;
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Begin: %u steps queued", g_xEngine.EditorAutomation().m_axActions.GetSize());
}

void Zenith_EditorAutomation::ExecuteNextStep()
{
	if (!g_xEngine.EditorAutomation().m_bRunning || g_xEngine.EditorAutomation().m_bComplete)
		return;

	if (g_xEngine.EditorAutomation().m_uCurrentAction >= g_xEngine.EditorAutomation().m_axActions.GetSize())
	{
		g_xEngine.EditorAutomation().m_bRunning = false;
		g_xEngine.EditorAutomation().m_bComplete = true;
		Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Complete: all %u steps executed", g_xEngine.EditorAutomation().m_axActions.GetSize());
		g_xEngine.EditorAutomation().m_axActions.Clear();
		return;
	}

	const Zenith_EditorAction& xAction = g_xEngine.EditorAutomation().m_axActions.Get(g_xEngine.EditorAutomation().m_uCurrentAction);
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Step %u/%u", g_xEngine.EditorAutomation().m_uCurrentAction + 1, g_xEngine.EditorAutomation().m_axActions.GetSize());

	ExecuteAction(xAction);
	g_xEngine.EditorAutomation().m_uCurrentAction++;

	// Detect completion immediately after executing the last step
	if (g_xEngine.EditorAutomation().m_uCurrentAction >= g_xEngine.EditorAutomation().m_axActions.GetSize())
	{
		g_xEngine.EditorAutomation().m_bRunning = false;
		g_xEngine.EditorAutomation().m_bComplete = true;
		Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Complete: all %u steps executed", g_xEngine.EditorAutomation().m_axActions.GetSize());
		g_xEngine.EditorAutomation().m_axActions.Clear();
	}
}

void Zenith_EditorAutomation::Reset()
{
	g_xEngine.EditorAutomation().m_axActions.Clear();
	g_xEngine.EditorAutomation().m_uCurrentAction = 0;
	g_xEngine.EditorAutomation().m_bRunning = false;
	g_xEngine.EditorAutomation().m_bComplete = false;
}

//=============================================================================
// Step Helpers
//=============================================================================

// File-local builder overloads that turn a Zenith_EditorAction construction
// plus PushBack into a single call. Each AddStep_* below collapses from
// five or six lines of boilerplate to one. A new AddStep that doesn't match
// any overload can still fall back to constructing the struct inline (see
// AddStep_SetUINavigation for the 5-string special case).
namespace
{
	using ActionType = Zenith_EditorActionType;
	using ActionList = Zenith_Vector<Zenith_EditorAction>;

	inline void Push(ActionList& xActions, ActionType eType)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz1, const char* sz2)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz1;
		xAction.m_szArg2 = sz2;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, bool b)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz;
		xAction.m_bArg = b;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, int i)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz;
		xAction.m_aiArgs[0] = i;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz;
		xAction.m_afArgs[0] = f;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f1, float f2)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz;
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f1, float f2, float f3)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz;
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xAction.m_afArgs[2] = f3;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f1, float f2, float f3, float f4)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz;
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xAction.m_afArgs[2] = f3;
		xAction.m_afArgs[3] = f4;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f1, float f2, bool b)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz;
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xAction.m_bArg = b;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f1, float f2, float f3, bool b)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz;
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xAction.m_afArgs[2] = f3;
		xAction.m_bArg = b;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, bool b1, bool b2)
	{
		// Two bools packed into m_aiArgs[0], m_aiArgs[1] (as 0/1). Used for
		// SetUILayoutChildForceExpand(width, height) only.
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = sz;
		xAction.m_aiArgs[0] = b1 ? 1 : 0;
		xAction.m_aiArgs[1] = b2 ? 1 : 0;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, bool b)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_bArg = b;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, float f)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_afArgs[0] = f;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, float f1, float f2, float f3)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xAction.m_afArgs[2] = f3;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, int i1, int i2)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_aiArgs[0] = i1;
		xAction.m_aiArgs[1] = i2;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, void* pArg)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_pArg = pArg;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, void* pArg, void* pArg2)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_pArg = pArg;
		xAction.m_pArg2 = pArg2;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, int i, void* pArg)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_aiArgs[0] = i;
		xAction.m_pArg = pArg;
		xActions.PushBack(xAction);
	}
}

// -- Scene --

void Zenith_EditorAutomation::AddStep_CreateScene(const char* szName) { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_SCENE, szName); }
void Zenith_EditorAutomation::AddStep_SaveScene(const char* szPath)   { Push(Zenith_EditorAutomation::m_axActions, ActionType::SAVE_SCENE, szPath); }
void Zenith_EditorAutomation::AddStep_UnloadScene()                   { Push(Zenith_EditorAutomation::m_axActions, ActionType::UNLOAD_SCENE); }

// -- Entity --

void Zenith_EditorAutomation::AddStep_CreateEntity(const char* szName)       { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_ENTITY, szName); }
void Zenith_EditorAutomation::AddStep_SelectEntity(const char* szName)       { Push(Zenith_EditorAutomation::m_axActions, ActionType::SELECT_ENTITY, szName); }
void Zenith_EditorAutomation::AddStep_SetEntityTransient(bool bTransient)    { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_ENTITY_TRANSIENT, bTransient); }

// -- Component --

void Zenith_EditorAutomation::AddStep_AddComponent(const char* szDisplayName) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ADD_COMPONENT, szDisplayName); }

// -- Camera --

void Zenith_EditorAutomation::AddStep_SetCameraPosition(float fX, float fY, float fZ) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_CAMERA_POSITION, fX, fY, fZ); }
void Zenith_EditorAutomation::AddStep_SetCameraPitch (float fPitch)   { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_CAMERA_PITCH,  fPitch); }
void Zenith_EditorAutomation::AddStep_SetCameraYaw   (float fYaw)     { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_CAMERA_YAW,    fYaw); }
void Zenith_EditorAutomation::AddStep_SetCameraFOV   (float fFOV)     { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_CAMERA_FOV,    fFOV); }
void Zenith_EditorAutomation::AddStep_SetCameraNear  (float fNear)    { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_CAMERA_NEAR,   fNear); }
void Zenith_EditorAutomation::AddStep_SetCameraFar   (float fFar)     { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_CAMERA_FAR,    fFar); }
void Zenith_EditorAutomation::AddStep_SetCameraAspect(float fAspect)  { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_CAMERA_ASPECT, fAspect); }
void Zenith_EditorAutomation::AddStep_SetAsMainCamera()               { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_MAIN_CAMERA); }

// -- Transform --

void Zenith_EditorAutomation::AddStep_SetTransformPosition(float fX, float fY, float fZ) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_TRANSFORM_POSITION, fX, fY, fZ); }
void Zenith_EditorAutomation::AddStep_SetTransformScale   (float fX, float fY, float fZ) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_TRANSFORM_SCALE,    fX, fY, fZ); }
void Zenith_EditorAutomation::AddStep_SetTransformYaw     (float fYawRadians)             { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_TRANSFORM_ROTATION_YAW, fYawRadians); }

// -- Light --

void Zenith_EditorAutomation::AddStep_SetLightIntensity(float fLumens)              { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_LIGHT_INTENSITY, fLumens); }
void Zenith_EditorAutomation::AddStep_SetLightRange    (float fMetres)              { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_LIGHT_RANGE,     fMetres); }
void Zenith_EditorAutomation::AddStep_SetLightColor    (float fR, float fG, float fB) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_LIGHT_COLOR, fR, fG, fB); }

// -- UI --

void Zenith_EditorAutomation::AddStep_CreateUIText         (const char* szName, const char* szText)          { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_TEXT,            szName, szText); }
void Zenith_EditorAutomation::AddStep_CreateUIButton       (const char* szName, const char* szText)          { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_BUTTON,          szName, szText); }
void Zenith_EditorAutomation::AddStep_CreateUIRect         (const char* szName)                              { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_RECT,            szName); }
void Zenith_EditorAutomation::AddStep_CreateUIImage        (const char* szName)                              { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_IMAGE,           szName); }
void Zenith_EditorAutomation::AddStep_SetUIImageTexturePath(const char* szElement, const char* szTexturePath){ Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_IMAGE_TEXTURE_PATH, szElement, szTexturePath); }
void Zenith_EditorAutomation::AddStep_SetUIAnchor          (const char* szElement, int iPreset)              { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_ANCHOR,             szElement, iPreset); }
void Zenith_EditorAutomation::AddStep_SetUIPosition        (const char* szElement, float fX, float fY)       { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_POSITION,           szElement, fX, fY); }
void Zenith_EditorAutomation::AddStep_SetUISize            (const char* szElement, float fW, float fH)       { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_SIZE,               szElement, fW, fH); }
void Zenith_EditorAutomation::AddStep_SetUIFontSize        (const char* szElement, float fSize)              { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_FONT_SIZE,          szElement, fSize); }
void Zenith_EditorAutomation::AddStep_SetUIColor           (const char* szElement, float fR, float fG, float fB, float fA) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_COLOR, szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIAlignment       (const char* szElement, int iAlignment)           { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_ALIGNMENT,          szElement, iAlignment); }
void Zenith_EditorAutomation::AddStep_SetUIVisible         (const char* szElement, bool bVisible)            { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_VISIBLE,            szElement, bVisible); }

// -- UI Layout Group --

void Zenith_EditorAutomation::AddStep_CreateUILayoutGroup        (const char* szName)                                            { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_LAYOUT_GROUP,           szName); }
void Zenith_EditorAutomation::AddStep_AddUIChild                 (const char* szParent, const char* szChild)                    { Push(Zenith_EditorAutomation::m_axActions, ActionType::ADD_UI_CHILD,                     szParent, szChild); }
void Zenith_EditorAutomation::AddStep_SetUILayoutDirection       (const char* szElement, int iDirection)                        { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_LAYOUT_DIRECTION,          szElement, iDirection); }
void Zenith_EditorAutomation::AddStep_SetUILayoutSpacing         (const char* szElement, float fSpacing)                        { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_LAYOUT_SPACING,            szElement, fSpacing); }
void Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment  (const char* szElement, int iAlignment)                        { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_LAYOUT_CHILD_ALIGNMENT,    szElement, iAlignment); }
void Zenith_EditorAutomation::AddStep_SetUILayoutPadding         (const char* szElement, float fL, float fT, float fR, float fB){ Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_LAYOUT_PADDING,            szElement, fL, fT, fR, fB); }
void Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent    (const char* szElement, bool bFit)                             { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_LAYOUT_FIT_TO_CONTENT,     szElement, bFit); }
void Zenith_EditorAutomation::AddStep_SetUILayoutChildForceExpand(const char* szElement, bool bWidth, bool bHeight)             { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_LAYOUT_CHILD_FORCE_EXPAND, szElement, bWidth, bHeight); }
void Zenith_EditorAutomation::AddStep_SetUILayoutReverse         (const char* szElement, bool bReverse)                         { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_LAYOUT_REVERSE,            szElement, bReverse); }

// -- UI Toggle --

void Zenith_EditorAutomation::AddStep_CreateUIToggle      (const char* szName, const char* szText)                      { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_TOGGLE,         szName, szText); }
void Zenith_EditorAutomation::AddStep_SetUIToggleOnColor  (const char* szElement, float fR, float fG, float fB, float fA) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_TOGGLE_ON_COLOR,  szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIToggleOffColor (const char* szElement, float fR, float fG, float fB, float fA) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_TOGGLE_OFF_COLOR, szElement, fR, fG, fB, fA); }

// -- UI Overlay --

void Zenith_EditorAutomation::AddStep_CreateUIOverlay       (const char* szName)                                            { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_OVERLAY,          szName); }
void Zenith_EditorAutomation::AddStep_SetUIOverlayDimColor  (const char* szElement, float fR, float fG, float fB, float fA){ Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_OVERLAY_DIM_COLOR,   szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIOverlayContentSize(const char* szElement, float fW, float fH)                   { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_OVERLAY_CONTENT_SIZE, szElement, fW, fH); }

// -- UI Focus Navigation --

void Zenith_EditorAutomation::AddStep_SetUINavigation(const char* szElement, const char* szUp, const char* szDown, const char* szLeft, const char* szRight)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::SET_UI_NAVIGATION;
	xAction.m_szArg1 = szElement;
	xAction.m_szArg2 = szUp;
	xAction.m_pArg = const_cast<void*>(static_cast<const void*>(szDown));
	xAction.m_pArg2 = const_cast<void*>(static_cast<const void*>(szLeft));
	xAction.m_pfnFunc = reinterpret_cast<void(*)()>(const_cast<char*>(szRight));
	g_xEngine.EditorAutomation().m_axActions.PushBack(xAction);
}

// -- UI ScrollView --

void Zenith_EditorAutomation::AddStep_CreateUIScrollView         (const char* szName)                                            { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_SCROLL_VIEW,             szName); }
void Zenith_EditorAutomation::AddStep_SetUIScrollViewContentSize (const char* szElement, float fW, float fH)                    { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_SCROLL_VIEW_CONTENT_SIZE,   szElement, fW, fH); }

// -- UI Button --

void Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor (const char* szElement, float fR, float fG, float fB, float fA)    { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_NORMAL_COLOR,  szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor  (const char* szElement, float fR, float fG, float fB, float fA)    { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_HOVER_COLOR,   szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor(const char* szElement, float fR, float fG, float fB, float fA)    { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_PRESSED_COLOR, szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIButtonFontSize    (const char* szElement, float fSize)                               { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_FONT_SIZE,     szElement, fSize); }
void Zenith_EditorAutomation::AddStep_SetUIButtonIcon        (const char* szElement, const char* szTexturePath)                 { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_ICON,          szElement, szTexturePath); }
void Zenith_EditorAutomation::AddStep_SetUIButtonIconSize    (const char* szElement, float fW, float fH)                        { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_ICON_SIZE,     szElement, fW, fH); }

void Zenith_EditorAutomation::AddStep_SetUIButtonIconPlacement(const char* szElement, int iPlacement)                         { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_ICON_PLACEMENT, szElement, iPlacement); }

// -- UIElement Background --

void Zenith_EditorAutomation::AddStep_SetUIBackgroundColor       (const char* szElement, float fR, float fG, float fB, float fA)        { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BACKGROUND_COLOR,         szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIBackgroundCornerRadius(const char* szElement, float fRadius)                                 { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BACKGROUND_CORNER_RADIUS, szElement, fRadius); }
void Zenith_EditorAutomation::AddStep_SetUIBackgroundBorder      (const char* szElement, float fR, float fG, float fB, float fThickness){ Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BACKGROUND_BORDER,        szElement, fR, fG, fB, fThickness); }

// -- UIRect Styling --

void Zenith_EditorAutomation::AddStep_SetUICornerRadius (const char* szElement, float fRadius)                                           { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_CORNER_RADIUS,  szElement, fRadius); }
void Zenith_EditorAutomation::AddStep_SetUIGradientColor(const char* szElement, float fR, float fG, float fB, float fA)                  { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_GRADIENT_COLOR, szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIShadow       (const char* szElement, float fOffX, float fOffY, float fSpread, bool bEnabled){ Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_SHADOW,        szElement, fOffX, fOffY, fSpread, bEnabled); }
void Zenith_EditorAutomation::AddStep_SetUIShadowColor  (const char* szElement, float fR, float fG, float fB, float fA)                  { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_SHADOW_COLOR,  szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIRectBorder   (const char* szElement, float fR, float fG, float fB, float fThickness)          { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_RECT_BORDER,   szElement, fR, fG, fB, fThickness); }

// -- UIText Shadow --

void Zenith_EditorAutomation::AddStep_SetUITextShadow     (const char* szElement, float fOffX, float fOffY, bool bEnabled)              { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_TEXT_SHADOW,       szElement, fOffX, fOffY, bEnabled); }
void Zenith_EditorAutomation::AddStep_SetUITextShadowColor(const char* szElement, float fR, float fG, float fB, float fA)                { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_TEXT_SHADOW_COLOR, szElement, fR, fG, fB, fA); }

// -- UIButton Styling --

void Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius   (const char* szElement, float fRadius)                                   { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_CORNER_RADIUS,    szElement, fRadius); }
void Zenith_EditorAutomation::AddStep_SetUIButtonShadow         (const char* szElement, float fOffX, float fOffY, float fSpread, bool bEnabled){ Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_SHADOW,    szElement, fOffX, fOffY, fSpread, bEnabled); }
void Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor    (const char* szElement, float fR, float fG, float fB, float fA)          { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_SHADOW_COLOR,     szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIButtonGradientColor  (const char* szElement, float fR, float fG, float fB, float fA)          { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_GRADIENT_COLOR,   szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor    (const char* szElement, float fR, float fG, float fB, float fA)          { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_BORDER_COLOR,     szElement, fR, fG, fB, fA); }
void Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness(const char* szElement, float fThickness)                                { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_BORDER_THICKNESS, szElement, fThickness); }

void Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration(const char* szElement, float fDuration)                            { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_TRANSITION_DURATION, szElement, fDuration); }
void Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow        (const char* szElement, float fOffX, float fOffY, bool bEnabled)  { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_TEXT_SHADOW,         szElement, fOffX, fOffY, bEnabled); }
void Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor   (const char* szElement, float fR, float fG, float fB, float fA)  { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_BUTTON_TEXT_SHADOW_COLOR,   szElement, fR, fG, fB, fA); }

// -- Script --

void Zenith_EditorAutomation::AddStep_AttachScript                (const char* szBehaviourTypeName) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ATTACH_SCRIPT, szBehaviourTypeName); }

// -- Particles --

void Zenith_EditorAutomation::AddStep_SetParticleConfig      (Flux_ParticleEmitterConfig* pxConfig) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_PARTICLE_CONFIG,         pxConfig); }
void Zenith_EditorAutomation::AddStep_SetParticleConfigByName(const char* szConfigName)              { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_PARTICLE_CONFIG_BY_NAME, szConfigName); }
void Zenith_EditorAutomation::AddStep_SetParticleEmitting    (bool bEmitting)                        { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_PARTICLE_EMITTING,       bEmitting); }

// -- Collider --

void Zenith_EditorAutomation::AddStep_AddColliderShape(int iVolumeType, int iBodyType) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ADD_COLLIDER_SHAPE, iVolumeType, iBodyType); }

// -- Model --

void Zenith_EditorAutomation::AddStep_AddMeshEntry(Flux_MeshGeometry* pxGeometry, Zenith_MaterialAsset* pxMaterial) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ADD_MESH_ENTRY, pxGeometry, pxMaterial); }
void Zenith_EditorAutomation::AddStep_LoadModel(const char* szPath)                                                { Push(Zenith_EditorAutomation::m_axActions, ActionType::LOAD_MODEL, szPath); }
void Zenith_EditorAutomation::AddStep_SetModelMaterial(int iIndex, Zenith_MaterialAsset* pxMaterial)               { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_MODEL_MATERIAL, iIndex, pxMaterial); }

// -- Terrain --

void Zenith_EditorAutomation::AddStep_SetTerrainMaterial(int iSlot, Zenith_MaterialAsset* pxMaterial) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_TERRAIN_MATERIAL, iSlot, pxMaterial); }
void Zenith_EditorAutomation::AddStep_SetTerrainSplatmapPath(const char* szPath)                      { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_TERRAIN_SPLATMAP_PATH, szPath); }

// -- Prefab Variant Authoring --

void Zenith_EditorAutomation::AddStep_CreatePrefabFromSelected(const char* szPrefabName, const char* szSavePath)
{
	Push(g_xEngine.EditorAutomation().m_axActions, ActionType::CREATE_PREFAB_FROM_SELECTED, szPrefabName, szSavePath);
}

void Zenith_EditorAutomation::AddStep_CreatePrefabVariant(
	const char* szVariantName,
	const char* szBasePath,
	const char* szSavePath)
{
	// CREATE_PREFAB_VARIANT needs THREE strings (name + base path + save path),
	// one more than the Push helpers cover. Stash the third in m_pArg as a
	// const char* — the action struct is purely declarative so this is safe
	// (m_pArg already serves variable-meaning string/pointer roles for other
	// step types).
	Zenith_EditorAction xAction = {};
	xAction.m_eType  = ActionType::CREATE_PREFAB_VARIANT;
	xAction.m_szArg1 = szVariantName;
	xAction.m_szArg2 = szBasePath;
	xAction.m_pArg   = const_cast<char*>(szSavePath);
	g_xEngine.EditorAutomation().m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AddPrefabVariantOverrideVec3(
	const char* szPrefabPath,
	const char* szComponentName,
	const char* szPropertyName,
	float fX, float fY, float fZ)
{
	// Uses both static-storage strings and m_pArg (property name) — same
	// pattern as CREATE_PREFAB_VARIANT — plus the float triple.
	Zenith_EditorAction xAction = {};
	xAction.m_eType     = ActionType::ADD_PREFAB_VARIANT_OVERRIDE_VEC3;
	xAction.m_szArg1    = szPrefabPath;
	xAction.m_szArg2    = szComponentName;
	xAction.m_pArg      = const_cast<char*>(szPropertyName);
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	xAction.m_afArgs[2] = fZ;
	g_xEngine.EditorAutomation().m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_InstantiatePrefab(const char* szPrefabPath, const char* szEntityName,
	float fPosX, float fPosY, float fPosZ,
	float fRotW, float fRotX, float fRotY, float fRotZ,
	float fScaleX, float fScaleY, float fScaleZ)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::INSTANTIATE_PREFAB;
	xAction.m_szArg1 = szPrefabPath;
	xAction.m_szArg2 = szEntityName;
	// pos[0..2], quat[3..6] (wxyz), scale[7..9] — see INSTANTIATE_PREFAB executor.
	xAction.m_afArgs[0] = fPosX;   xAction.m_afArgs[1] = fPosY;   xAction.m_afArgs[2] = fPosZ;
	xAction.m_afArgs[3] = fRotW;   xAction.m_afArgs[4] = fRotX;   xAction.m_afArgs[5] = fRotY;   xAction.m_afArgs[6] = fRotZ;
	xAction.m_afArgs[7] = fScaleX; xAction.m_afArgs[8] = fScaleY; xAction.m_afArgs[9] = fScaleZ;
	g_xEngine.EditorAutomation().m_axActions.PushBack(xAction);
}

// -- Scene Loading --

void Zenith_EditorAutomation::AddStep_LoadInitialScene(void (*pfnCallback)())
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::LOAD_INITIAL_SCENE;
	xAction.m_pfnFunc = pfnCallback;
	g_xEngine.EditorAutomation().m_axActions.PushBack(xAction);
}

// -- Custom --

void Zenith_EditorAutomation::AddStep_Custom(void (*pfnFunc)())
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::CUSTOM_STEP;
	xAction.m_pfnFunc = pfnFunc;
	g_xEngine.EditorAutomation().m_axActions.PushBack(xAction);
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
		g_xEngine.Editor().CreateNewScene(xAction.m_szArg1);
		break;

	case Zenith_EditorActionType::SAVE_SCENE:
		g_xEngine.Editor().SaveActiveScene(xAction.m_szArg1);
		break;

	case Zenith_EditorActionType::UNLOAD_SCENE:
		g_xEngine.Editor().UnloadActiveScene();
		break;

	//--------------------------------------------------------------------------
	// Entity operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_ENTITY:
		g_xEngine.Editor().CreateEntity(xAction.m_szArg1);
		break;

	case Zenith_EditorActionType::SELECT_ENTITY:
		g_xEngine.Editor().SelectEntityByName(xAction.m_szArg1);
		break;

	case Zenith_EditorActionType::SET_ENTITY_TRANSIENT:
		g_xEngine.Editor().SetSelectedEntityTransient(xAction.m_bArg);
		break;

	//--------------------------------------------------------------------------
	// Component operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::ADD_COMPONENT:
		g_xEngine.Editor().AddComponentToSelected(xAction.m_szArg1);
		break;

	//--------------------------------------------------------------------------
	// Camera field edits
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_CAMERA_POSITION:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_POSITION");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetPosition(
			{xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]});
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_PITCH:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_PITCH");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetPitch(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_YAW:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_YAW");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetYaw(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_FOV:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_FOV");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetFOV(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_NEAR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_NEAR");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetNearPlane(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_FAR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_FAR");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetFarPlane(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_CAMERA_ASPECT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_CAMERA_ASPECT");
		pxEntity->GetComponent<Zenith_CameraComponent>().SetAspectRatio(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_MAIN_CAMERA:
		g_xEngine.Editor().SetSelectedAsMainCamera();
		break;

	//--------------------------------------------------------------------------
	// Transform field edits
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_TRANSFORM_POSITION:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_TRANSFORM_POSITION");
		pxEntity->GetComponent<Zenith_TransformComponent>().SetPosition(
			{xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]});
		break;
	}

	case Zenith_EditorActionType::SET_TRANSFORM_SCALE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_TRANSFORM_SCALE");
		pxEntity->GetComponent<Zenith_TransformComponent>().SetScale(
			{xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]});
		break;
	}

	case Zenith_EditorActionType::SET_TRANSFORM_ROTATION_YAW:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_TRANSFORM_ROTATION_YAW");
		const float fYaw = xAction.m_afArgs[0];
		const Zenith_Maths::Quat xRot = glm::angleAxis(
			fYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		pxEntity->GetComponent<Zenith_TransformComponent>().SetRotation(xRot);
		break;
	}

	case Zenith_EditorActionType::SET_LIGHT_INTENSITY:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_LIGHT_INTENSITY");
		Zenith_Assert(pxEntity->HasComponent<Zenith_LightComponent>(),
			"SET_LIGHT_INTENSITY: selected entity has no LightComponent");
		pxEntity->GetComponent<Zenith_LightComponent>().SetIntensity(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_LIGHT_RANGE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_LIGHT_RANGE");
		Zenith_Assert(pxEntity->HasComponent<Zenith_LightComponent>(),
			"SET_LIGHT_RANGE: selected entity has no LightComponent");
		pxEntity->GetComponent<Zenith_LightComponent>().SetRange(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_LIGHT_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_LIGHT_COLOR");
		Zenith_Assert(pxEntity->HasComponent<Zenith_LightComponent>(),
			"SET_LIGHT_COLOR: selected entity has no LightComponent");
		pxEntity->GetComponent<Zenith_LightComponent>().SetColor(
			Zenith_Maths::Vector3(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]));
		break;
	}

	//--------------------------------------------------------------------------
	// UI element creation and field edits
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_UI_TEXT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_TEXT");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateText(xAction.m_szArg1, xAction.m_szArg2);
		break;
	}

	case Zenith_EditorActionType::CREATE_UI_BUTTON:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_BUTTON");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateButton(xAction.m_szArg1, xAction.m_szArg2);
		break;
	}

	case Zenith_EditorActionType::CREATE_UI_RECT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_RECT");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateRect(xAction.m_szArg1);
		break;
	}

	case Zenith_EditorActionType::CREATE_UI_IMAGE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_IMAGE");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateImage(xAction.m_szArg1);
		break;
	}

	case Zenith_EditorActionType::SET_UI_IMAGE_TEXTURE_PATH:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_IMAGE_TEXTURE_PATH");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIImage* pxImage = xUI.FindElement<Zenith_UI::Zenith_UIImage>(xAction.m_szArg1);
		Zenith_Assert(pxImage, "UI image not found: %s", xAction.m_szArg1);
		pxImage->SetTexturePath(xAction.m_szArg2);
		break;
	}

	case Zenith_EditorActionType::SET_UI_ANCHOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_ANCHOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetAnchorAndPivot(static_cast<Zenith_UI::AnchorPreset>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_POSITION:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_POSITION");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetPosition(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetSize(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_FONT_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_FONT_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(xAction.m_szArg1);
		Zenith_Assert(pxText, "UI text element not found: %s", xAction.m_szArg1);
		pxText->SetFontSize(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_ALIGNMENT");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(xAction.m_szArg1);
		Zenith_Assert(pxText, "UI text element not found: %s", xAction.m_szArg1);
		pxText->SetAlignment(static_cast<Zenith_UI::TextAlignment>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_VISIBLE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_VISIBLE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetVisible(xAction.m_bArg);
		break;
	}

	//--------------------------------------------------------------------------
	// UI layout group operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_UI_LAYOUT_GROUP:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_LAYOUT_GROUP");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateLayoutGroup(xAction.m_szArg1);
		break;
	}

	case Zenith_EditorActionType::ADD_UI_CHILD:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for ADD_UI_CHILD");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxParent = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxParent, "UI parent element not found: %s", xAction.m_szArg1);
		Zenith_UI::Zenith_UIElement* pxChild = xUI.FindElement(xAction.m_szArg2);
		Zenith_Assert(pxChild, "UI child element not found: %s", xAction.m_szArg2);
		xUI.GetCanvas().ReparentElement(pxChild, pxParent);
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_DIRECTION:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_DIRECTION");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1);
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1);
		pxLayout->SetDirection(static_cast<Zenith_UI::LayoutDirection>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_SPACING:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_SPACING");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1);
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1);
		pxLayout->SetSpacing(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_CHILD_ALIGNMENT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_CHILD_ALIGNMENT");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1);
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1);
		pxLayout->SetChildAlignment(static_cast<Zenith_UI::ChildAlignment>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_PADDING:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_PADDING");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1);
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1);
		pxLayout->SetPadding(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_FIT_TO_CONTENT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_FIT_TO_CONTENT");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1);
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1);
		pxLayout->SetFitToContent(xAction.m_bArg);
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_CHILD_FORCE_EXPAND:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_CHILD_FORCE_EXPAND");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1);
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1);
		pxLayout->SetChildForceExpandWidth(xAction.m_aiArgs[0] != 0);
		pxLayout->SetChildForceExpandHeight(xAction.m_aiArgs[1] != 0);
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_REVERSE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_REVERSE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1);
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1);
		pxLayout->SetReverseArrangement(xAction.m_bArg);
		break;
	}

	//--------------------------------------------------------------------------
	// UI toggle
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_UI_TOGGLE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_TOGGLE");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateToggle(xAction.m_szArg1, xAction.m_szArg2);
		break;
	}

	case Zenith_EditorActionType::SET_UI_TOGGLE_ON_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_TOGGLE_ON_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIToggle* pxToggle = xUI.FindElement<Zenith_UI::Zenith_UIToggle>(xAction.m_szArg1);
		Zenith_Assert(pxToggle, "UI toggle not found: %s", xAction.m_szArg1);
		pxToggle->SetOnColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_TOGGLE_OFF_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_TOGGLE_OFF_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIToggle* pxToggle = xUI.FindElement<Zenith_UI::Zenith_UIToggle>(xAction.m_szArg1);
		Zenith_Assert(pxToggle, "UI toggle not found: %s", xAction.m_szArg1);
		pxToggle->SetOffColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	//--------------------------------------------------------------------------
	// UI overlay
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_UI_OVERLAY:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_OVERLAY");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateOverlay(xAction.m_szArg1);
		break;
	}

	case Zenith_EditorActionType::SET_UI_OVERLAY_DIM_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_OVERLAY_DIM_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIOverlay* pxOverlay = xUI.FindElement<Zenith_UI::Zenith_UIOverlay>(xAction.m_szArg1);
		Zenith_Assert(pxOverlay, "UI overlay not found: %s", xAction.m_szArg1);
		pxOverlay->SetDimColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_OVERLAY_CONTENT_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_OVERLAY_CONTENT_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIOverlay* pxOverlay = xUI.FindElement<Zenith_UI::Zenith_UIOverlay>(xAction.m_szArg1);
		Zenith_Assert(pxOverlay, "UI overlay not found: %s", xAction.m_szArg1);
		pxOverlay->SetContentSize(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	//--------------------------------------------------------------------------
	// UI focus navigation
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_UI_NAVIGATION:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_NAVIGATION");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);

		const char* szUp = xAction.m_szArg2;
		const char* szDown = static_cast<const char*>(xAction.m_pArg);
		const char* szLeft = static_cast<const char*>(xAction.m_pArg2);
		const char* szRight = reinterpret_cast<const char*>(xAction.m_pfnFunc);

		Zenith_UI::Zenith_UIElement* pxUp = szUp ? xUI.FindElement(szUp) : nullptr;
		Zenith_UI::Zenith_UIElement* pxDown = szDown ? xUI.FindElement(szDown) : nullptr;
		Zenith_UI::Zenith_UIElement* pxLeft = szLeft ? xUI.FindElement(szLeft) : nullptr;
		Zenith_UI::Zenith_UIElement* pxRight = szRight ? xUI.FindElement(szRight) : nullptr;

		pxElement->SetNavigation(pxUp, pxDown, pxLeft, pxRight);
		break;
	}

	//--------------------------------------------------------------------------
	// UI scroll view
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_UI_SCROLL_VIEW:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_SCROLL_VIEW");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		xUI.CreateScrollView(xAction.m_szArg1);
		break;
	}

	case Zenith_EditorActionType::SET_UI_SCROLL_VIEW_CONTENT_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_SCROLL_VIEW_CONTENT_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIScrollView* pxScrollView = xUI.FindElement<Zenith_UI::Zenith_UIScrollView>(xAction.m_szArg1);
		Zenith_Assert(pxScrollView, "UI scroll view not found: %s", xAction.m_szArg1);
		pxScrollView->SetContentSize(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	//--------------------------------------------------------------------------
	// UI button field edits
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_UI_BUTTON_NORMAL_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_NORMAL_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetNormalColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_HOVER_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_HOVER_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetHoverColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_PRESSED_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_PRESSED_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetPressedColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_FONT_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_FONT_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetFontSize(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_ICON:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_ICON");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetIconTexturePath(xAction.m_szArg2);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_ICON_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_ICON_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetIconSize(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_ICON_PLACEMENT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_ICON_PLACEMENT");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetIconPlacement(static_cast<Zenith_UI::Zenith_UIButton::IconPlacement>(xAction.m_aiArgs[0]));
		break;
	}

	//--------------------------------------------------------------------------
	// UIElement background operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_UI_BACKGROUND_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BACKGROUND_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetBackgroundColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BACKGROUND_CORNER_RADIUS:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BACKGROUND_CORNER_RADIUS");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetBackgroundCornerRadius(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BACKGROUND_BORDER:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BACKGROUND_BORDER");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1);
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1);
		pxElement->SetBackgroundBorderColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], 1.0f});
		pxElement->SetBackgroundBorderThickness(xAction.m_afArgs[3]);
		break;
	}

	//--------------------------------------------------------------------------
	// UIRect styling operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_UI_CORNER_RADIUS:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_CORNER_RADIUS");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>(xAction.m_szArg1);
		Zenith_Assert(pxRect, "UI rect not found: %s", xAction.m_szArg1);
		pxRect->SetCornerRadius(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_GRADIENT_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_GRADIENT_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>(xAction.m_szArg1);
		Zenith_Assert(pxRect, "UI rect not found: %s", xAction.m_szArg1);
		pxRect->SetGradientColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_SHADOW:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_SHADOW");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>(xAction.m_szArg1);
		Zenith_Assert(pxRect, "UI rect not found: %s", xAction.m_szArg1);
		pxRect->SetShadowEnabled(xAction.m_bArg);
		pxRect->SetShadowOffset({xAction.m_afArgs[0], xAction.m_afArgs[1]});
		pxRect->SetShadowSpread(xAction.m_afArgs[2]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_SHADOW_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_SHADOW_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>(xAction.m_szArg1);
		Zenith_Assert(pxRect, "UI rect not found: %s", xAction.m_szArg1);
		pxRect->SetShadowColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_RECT_BORDER:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_RECT_BORDER");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>(xAction.m_szArg1);
		Zenith_Assert(pxRect, "UI rect not found: %s", xAction.m_szArg1);
		pxRect->SetBorderColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], 1.0f});
		pxRect->SetBorderThickness(xAction.m_afArgs[3]);
		break;
	}

	//--------------------------------------------------------------------------
	// UIText shadow operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_UI_TEXT_SHADOW:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_TEXT_SHADOW");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(xAction.m_szArg1);
		Zenith_Assert(pxText, "UI text not found: %s", xAction.m_szArg1);
		pxText->SetShadowEnabled(xAction.m_bArg);
		pxText->SetShadowOffset({xAction.m_afArgs[0], xAction.m_afArgs[1]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_TEXT_SHADOW_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_TEXT_SHADOW_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(xAction.m_szArg1);
		Zenith_Assert(pxText, "UI text not found: %s", xAction.m_szArg1);
		pxText->SetShadowColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	//--------------------------------------------------------------------------
	// UIButton styling operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_UI_BUTTON_CORNER_RADIUS:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_CORNER_RADIUS");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetCornerRadius(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_SHADOW:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_SHADOW");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetShadowEnabled(xAction.m_bArg);
		pxButton->SetShadowOffset({xAction.m_afArgs[0], xAction.m_afArgs[1]});
		pxButton->SetShadowSpread(xAction.m_afArgs[2]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_SHADOW_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_SHADOW_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetShadowColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_GRADIENT_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_GRADIENT_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetGradientColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_BORDER_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_BORDER_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetBorderColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_BORDER_THICKNESS:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_BORDER_THICKNESS");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetBorderThickness(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_TRANSITION_DURATION:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_TRANSITION_DURATION");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetTransitionDuration(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_TEXT_SHADOW:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_TEXT_SHADOW");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetTextShadowEnabled(xAction.m_bArg);
		pxButton->SetTextShadowOffset({xAction.m_afArgs[0], xAction.m_afArgs[1]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_TEXT_SHADOW_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_TEXT_SHADOW_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1);
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1);
		pxButton->SetTextShadowColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	//--------------------------------------------------------------------------
	// Script operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::ATTACH_SCRIPT:
		g_xEngine.Editor().AttachScriptForSerializationToSelected(xAction.m_szArg1);
		break;

	//--------------------------------------------------------------------------
	// Particle operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_PARTICLE_CONFIG:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_PARTICLE_CONFIG");
		Zenith_Assert(pxEntity->HasComponent<Zenith_ParticleEmitterComponent>(), "Selected entity has no ParticleEmitterComponent");
		pxEntity->GetComponent<Zenith_ParticleEmitterComponent>().SetConfig(
			static_cast<Flux_ParticleEmitterConfig*>(xAction.m_pArg));
		break;
	}

	case Zenith_EditorActionType::SET_PARTICLE_CONFIG_BY_NAME:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_PARTICLE_CONFIG_BY_NAME");
		Zenith_Assert(pxEntity->HasComponent<Zenith_ParticleEmitterComponent>(), "Selected entity has no ParticleEmitterComponent");
		Flux_ParticleEmitterConfig* pxConfig = Flux_ParticleEmitterConfig::Find(xAction.m_szArg1);
		Zenith_Assert(pxConfig, "Particle config not found: %s", xAction.m_szArg1);
		pxEntity->GetComponent<Zenith_ParticleEmitterComponent>().SetConfig(pxConfig);
		break;
	}

	case Zenith_EditorActionType::SET_PARTICLE_EMITTING:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for ADD_MESH_ENTRY");
		Zenith_Assert(pxEntity->HasComponent<Zenith_ModelComponent>(), "Selected entity has no ModelComponent");
		Flux_MeshGeometry* pxGeometry = static_cast<Flux_MeshGeometry*>(xAction.m_pArg);
		Zenith_MaterialAsset* pxMaterial = static_cast<Zenith_MaterialAsset*>(xAction.m_pArg2);
		Zenith_Assert(pxGeometry, "Null geometry for ADD_MESH_ENTRY");
		Zenith_Assert(pxMaterial, "Null material for ADD_MESH_ENTRY");
		pxEntity->GetComponent<Zenith_ModelComponent>().AddMeshEntry(*pxGeometry, *pxMaterial);
		break;
	}

	case Zenith_EditorActionType::LOAD_MODEL:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for LOAD_MODEL");
		Zenith_Assert(pxEntity->HasComponent<Zenith_ModelComponent>(), "Selected entity has no ModelComponent");
		Zenith_Assert(xAction.m_szArg1, "Null path for LOAD_MODEL");
		pxEntity->GetComponent<Zenith_ModelComponent>().LoadModel(xAction.m_szArg1);
		break;
	}

	case Zenith_EditorActionType::SET_MODEL_MATERIAL:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_MODEL_MATERIAL");
		Zenith_Assert(pxEntity->HasComponent<Zenith_ModelComponent>(), "Selected entity has no ModelComponent");
		Zenith_ModelComponent& xModel = pxEntity->GetComponent<Zenith_ModelComponent>();
		// Soften: missing model means the previous LOAD_MODEL silently failed
		// (file not found in CI checkouts where Assets/Meshes/ is .gitignore'd).
		// LOAD_MODEL logs an error and returns; downstream SET_MODEL_MATERIAL
		// used to assert here. Now we warn and skip so EditorAutomation can
		// continue and downstream state-only tests still run.
		if (!xModel.HasModel())
		{
			Zenith_Warning(LOG_CATEGORY_EDITOR,
				"SET_MODEL_MATERIAL skipped on entity %u: no model loaded "
				"(likely a missing .zmodel asset on this checkout)",
				static_cast<u_int>(pxEntity->GetEntityID().m_uIndex));
			break;
		}
		const int iIndex = xAction.m_aiArgs[0];
		Zenith_MaterialAsset* pxMaterial = static_cast<Zenith_MaterialAsset*>(xAction.m_pArg);
		Zenith_Assert(pxMaterial, "Null material for SET_MODEL_MATERIAL");
		Flux_ModelInstance* pxInstance = xModel.GetModelInstance();
		Zenith_Assert(pxInstance, "Null model instance for SET_MODEL_MATERIAL");
		Zenith_Assert(iIndex >= 0 && static_cast<uint32_t>(iIndex) < pxInstance->GetNumMaterials(),
			"SET_MODEL_MATERIAL slot %d out of range (model has %u materials)", iIndex, pxInstance->GetNumMaterials());
		pxInstance->SetMaterial(static_cast<uint32_t>(iIndex), pxMaterial);
		break;
	}

	//--------------------------------------------------------------------------
	// Terrain operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_TERRAIN_MATERIAL:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_TERRAIN_MATERIAL");
		Zenith_Assert(pxEntity->HasComponent<Zenith_TerrainComponent>(), "Selected entity has no TerrainComponent");
		const int iSlot = xAction.m_aiArgs[0];
		Zenith_MaterialAsset* pxMaterial = static_cast<Zenith_MaterialAsset*>(xAction.m_pArg);
		Zenith_Assert(iSlot >= 0 && iSlot < static_cast<int>(Zenith_TerrainComponent::TERRAIN_MATERIAL_COUNT),
			"SET_TERRAIN_MATERIAL slot %d out of range [0, %u)", iSlot, Zenith_TerrainComponent::TERRAIN_MATERIAL_COUNT);
		Zenith_Assert(pxMaterial, "Null material for SET_TERRAIN_MATERIAL");
		pxEntity->GetComponent<Zenith_TerrainComponent>().GetMaterialHandle(static_cast<u_int>(iSlot)).Set(pxMaterial);
		break;
	}

	case Zenith_EditorActionType::SET_TERRAIN_SPLATMAP_PATH:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_TERRAIN_SPLATMAP_PATH");
		Zenith_Assert(pxEntity->HasComponent<Zenith_TerrainComponent>(), "Selected entity has no TerrainComponent");
		Zenith_Assert(xAction.m_szArg1, "Null path for SET_TERRAIN_SPLATMAP_PATH");
		pxEntity->GetComponent<Zenith_TerrainComponent>().GetSplatmapHandle().SetPath(xAction.m_szArg1);
		break;
	}

	//--------------------------------------------------------------------------
	// Prefab variant authoring
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_PREFAB_FROM_SELECTED:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_PREFAB_FROM_SELECTED");
		Zenith_Assert(xAction.m_szArg1, "Null prefab name for CREATE_PREFAB_FROM_SELECTED");
		Zenith_Assert(xAction.m_szArg2, "Null save path for CREATE_PREFAB_FROM_SELECTED");

		Zenith_Prefab xPrefab;
		const bool bCreated = xPrefab.CreateFromEntity(*pxEntity, xAction.m_szArg1);
		Zenith_Assert(bCreated, "CreateFromEntity failed for '%s'", xAction.m_szArg1);
		const bool bSaved = xPrefab.SaveToFile(xAction.m_szArg2);
		Zenith_Assert(bSaved, "SaveToFile failed for '%s'", xAction.m_szArg2);

		// Force-cache through the registry so subsequent steps that look the
		// path up via PrefabHandle resolve cheaply (no disk re-read on every
		// CreateAsVariant cycle check).
		Zenith_AssetRegistry::Get<Zenith_Prefab>(xAction.m_szArg2);
		break;
	}

	case Zenith_EditorActionType::CREATE_PREFAB_VARIANT:
	{
		Zenith_Assert(xAction.m_szArg1, "Null variant name for CREATE_PREFAB_VARIANT");
		Zenith_Assert(xAction.m_szArg2, "Null base path for CREATE_PREFAB_VARIANT");
		const char* szSavePath = static_cast<const char*>(xAction.m_pArg);
		Zenith_Assert(szSavePath, "Null save path for CREATE_PREFAB_VARIANT");

		// Make sure the base prefab is loaded so PrefabHandle's cycle check
		// can resolve it. The cycle detector deliberately does NOT trigger a
		// disk load (see Zenith_Prefab::WouldFormVariantCycle) — we have to
		// prime the registry here.
		Zenith_AssetRegistry::Get<Zenith_Prefab>(xAction.m_szArg2);

		PrefabHandle xBaseHandle(xAction.m_szArg2);
		Zenith_Prefab xVariant;
		const bool bCreated = xVariant.CreateAsVariant(xBaseHandle, xAction.m_szArg1);
		Zenith_Assert(bCreated, "CreateAsVariant failed for '%s' (base '%s')",
			xAction.m_szArg1, xAction.m_szArg2);
		const bool bSaved = xVariant.SaveToFile(szSavePath);
		Zenith_Assert(bSaved, "SaveToFile failed for variant '%s' at '%s'",
			xAction.m_szArg1, szSavePath);

		Zenith_AssetRegistry::Get<Zenith_Prefab>(szSavePath);
		break;
	}

	case Zenith_EditorActionType::ADD_PREFAB_VARIANT_OVERRIDE_VEC3:
	{
		Zenith_Assert(xAction.m_szArg1, "Null prefab path for ADD_PREFAB_VARIANT_OVERRIDE_VEC3");
		Zenith_Assert(xAction.m_szArg2, "Null component name for ADD_PREFAB_VARIANT_OVERRIDE_VEC3");
		const char* szPropertyName = static_cast<const char*>(xAction.m_pArg);
		Zenith_Assert(szPropertyName, "Null property name for ADD_PREFAB_VARIANT_OVERRIDE_VEC3");

		// Modifying a prefab held by the registry is safe because Zenith_Prefab*
		// is the same pointer the registry caches — adding an override mutates
		// in-memory state, then SaveToFile rewrites the .zpfb. Callers that load
		// the file again get the updated overrides.
		Zenith_Prefab* pxPrefab = Zenith_AssetRegistry::Get<Zenith_Prefab>(xAction.m_szArg1);
		Zenith_Assert(pxPrefab, "Could not load prefab '%s' for override", xAction.m_szArg1);

		Zenith_PropertyOverride xOv;
		xOv.m_strComponentName = xAction.m_szArg2;
		xOv.m_strPropertyPath  = szPropertyName;
		xOv.m_xValue << Zenith_Maths::Vector3(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]);
		pxPrefab->AddOverride(std::move(xOv));

		const bool bSaved = pxPrefab->SaveToFile(xAction.m_szArg1);
		Zenith_Assert(bSaved, "SaveToFile failed after AddOverride for '%s'", xAction.m_szArg1);
		break;
	}

	case Zenith_EditorActionType::INSTANTIATE_PREFAB:
	{
		Zenith_Assert(xAction.m_szArg1, "Null prefab path for INSTANTIATE_PREFAB");

		Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_Assert(xActiveScene.IsValid(), "INSTANTIATE_PREFAB requires an active scene");
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
		Zenith_Assert(pxSceneData, "Active scene data was null in INSTANTIATE_PREFAB");

		Zenith_Prefab* pxPrefab = Zenith_AssetRegistry::Get<Zenith_Prefab>(xAction.m_szArg1);
		Zenith_Assert(pxPrefab, "Could not load prefab '%s' for instantiation", xAction.m_szArg1);

		const char* szEntityName = (xAction.m_szArg2 != nullptr) ? xAction.m_szArg2 : "";
		// Transform payload: pos[0..2], quat[3..6] (wxyz), scale[7..9].
		const Zenith_Maths::Vector3 xPos(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]);
		const Zenith_Maths::Quat    xRot(xAction.m_afArgs[3], xAction.m_afArgs[4], xAction.m_afArgs[5], xAction.m_afArgs[6]);
		const Zenith_Maths::Vector3 xScale(xAction.m_afArgs[7], xAction.m_afArgs[8], xAction.m_afArgs[9]);
		Zenith_Entity xEntity = pxPrefab->Instantiate(pxSceneData, szEntityName, xPos, xRot, xScale);
		Zenith_Assert(xEntity.IsValid(), "Instantiate returned invalid entity for '%s'", xAction.m_szArg1);

		// Mirror the editor's normal selection behaviour after entity creation
		// so subsequent transform/component steps target the new entity.
		g_xEngine.Editor().SelectEntity(xEntity.GetEntityID());
		break;
	}

	//--------------------------------------------------------------------------
	// Scene loading operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::LOAD_INITIAL_SCENE:
	{
		Zenith_Assert(xAction.m_pfnFunc, "Null function pointer for LOAD_INITIAL_SCENE");
		// Register the initial-scene-load callback so the editor's Play/Stop
		// cycle can re-run it later. Then invoke it once under a lifecycle
		// deferral guard so DispatchFullLifecycleInit owns Awake/OnEnable order.
		g_xEngine.Scenes().SetInitialSceneLoadCallback(xAction.m_pfnFunc);
		{
			Zenith_LifecycleDeferralGuard xGuard(g_xEngine.Scenes().MutableLifecycleLoadingFlagForGuard());
			xAction.m_pfnFunc();
		}
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

#ifdef ZENITH_TESTING
#include "Editor/Zenith_EditorAutomation.Tests.inl"
#endif

#endif // ZENITH_TOOLS
