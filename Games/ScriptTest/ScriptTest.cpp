#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/Flux_Enums.h"
#include "Input/Zenith_KeyCodes.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "EntityComponent/Zenith_EngineGraphBuilder.h"
#include "EntityComponent/Zenith_GraphOps.h"

#include <filesystem>
#include <string>

#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Physics/Zenith_Physics_Fwd.h"
#endif

#include "ScriptTest/ScriptTest_Graphs.h"

// ============================================================================
// ScriptTest -- a zero-gameplay-C++ game.
//
// There is no game component, no game graph node, and no input binding table
// in this project. Every behaviour in all eight scenes is executed by
// ENGINE-owned Behaviour Graph nodes, wired by the eighteen BuildGraph_ST_*
// functions below and attached to boot-authored entities. What the C++ here
// does is exactly three things:
//
//   1. create the seven flat-colour materials the scenes reference by path;
//   2. generate the two primitive meshes the scenes load (tools only);
//   3. author the graphs, the ball prefab and the eight scenes (tools only).
//
// That split is why (1) is NOT tools-gated while (2) and (3) are: a committed
// .zscen stores material and model PATHS, so a _False boot -- which runs no
// automation at all -- still has to resolve them, and only the material half
// is created in code.
//
// Every string that two things must agree on (entity names, UI element names,
// blackboard variables, custom events, asset paths, build indices) lives in
// ScriptTest_Graphs.h and is spelled once. None of them is compile-checked.
// ============================================================================

//=============================================================================
// Materials
//=============================================================================

namespace
{
	enum ScriptTest_MaterialSlot : u_int
	{
		ST_MATERIAL_FLOOR,
		ST_MATERIAL_PLAYER,
		ST_MATERIAL_BALL,
		ST_MATERIAL_PROP,
		ST_MATERIAL_RED,
		ST_MATERIAL_AMBER,
		ST_MATERIAL_GREEN,

		ST_MATERIAL_COUNT
	};

	// RAW pointers with an explicit AddRef, deliberately NOT Zenith_AssetHandles:
	// a file-scope handle would Release() into an already-freed registry from its
	// static destructor at atexit. Project_Shutdown drops every pin while the
	// registry is still alive, and a raw-pointer array has no destructor at all.
	Zenith_MaterialAsset* g_apxMaterials[ST_MATERIAL_COUNT] = {};

	// Write a 1x1 solid-colour .ztxtr and return a path-based handle for it.
	// Copied from Combat's ExportColoredTexture (Combat.cpp:191): the colour of
	// a flat material lives in a REAL diffuse texture, not in the base-colour
	// factor -- every game whose geometry demonstrably renders (Combat's arena,
	// RenderTest's platforms) binds a diffuse texture, and the base colour then
	// stays white so the multiply does not square the tint. Unconditional and
	// idempotent: a 25-byte overwrite per boot, so _False boots re-create it too.
	TextureHandle ST_ExportColoredTexture(const char* szPath, uint8_t uR, uint8_t uG, uint8_t uB)
	{
		uint8_t aucPixelData[] = { uR, uG, uB, 255 };

		Zenith_DataStream xStream;
		xStream << (int32_t)1;	// width
		xStream << (int32_t)1;	// height
		xStream << (int32_t)1;	// depth
		xStream << (TextureFormat)TEXTURE_FORMAT_RGBA8_UNORM;
		xStream << (size_t)4;	// data size (1x1x4 bytes)
		xStream.WriteData(aucPixelData, 4);
		xStream.WriteToFile(szPath);

		std::string strRelativePath = Zenith_AssetRegistry::MakeRelativePath(szPath);
		if (strRelativePath.empty())
		{
			Zenith_Error(LOG_CATEGORY_MATERIAL, "[ScriptTest] failed to make relative path for texture: %s", szPath);
			return TextureHandle();
		}
		return TextureHandle(strRelativePath);
	}

	Zenith_MaterialAsset* ST_CreateFlatMaterial(
		const char* szRegistryPath,
		const char* szTexturePath,
		const char* szName,
		const Zenith_Maths::Vector3& xBaseColour,
		float fRoughness,
		float fMetallic)
	{
		// Create(path) is documented get-or-create and ATOMIC: an already
		// registered path comes back AddRef'd rather than clobbered, so no
		// separate probe is needed (and a GetView probe would additionally try
		// to load a file that never exists on disk for a procedural asset).
		auto xhMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>(szRegistryPath);
		Zenith_MaterialAsset* pxMaterial = xhMaterial.GetDirect();
		if (pxMaterial == nullptr)
		{
			Zenith_Error(LOG_CATEGORY_MATERIAL, "[ScriptTest] failed to create material %s", szRegistryPath);
			return nullptr;
		}

		const TextureHandle xhTexture = ST_ExportColoredTexture(
			szTexturePath,
			static_cast<uint8_t>(xBaseColour.x * 255.0f),
			static_cast<uint8_t>(xBaseColour.y * 255.0f),
			static_cast<uint8_t>(xBaseColour.z * 255.0f));

		pxMaterial->SetName(szName);
		pxMaterial->SetDiffuseTexture(xhTexture);
		pxMaterial->SetBaseColor(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		pxMaterial->SetRoughness(fRoughness);
		pxMaterial->SetMetallic(fMetallic);

		// Pin it. This game changes scene on every gym entry and exit, and each
		// swap runs UnloadUnused -- without a ref of our own a material a
		// committed scene still references would be freed underneath it.
		pxMaterial->AddRef();
		return pxMaterial;
	}

	void ST_CreateMaterials()
	{
		if (g_apxMaterials[ST_MATERIAL_FLOOR] != nullptr)
		{
			return;	// already built (one process, one call -- but stay idempotent)
		}

		std::filesystem::create_directories(std::filesystem::path(GAME_ASSETS_DIR "Textures"));

		g_apxMaterials[ST_MATERIAL_FLOOR] = ST_CreateFlatMaterial(
			ScriptTest::Materials::szFLOOR,
			GAME_ASSETS_DIR "Textures/ST_Floor" ZENITH_TEXTURE_EXT, "ST_Floor", Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f), 0.9f, 0.0f);
		g_apxMaterials[ST_MATERIAL_PLAYER] = ST_CreateFlatMaterial(
			ScriptTest::Materials::szPLAYER,
			GAME_ASSETS_DIR "Textures/ST_Player" ZENITH_TEXTURE_EXT, "ST_Player", Zenith_Maths::Vector3(0.15f, 0.35f, 0.9f), 0.7f, 0.0f);
		g_apxMaterials[ST_MATERIAL_BALL] = ST_CreateFlatMaterial(
			ScriptTest::Materials::szBALL,
			GAME_ASSETS_DIR "Textures/ST_Ball" ZENITH_TEXTURE_EXT, "ST_Ball", Zenith_Maths::Vector3(1.0f, 0.55f, 0.1f), 0.7f, 0.0f);
		g_apxMaterials[ST_MATERIAL_PROP] = ST_CreateFlatMaterial(
			ScriptTest::Materials::szPROP,
			GAME_ASSETS_DIR "Textures/ST_Prop" ZENITH_TEXTURE_EXT, "ST_Prop", Zenith_Maths::Vector3(0.1f, 0.6f, 0.6f), 0.7f, 0.0f);
		g_apxMaterials[ST_MATERIAL_RED] = ST_CreateFlatMaterial(
			ScriptTest::Materials::szRED,
			GAME_ASSETS_DIR "Textures/ST_Red" ZENITH_TEXTURE_EXT, "ST_Red", Zenith_Maths::Vector3(0.9f, 0.1f, 0.1f), 0.7f, 0.0f);
		g_apxMaterials[ST_MATERIAL_AMBER] = ST_CreateFlatMaterial(
			ScriptTest::Materials::szAMBER,
			GAME_ASSETS_DIR "Textures/ST_Amber" ZENITH_TEXTURE_EXT, "ST_Amber", Zenith_Maths::Vector3(1.0f, 0.75f, 0.1f), 0.7f, 0.0f);
		g_apxMaterials[ST_MATERIAL_GREEN] = ST_CreateFlatMaterial(
			ScriptTest::Materials::szGREEN,
			GAME_ASSETS_DIR "Textures/ST_Green" ZENITH_TEXTURE_EXT, "ST_Green", Zenith_Maths::Vector3(0.1f, 0.8f, 0.2f), 0.7f, 0.0f);
	}

	void ST_ReleaseMaterials()
	{
		for (u_int u = 0; u < ST_MATERIAL_COUNT; ++u)
		{
			if (g_apxMaterials[u] != nullptr)
			{
				g_apxMaterials[u]->Release();
				g_apxMaterials[u] = nullptr;
			}
		}
	}
}

//=============================================================================
// Project entry points (non-authoring half)
//=============================================================================

const char* Project_GetName()
{
	return "ScriptTest";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
	// Engine defaults are what this game wants -- it is a scripting testbed,
	// not a rendering one.
}

// Materials ONLY. No game components, no game graph nodes, no input bindings:
// this game deliberately owns none. Unconditional (not tools-gated) because a
// _False boot loads committed scenes whose material references have to resolve
// with no authoring pass to create them.
void Project_RegisterGameComponents()
{
	ST_CreateMaterials();
}

void Project_Shutdown()
{
	ST_ReleaseMaterials();
}

void Project_LoadInitialScene();	// forward decl for the automation step below

//=============================================================================
// Behaviour graph builders
//
// Compiled UNCONDITIONALLY -- deliberately unlike Combat's, which are static
// and sit inside #ifdef ZENITH_TOOLS. ZENITH_INPUT_SIMULATOR is unconditional
// in Zenith.h, so the hermetic tests reference every builder in _False
// configurations; tools-gating them would break the D3D12_*_False link gate.
//
// Two rules govern the shapes below and are worth stating once:
//
//   * NO EXEC FAN-IN. A node with two exec predecessors is forbidden, so a
//     chain that would converge duplicates the node instance per pin instead.
//     ST_HubFlow's fourteen LoadSceneByIndex instances are the extreme case.
//   * EVERY NON-ON_UPDATE DISPATCH CARRIES dt = 0. Not just custom events:
//     OnStart, OnCollisionEnter/Exit and OnCustomEvent (including the
//     StateMachine's TLEnter_*/TLExit_* transitions) all fire with a zero delta
//     -- Zenith_GraphComponent::OnStart is literally
//     FireEventOnSlots(GRAPH_EVENT_ON_START, 0.0f, nullptr). So no Wait, Timer
//     or dt-scaled node may appear under ANY of them, ON_UPDATE being the only
//     source that carries a real delta. Tweens are safe under all of them:
//     Zenith_TweenComponent self-ticks at meta order 12, which is what lets
//     ST_PingPong hang an endless tween loop off OnStart.
//=============================================================================

// One hub row driven by its button. A fresh LoadSceneByIndex per row, because
// six buttons converging on one load node would be exec fan-in.
static void ST_BuildHubButtonChain(Zenith_EngineGraphBuilder& xB, const char* szButton, int32_t iSceneIndex)
{
	const u_int uClick = xB.Node("OnUIButtonClicked");
	xB.ParamString(uClick, "m_strButton", szButton);
	const u_int uLoad = xB.Node("LoadSceneByIndex");
	xB.ParamInt(uLoad, "m_iSceneIndex", iSceneIndex);
	xB.Chain(uClick, uLoad);
}

// The same row driven by its number key. Again its own load node.
static void ST_BuildHubKeyChain(Zenith_EngineGraphBuilder& xB, int32_t iKeyCode, int32_t iSceneIndex)
{
	const u_int uKey = xB.OnKeyPressed(iKeyCode);
	const u_int uLoad = xB.Node("LoadSceneByIndex");
	xB.ParamInt(uLoad, "m_iSceneIndex", iSceneIndex);
	xB.Chain(uKey, uLoad);
}

// spawn -> count -> HUD readout. Returns the chain's HEAD so the caller can
// anchor it to whichever source it is building (timer or key press).
static u_int ST_BuildBallSpawnChain(Zenith_EngineGraphBuilder& xB)
{
	const u_int uSpawn = xB.Node("SpawnPrefab");
	xB.ParamString(uSpawn, "m_strPrefabPath", ScriptTest::Prefabs::szBALL_ASSET);
	xB.ParamString(uSpawn, "m_strEntityName", ScriptTest::Entities::szBALL);

	const u_int uCount = xB.Node("AddBlackboardInt");
	xB.ParamString(uCount, "m_strVariable", ScriptTest::Vars::szSPAWN_COUNT);
	xB.ParamInt(uCount, "m_iDelta", 1);

	const u_int uText = xB.Node("SetUIText");
	xB.ParamString(uText, "m_strTargetVar", ScriptTest::Vars::szUI_TARGET);
	xB.ParamString(uText, "m_strElement", ScriptTest::UINames::szSPAWNED);
	xB.ParamString(uText, "m_strText", "Spawned: {}");
	xB.ParamString(uText, "m_strValueVar", ScriptTest::Vars::szSPAWN_COUNT);

	xB.Chain(uSpawn, uCount).Chain(uCount, uText);
	return uSpawn;
}

// count += iDelta, then repaint the counter. Its own SetUIText instance per
// caller (three sources drive this: two buttons and a key).
static u_int ST_BuildCounterBumpChain(Zenith_EngineGraphBuilder& xB, int32_t iDelta)
{
	const u_int uAdd = xB.Node("AddBlackboardInt");
	xB.ParamString(uAdd, "m_strVariable", ScriptTest::Vars::szCOUNT);
	xB.ParamInt(uAdd, "m_iDelta", iDelta);

	const u_int uText = xB.Node("SetUIText");
	xB.ParamString(uText, "m_strElement", ScriptTest::UINames::szCOUNTER);
	xB.ParamString(uText, "m_strText", "Count: {}");
	xB.ParamString(uText, "m_strValueVar", ScriptTest::Vars::szCOUNT);

	xB.Chain(uAdd, uText);
	return uAdd;
}

// A traffic-light lamp lighting up: grow it and name the state on the HUD.
// The SetUIText carries no m_strTargetVar, so it addresses SELF's canvas --
// this graph runs on the GameManager, which owns the UI.
static void ST_BuildLampEnterChain(Zenith_EngineGraphBuilder& xB, const char* szEvent, const char* szLamp, const char* szLabel)
{
	Zenith_GraphChain xEnter = xB.OnCustomEvent(szEvent);

	const u_int uFind = xB.Node("FindEntityByName");
	xB.ParamString(uFind, "m_strName", szLamp);
	xB.ParamString(uFind, "m_strResultVar", ScriptTest::Vars::szLAMP);

	const u_int uGrow = xB.Node("SetEntityScale");
	xB.ParamVec3(uGrow, "m_xScale", Zenith_Maths::Vector3(1.4f, 1.4f, 1.4f));
	xB.ParamString(uGrow, "m_strTargetVar", ScriptTest::Vars::szLAMP);

	const u_int uName = xB.Node("SetUIText");
	xB.ParamString(uName, "m_strElement", ScriptTest::UINames::szSTATE_NAME);
	xB.ParamString(uName, "m_strText", szLabel);

	xEnter.Then(uFind).Then(uGrow).Then(uName);
}

// ...and going dark again: scale only. The label is owned by whichever Enter
// chain runs next, so resetting it here would blank the readout between states.
static void ST_BuildLampExitChain(Zenith_EngineGraphBuilder& xB, const char* szEvent, const char* szLamp)
{
	Zenith_GraphChain xExit = xB.OnCustomEvent(szEvent);

	const u_int uFind = xB.Node("FindEntityByName");
	xB.ParamString(uFind, "m_strName", szLamp);
	xB.ParamString(uFind, "m_strResultVar", ScriptTest::Vars::szLAMP);

	const u_int uShrink = xB.Node("SetEntityScale");
	xB.ParamVec3(uShrink, "m_xScale", Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));
	xB.ParamString(uShrink, "m_strTargetVar", ScriptTest::Vars::szLAMP);

	xExit.Then(uFind).Then(uShrink);
}

// One SwitchOnInt case for ST_Dispenser: name the mode, then resize the nozzle
// to a value UNIQUE to this pin. The scale is what makes the pin identifiable
// from outside -- the label alone could have been written by any of the four.
static void ST_BuildModeCase(
	Zenith_EngineGraphBuilder& xB, u_int uSwitch, u_int uPin, const char* szLabel, float fNozzleScale)
{
	const u_int uName = xB.Node("SetBlackboardString");
	xB.ParamString(uName, "m_strVariable", ScriptTest::Vars::szLABEL);
	xB.ParamString(uName, "m_strValue", szLabel);

	const u_int uFind = xB.Node("FindEntityByName");
	xB.ParamString(uFind, "m_strName", ScriptTest::Entities::szNOZZLE);
	xB.ParamString(uFind, "m_strResultVar", ScriptTest::Vars::szNOZZLE_REF);

	const u_int uScale = xB.Node("SetEntityScale");
	xB.ParamVec3(uScale, "m_xScale", Zenith_Maths::Vector3(fNozzleScale, fNozzleScale, fNozzleScale));
	xB.ParamString(uScale, "m_strTargetVar", ScriptTest::Vars::szNOZZLE_REF);

	xB.Edge(uSwitch, uPin, uName);
	xB.Chain(uName, uFind).Chain(uFind, uScale);
}

// --- 1. ST_EscToHub ---------------------------------------------------------
// Escape leaves any gym. The load is LAST in its chain by necessity: a
// SINGLE-mode load tears down the dispatching entity's own scene, so nothing
// downstream of it would survive to run.
void BuildGraph_ST_EscToHub(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xEsc = xB.OnKeyPressed(ZENITH_KEY_ESCAPE);
	const u_int uLoad = xB.Node("LoadSceneByIndex");
	xB.ParamInt(uLoad, "m_iSceneIndex", ScriptTest::Scenes::iHUB);
	xEsc.Then(uLoad);
}

// --- 2. ST_HubFlow ----------------------------------------------------------
// Fourteen independent chains: each gym is reachable by clicking its button or
// by pressing its number. Fourteen LoadSceneByIndex instances, not one shared
// node -- see the exec-fan-in rule above.
void BuildGraph_ST_HubFlow(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	ST_BuildHubButtonChain(xB, ScriptTest::UINames::szBTN_MOTION,  ScriptTest::Scenes::iGYM_MOTION);
	ST_BuildHubButtonChain(xB, ScriptTest::UINames::szBTN_INPUT,   ScriptTest::Scenes::iGYM_INPUT);
	ST_BuildHubButtonChain(xB, ScriptTest::UINames::szBTN_PHYSICS, ScriptTest::Scenes::iGYM_PHYSICS);
	ST_BuildHubButtonChain(xB, ScriptTest::UINames::szBTN_EVENTS,  ScriptTest::Scenes::iGYM_EVENTS);
	ST_BuildHubButtonChain(xB, ScriptTest::UINames::szBTN_STATE,   ScriptTest::Scenes::iGYM_STATE);
	ST_BuildHubButtonChain(xB, ScriptTest::UINames::szBTN_UI,      ScriptTest::Scenes::iGYM_UI);
	ST_BuildHubButtonChain(xB, ScriptTest::UINames::szBTN_FLOW,    ScriptTest::Scenes::iGYM_FLOW);

	ST_BuildHubKeyChain(xB, ZENITH_KEY_1, ScriptTest::Scenes::iGYM_MOTION);
	ST_BuildHubKeyChain(xB, ZENITH_KEY_2, ScriptTest::Scenes::iGYM_INPUT);
	ST_BuildHubKeyChain(xB, ZENITH_KEY_3, ScriptTest::Scenes::iGYM_PHYSICS);
	ST_BuildHubKeyChain(xB, ZENITH_KEY_4, ScriptTest::Scenes::iGYM_EVENTS);
	ST_BuildHubKeyChain(xB, ZENITH_KEY_5, ScriptTest::Scenes::iGYM_STATE);
	ST_BuildHubKeyChain(xB, ZENITH_KEY_6, ScriptTest::Scenes::iGYM_UI);
	ST_BuildHubKeyChain(xB, ZENITH_KEY_7, ScriptTest::Scenes::iGYM_FLOW);
}

// --- 3. ST_Spin -------------------------------------------------------------
// The simplest graph in the game: yaw, forever. RotateEntity scales by the
// dispatched dt itself, so the property is a RATE.
void BuildGraph_ST_Spin(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xTick = xB.OnUpdate();
	const u_int uRotate = xB.Node("RotateEntity");
	xB.ParamFloat(uRotate, "m_fDegreesPerSecond", 45.0f);
	xTick.Then(uRotate);
}

// --- 4. ST_PingPong ---------------------------------------------------------
// OnStart fires once, but Repeat returns RUNNING between iterations, and a
// suspended ONE-SHOT anchor is re-driven by the ON_UPDATE dispatch until it
// finishes -- which, at m_iCount = -1, is never. That is what makes a
// fire-once source drive an endless animation.
void BuildGraph_ST_PingPong(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xStart = xB.OnStart();
	const u_int uRepeat = xB.Node("Repeat");
	xB.ParamInt(uRepeat, "m_iCount", -1);	// forever
	xStart.Then(uRepeat);

	const u_int uToRight = xB.Node("TweenPosition");
	xB.ParamVec3(uToRight, "m_xTo", Zenith_Maths::Vector3(3.0f, 1.0f, 4.0f));
	xB.ParamFloat(uToRight, "m_fDuration", 1.5f);
	const u_int uWaitRight = xB.Node("WaitForTween");

	const u_int uToLeft = xB.Node("TweenPosition");
	xB.ParamVec3(uToLeft, "m_xTo", Zenith_Maths::Vector3(-3.0f, 1.0f, 4.0f));
	xB.ParamFloat(uToLeft, "m_fDuration", 1.5f);
	const u_int uWaitLeft = xB.Node("WaitForTween");

	// Pin 0 is Repeat's BODY; pin 1 (done) is deliberately unwired.
	xB.Edge(uRepeat, 0, uToRight);
	xB.Chain(uToRight, uWaitRight).Chain(uWaitRight, uToLeft).Chain(uToLeft, uWaitLeft);
}

// --- 5. ST_SineBob ----------------------------------------------------------
// bobVel = (0, 2, 0) * cos(t), then translate by it. The SetBlackboardVector3
// re-seeds bobVel every tick ON PURPOSE: scaling in place without it would
// compound cos(t) frame after frame and collapse the vector to zero.
void BuildGraph_ST_SineBob(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xTick = xB.OnUpdate();

	const u_int uAdvance = xB.Node("AddBlackboardFloat");
	xB.ParamString(uAdvance, "m_strVariable", ScriptTest::Vars::szT);
	xB.ParamFloat(uAdvance, "m_fDelta", 2.0f);
	xB.ParamBool(uAdvance, "m_bScaleByDt", true);

	const u_int uCos = xB.Node("MathBlackboardFloat");
	xB.ParamString(uCos, "m_strVar", ScriptTest::Vars::szT);
	xB.ParamEnum(uCos, "m_iOp", GRAPH_MATH_FLOAT_OP_COS);
	xB.ParamString(uCos, "m_strResultVar", ScriptTest::Vars::szCOS_T);

	const u_int uSeed = xB.Node("SetBlackboardVector3");
	xB.ParamString(uSeed, "m_strVariable", ScriptTest::Vars::szBOB_VEL);
	xB.ParamVec3(uSeed, "m_xValue", Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f));

	// MathBlackboardVector3's op codes are raw ints -- unlike the float node,
	// this one has no named enum in Zenith_GraphOps.h. 2 = scale by scalar.
	const u_int uScale = xB.Node("MathBlackboardVector3");
	xB.ParamString(uScale, "m_strVar", ScriptTest::Vars::szBOB_VEL);
	xB.ParamInt(uScale, "m_iOp", 2);
	xB.ParamString(uScale, "m_strScalarVar", ScriptTest::Vars::szCOS_T);
	xB.ParamString(uScale, "m_strResultVar", ScriptTest::Vars::szBOB_VEL);

	// TranslateEntity multiplies by dt itself, so bobVel is a VELOCITY.
	const u_int uMove = xB.Node("TranslateEntity");
	xB.ParamString(uMove, "m_strUnitsVar", ScriptTest::Vars::szBOB_VEL);

	xTick.Then(uAdvance).Then(uCos).Then(uSeed).Then(uScale).Then(uMove);
}

// --- 6. ST_PlayerMove -------------------------------------------------------
// WASD -> a normalized direction -> a 6 m/s velocity on XZ. m_bSetY stays
// false so gravity keeps owning the vertical axis; that is what lets ST_Jump
// exist as a separate graph on the same entity.
void BuildGraph_ST_PlayerMove(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	// Chain 1: stand the cube upright for good. LockRotation is ONE-WAY (the
	// engine never restores inertia), so this runs once and is done.
	Zenith_GraphChain xStart = xB.OnStart();
	const u_int uLock = xB.Node("LockRotation");
	xB.ParamBool(uLock, "m_bLockX", true);
	xB.ParamBool(uLock, "m_bLockY", true);
	xB.ParamBool(uLock, "m_bLockZ", true);
	xStart.Then(uLock);

	// Chain 2: the per-frame drive.
	Zenith_GraphChain xTick = xB.OnUpdate();

	const u_int uRead = xB.Node("ReadMovementAxis");
	xB.ParamString(uRead, "m_strResultVar", ScriptTest::Vars::szMOVE_DIR);

	const u_int uScale = xB.Node("MathBlackboardVector3");
	xB.ParamString(uScale, "m_strVar", ScriptTest::Vars::szMOVE_DIR);
	xB.ParamInt(uScale, "m_iOp", 2);	// scale by scalar
	xB.ParamFloat(uScale, "m_fScalar", 6.0f);
	xB.ParamString(uScale, "m_strResultVar", ScriptTest::Vars::szMOVE_VEL);

	const u_int uVelocity = xB.Node("SetVelocity");
	xB.ParamString(uVelocity, "m_strVelocityVar", ScriptTest::Vars::szMOVE_VEL);
	xB.ParamBool(uVelocity, "m_bSetY", false);

	xTick.Then(uRead).Then(uScale).Then(uVelocity);
}

// --- 7. ST_Jump -------------------------------------------------------------
// The Raycast IS the grounded test. It has one exec pin and gates by STATUS:
// a miss returns FAILURE, which aborts the chain before the impulse -- so a
// player in mid-air cannot double-jump, with no explicit branch anywhere.
void BuildGraph_ST_Jump(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xJump = xB.OnKeyPressed(ZENITH_KEY_SPACE);

	const u_int uGrounded = xB.Node("Raycast");
	xB.ParamVec3(uGrounded, "m_xDirection", Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f));
	xB.ParamFloat(uGrounded, "m_fMaxDistance", 0.8f);

	// AddImpulse is a mass-independent VELOCITY delta (m/s), not N*s.
	const u_int uImpulse = xB.Node("ApplyImpulse");
	xB.ParamVec3(uImpulse, "m_xImpulse", Zenith_Maths::Vector3(0.0f, 6.0f, 0.0f));

	xJump.Then(uGrounded).Then(uImpulse);
}

// --- 8. ST_BallSpawner ------------------------------------------------------
// Balls drop from a spawner parked over the void, so every one of them misses
// the platform and falls into ST_KillVolume -- the two gym halves are one
// loop. The HUD lives on the GameManager, so the count has to be written
// cross-entity: FindEntityByName stashes a packed EntityID once at OnStart and
// SetUIText targets it through m_strTargetVar.
void BuildGraph_ST_BallSpawner(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	// Chain 1: resolve the HUD entity once.
	Zenith_GraphChain xStart = xB.OnStart();
	const u_int uFind = xB.Node("FindEntityByName");
	xB.ParamString(uFind, "m_strName", ScriptTest::Entities::szGAME_MANAGER);
	xB.ParamString(uFind, "m_strResultVar", ScriptTest::Vars::szUI_TARGET);
	xStart.Then(uFind);

	// Chain 2: a ball every 1.5 s.
	const u_int uTimer = xB.Node("Timer");
	xB.ParamFloat(uTimer, "m_fInterval", 1.5f);
	const u_int uTimedSpawn = ST_BuildBallSpawnChain(xB);
	xB.Chain(uTimer, uTimedSpawn);

	// Chain 3: and one on demand. Fresh node instances -- two sources reaching
	// the same spawn node would be exec fan-in.
	const u_int uKey = xB.OnKeyPressed(ZENITH_KEY_SPACE);
	const u_int uKeyedSpawn = ST_BuildBallSpawnChain(xB);
	xB.Chain(uKey, uKeyedSpawn);
}

// --- 9. ST_KillVolume -------------------------------------------------------
// A static sensor slab under the world. Static sensors only receive events
// from DYNAMIC bodies, which is exactly what the balls are.
void BuildGraph_ST_KillVolume(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	// Two separate OnStart chains rather than one with two actions: keeping
	// them independent means a failure in either cannot abort the other.
	Zenith_GraphChain xArm = xB.OnStart();
	const u_int uSensor = xB.Node("SetSensor");
	xB.ParamBool(uSensor, "m_bSensor", true);
	xArm.Then(uSensor);

	Zenith_GraphChain xResolve = xB.OnStart();
	const u_int uFind = xB.Node("FindEntityByName");
	xB.ParamString(uFind, "m_strName", ScriptTest::Entities::szGAME_MANAGER);
	xB.ParamString(uFind, "m_strResultVar", ScriptTest::Vars::szUI_TARGET);
	xResolve.Then(uFind);

	// The collision source stashes the other entity as a packed EntityID; every
	// node below reads it through m_strTargetVar. Note there is no Wait or
	// Timer anywhere here -- this chain dispatches at dt = 0.
	const u_int uHit = xB.Node("OnCollisionEnter");
	xB.ParamString(uHit, "m_strStoreEntityVar", ScriptTest::Vars::szOTHER);

	const u_int uDestroy = xB.Node("DestroyEntity");
	xB.ParamString(uDestroy, "m_strTargetVar", ScriptTest::Vars::szOTHER);

	const u_int uCount = xB.Node("AddBlackboardInt");
	xB.ParamString(uCount, "m_strVariable", ScriptTest::Vars::szKILL_COUNT);
	xB.ParamInt(uCount, "m_iDelta", 1);

	const u_int uText = xB.Node("SetUIText");
	xB.ParamString(uText, "m_strTargetVar", ScriptTest::Vars::szUI_TARGET);
	xB.ParamString(uText, "m_strElement", ScriptTest::UINames::szKILLED);
	xB.ParamString(uText, "m_strText", "Killed: {}");
	xB.ParamString(uText, "m_strValueVar", ScriptTest::Vars::szKILL_COUNT);

	xB.Chain(uHit, uDestroy).Chain(uDestroy, uCount).Chain(uCount, uText);
}

// --- 10. ST_PressurePlate ---------------------------------------------------
// Stand on it, the door opens; step off, it closes. The plate does not know
// what a door is -- it fires a named event at an entity it looked up by name,
// and ST_Door decides what that means.
void BuildGraph_ST_PressurePlate(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xArm = xB.OnStart();
	const u_int uSensor = xB.Node("SetSensor");
	xB.ParamBool(uSensor, "m_bSensor", true);
	xArm.Then(uSensor);

	const u_int uEnter = xB.Node("OnCollisionEnter");
	const u_int uFindOpen = xB.Node("FindEntityByName");
	xB.ParamString(uFindOpen, "m_strName", ScriptTest::Entities::szGYM_DOOR);
	xB.ParamString(uFindOpen, "m_strResultVar", ScriptTest::Vars::szDOOR);
	const u_int uOpen = xB.FireCustomEvent(ScriptTest::Events::szOPEN_DOOR, ScriptTest::Vars::szDOOR);
	xB.Chain(uEnter, uFindOpen).Chain(uFindOpen, uOpen);

	// A second FindEntityByName instance, for the fan-in rule again.
	const u_int uExit = xB.Node("OnCollisionExit");
	const u_int uFindClose = xB.Node("FindEntityByName");
	xB.ParamString(uFindClose, "m_strName", ScriptTest::Entities::szGYM_DOOR);
	xB.ParamString(uFindClose, "m_strResultVar", ScriptTest::Vars::szDOOR);
	const u_int uClose = xB.FireCustomEvent(ScriptTest::Events::szCLOSE_DOOR, ScriptTest::Vars::szDOOR);
	xB.Chain(uExit, uFindClose).Chain(uFindClose, uClose);
}

// --- 11. ST_Door ------------------------------------------------------------
// Two absolute positions, tweened. Tweens are the one time-based thing a
// dt = 0 custom-event chain may start: Zenith_TweenComponent ticks itself at
// meta order 12 rather than riding the dispatching context's delta.
void BuildGraph_ST_Door(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xOpen = xB.OnCustomEvent(ScriptTest::Events::szOPEN_DOOR);
	const u_int uRaise = xB.Node("TweenPosition");
	xB.ParamVec3(uRaise, "m_xTo", Zenith_Maths::Vector3(8.0f, 4.5f, 0.0f));
	xB.ParamFloat(uRaise, "m_fDuration", 0.8f);
	xOpen.Then(uRaise);

	Zenith_GraphChain xClose = xB.OnCustomEvent(ScriptTest::Events::szCLOSE_DOOR);
	const u_int uLower = xB.Node("TweenPosition");
	xB.ParamVec3(uLower, "m_xTo", Zenith_Maths::Vector3(8.0f, 1.5f, 0.0f));
	xB.ParamFloat(uLower, "m_fDuration", 0.8f);
	xClose.Then(uLower);
}

// --- 12. ST_BellRing --------------------------------------------------------
// Broadcast, not FireCustomEvent: the three listeners are not named anywhere,
// and the ringer does not need to know they exist.
void BuildGraph_ST_BellRing(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xBell = xB.OnKeyPressed(ZENITH_KEY_B);
	const u_int uBroadcast = xB.Node("BroadcastCustomEvent");
	xB.ParamString(uBroadcast, "m_strEventName", ScriptTest::Events::szBELL);
	xBell.Then(uBroadcast);
}

// --- 13. ST_BellListener ----------------------------------------------------
// One graph, three entities. The WaitForTween suspends the chain and the
// custom-event anchor is re-driven by ON_UPDATE until the pop finishes.
void BuildGraph_ST_BellListener(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xBell = xB.OnCustomEvent(ScriptTest::Events::szBELL);

	const u_int uGrow = xB.Node("TweenScale");
	xB.ParamVec3(uGrow, "m_xTo", Zenith_Maths::Vector3(1.3f, 1.3f, 1.3f));
	xB.ParamFloat(uGrow, "m_fDuration", 0.15f);

	const u_int uWait = xB.Node("WaitForTween");

	const u_int uSettle = xB.Node("TweenScale");
	xB.ParamVec3(uSettle, "m_xTo", Zenith_Maths::Vector3(0.8f, 0.8f, 0.8f));
	xB.ParamFloat(uSettle, "m_fDuration", 0.3f);

	xBell.Then(uGrow).Then(uWait).Then(uSettle);
}

// --- 14. ST_TrafficLight ----------------------------------------------------
// The StateMachine's state VARIABLE is the single source of truth: each state
// body waits, then writes the next value, and the machine reacts on the
// following fire. m_strEventPrefix makes the transitions observable as custom
// events ("TL" + "Enter_" + the state name), which is what lets the six visual
// chains live in this same graph without the state bodies knowing about lamps.
void BuildGraph_ST_TrafficLight(Zenith_GraphBuilder& xBuilder)
{
	Zenith_PropertyValue xLight;
	xLight.SetInt32(0);	// start on Red
	xBuilder.Variable(ScriptTest::Vars::szLIGHT, xLight);

	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xTick = xB.OnUpdate();
	const u_int uStateMachine = xB.StateMachine(ScriptTest::Vars::szLIGHT, 3, ScriptTest::Events::szTL_STATE_NAMES);
	xB.ParamString(uStateMachine, "m_strEventPrefix", ScriptTest::Events::szTL_PREFIX);
	xTick.Then(uStateMachine);

	// Pin 0 = Red. The Wait nodes are legal here because this machine is driven
	// by ON_UPDATE, which carries a real dt.
	const u_int uWaitRed = xB.Node("Wait");
	xB.ParamFloat(uWaitRed, "m_fSeconds", 3.0f);
	const u_int uToGreen = xB.SetBlackboardInt(ScriptTest::Vars::szLIGHT, 1);
	xB.Edge(uStateMachine, 0, uWaitRed);
	xB.Chain(uWaitRed, uToGreen);

	// Pin 1 = Green.
	const u_int uWaitGreen = xB.Node("Wait");
	xB.ParamFloat(uWaitGreen, "m_fSeconds", 3.0f);
	const u_int uToAmber = xB.SetBlackboardInt(ScriptTest::Vars::szLIGHT, 2);
	xB.Edge(uStateMachine, 1, uWaitGreen);
	xB.Chain(uWaitGreen, uToAmber);

	// Pin 2 = Amber, the short one.
	const u_int uWaitAmber = xB.Node("Wait");
	xB.ParamFloat(uWaitAmber, "m_fSeconds", 1.0f);
	const u_int uToRed = xB.SetBlackboardInt(ScriptTest::Vars::szLIGHT, 0);
	xB.Edge(uStateMachine, 2, uWaitAmber);
	xB.Chain(uWaitAmber, uToRed);

	// The six visual chains. These run at dt = 0 (they are custom events fired
	// from inside the transition), so they contain no timed node at all.
	ST_BuildLampEnterChain(xB, ScriptTest::Events::szTL_ENTER_RED, ScriptTest::Entities::szLAMP_RED, "RED");
	ST_BuildLampExitChain(xB, ScriptTest::Events::szTL_EXIT_RED, ScriptTest::Entities::szLAMP_RED);
	ST_BuildLampEnterChain(xB, ScriptTest::Events::szTL_ENTER_GREEN, ScriptTest::Entities::szLAMP_GREEN, "GREEN");
	ST_BuildLampExitChain(xB, ScriptTest::Events::szTL_EXIT_GREEN, ScriptTest::Entities::szLAMP_GREEN);
	ST_BuildLampEnterChain(xB, ScriptTest::Events::szTL_ENTER_AMBER, ScriptTest::Entities::szLAMP_AMBER, "AMBER");
	ST_BuildLampExitChain(xB, ScriptTest::Events::szTL_EXIT_AMBER, ScriptTest::Entities::szLAMP_AMBER);
}

// --- 15. ST_UIPlayground ----------------------------------------------------
// Buttons, a key, a formatted clock, a fill bar driven by a modulo cycle, and
// a colour that flips past 80%. Everything the UI node family does, on one
// canvas, with no C++ behind any of it.
void BuildGraph_ST_UIPlayground(Zenith_GraphBuilder& xBuilder)
{
	Zenith_PropertyValue xCount;
	xCount.SetInt32(0);
	xBuilder.Variable(ScriptTest::Vars::szCOUNT, xCount);

	Zenith_PropertyValue xClock;
	xClock.SetFloat(0.0f);
	xBuilder.Variable(ScriptTest::Vars::szCLOCK, xClock);

	Zenith_EngineGraphBuilder xB(xBuilder);

	// Three sources, three private copies of the bump chain.
	const u_int uPlus = xB.Node("OnUIButtonClicked");
	xB.ParamString(uPlus, "m_strButton", ScriptTest::UINames::szBTN_PLUS);
	const u_int uPlusChain = ST_BuildCounterBumpChain(xB, 1);
	xB.Chain(uPlus, uPlusChain);

	const u_int uMinus = xB.Node("OnUIButtonClicked");
	xB.ParamString(uMinus, "m_strButton", ScriptTest::UINames::szBTN_MINUS);
	const u_int uMinusChain = ST_BuildCounterBumpChain(xB, -1);
	xB.Chain(uMinus, uMinusChain);

	const u_int uUpKey = xB.OnKeyPressed(ZENITH_KEY_UP);
	const u_int uUpChain = ST_BuildCounterBumpChain(xB, 1);
	xB.Chain(uUpKey, uUpChain);

	// The clock / bar chain.
	Zenith_GraphChain xTick = xB.OnUpdate();

	const u_int uAdvance = xB.Node("AddBlackboardFloat");
	xB.ParamString(uAdvance, "m_strVariable", ScriptTest::Vars::szCLOCK);
	xB.ParamFloat(uAdvance, "m_fDelta", 1.0f);
	xB.ParamBool(uAdvance, "m_bScaleByDt", true);

	const u_int uClockText = xB.Node("SetUIText");
	xB.ParamString(uClockText, "m_strElement", ScriptTest::UINames::szCLOCK);
	xB.ParamString(uClockText, "m_strText", "t = {}s");
	xB.ParamString(uClockText, "m_strValueVar", ScriptTest::Vars::szCLOCK);
	xB.ParamInt(uClockText, "m_iDecimals", 1);	// -1 would print the shortest form

	// cycle = fmod(clock, 5); fill01 = cycle / 5 -- a 5 s sawtooth in [0, 1).
	const u_int uModulo = xB.Node("MathBlackboardFloat");
	xB.ParamString(uModulo, "m_strVar", ScriptTest::Vars::szCLOCK);
	xB.ParamEnum(uModulo, "m_iOp", GRAPH_MATH_FLOAT_OP_MODULO);
	xB.ParamFloat(uModulo, "m_fOperand", 5.0f);
	xB.ParamString(uModulo, "m_strResultVar", ScriptTest::Vars::szCYCLE);

	const u_int uDivide = xB.Node("MathBlackboardFloat");
	xB.ParamString(uDivide, "m_strVar", ScriptTest::Vars::szCYCLE);
	xB.ParamEnum(uDivide, "m_iOp", GRAPH_MATH_FLOAT_OP_DIVIDE);
	xB.ParamFloat(uDivide, "m_fOperand", 5.0f);
	xB.ParamString(uDivide, "m_strResultVar", ScriptTest::Vars::szFILL01);

	const u_int uFill = xB.Node("SetUIFillAmount");
	xB.ParamString(uFill, "m_strElement", ScriptTest::UINames::szBAR_FILL);
	xB.ParamString(uFill, "m_strAmountVar", ScriptTest::Vars::szFILL01);

	const u_int uCompare = xB.CompareFloat(
		ScriptTest::Vars::szFILL01, GRAPH_COMPARE_FLOAT_OP_GREATER, 0.8f, ScriptTest::Vars::szHOT);
	const u_int uBranch = xB.Branch(ScriptTest::Vars::szHOT);

	// Both colours come from the header, not from literals here: SetUIColor is
	// chain-TERMINAL on both Branch pins, so ST_UIGym_Test reading the element's
	// colour back is the only thing that can see either one -- and it has to
	// compare against the same spelling the node was authored with.
	Zenith_PropertyValue xHotColour;
	xHotColour.SetVector4(ScriptTest::Colours::xBAR_HOT);
	const u_int uHot = xB.Node("SetUIColor");
	xB.ParamString(uHot, "m_strElement", ScriptTest::UINames::szBAR_FILL);
	xB.Param(uHot, "m_xColor", xHotColour);

	Zenith_PropertyValue xCoolColour;
	xCoolColour.SetVector4(ScriptTest::Colours::xBAR_COOL);
	const u_int uCool = xB.Node("SetUIColor");
	xB.ParamString(uCool, "m_strElement", ScriptTest::UINames::szBAR_FILL);
	xB.Param(uCool, "m_xColor", xCoolColour);

	xTick.Then(uAdvance).Then(uClockText).Then(uModulo).Then(uDivide).Then(uFill).Then(uCompare).Then(uBranch);
	xB.Edge(uBranch, 0, uHot);	// true
	xB.Edge(uBranch, 1, uCool);	// false
}

// --- 16. ST_Dispenser -------------------------------------------------------
// A dispenser, and the one graph in this game that uses the MULTI-WAY flow
// constructs. Fifteen independent chains on one entity, because every one of
// them needs its own source (no exec fan-in):
//
//   Once ............ a start-up bonus that must land EXACTLY once
//   WaitForCondition. RUNNING under a one-shot OnStart anchor until the plate
//                     arms it -- the ST_PingPong re-drive mechanism, and the
//                     reason a fire-once source can wait on something
//   Cooldown ........ throttles the dispense key. Reads m_fTimeSeconds (which
//                     ACCUMULATES dt, so it is frame-deterministic under the
//                     test harness's pinned 1/60), not the dispatched dt
//   Gate ............ blocks the dispense until armed
//   CallGraph ....... scoring, delegated to ST_FlowScore against THIS
//                     blackboard -- the child writes 'score' here
//   ListAdd/RemoveAt/Clear + GetListCount/GetListElement + ForEach ... the
//                     whole list family, produce and consume
//   LogicBlackboardBool ... twice: once as NOT (one operand + m_bInvert), once
//                     as the N-ary AND over the result
//   SwitchOnInt ..... mode -> a label and a nozzle scale, 3 cases + default
//   SwitchOnString .. that label -> an index, 3 cases + default
//   Selector ........ an alarm branch with priority over the normal one
//
// ★ EVERY CHAIN BELOW IS ANCHORED ON A KEY OR ON OnUpdate, both of which carry
// a REAL dt, so the no-timed-node-under-a-zero-dt-source rule never bites here.
// OnKeyPressed is registered as an ON_UPDATE source that gates on the key edge,
// so it fires every frame and resumes suspended chains exactly like OnUpdate.
// The one exception is chain 3, a custom event -- and it contains no timed node.
void BuildGraph_ST_Dispenser(Zenith_GraphBuilder& xBuilder)
{
	// Declared with defaults so the graph editor shows them and, more
	// importantly, so 'sentinel' carries a value DISTINGUISHABLE from the one
	// its chain would write: chain 11 proves a node did NOT run, which needs a
	// pre-existing value to survive.
	auto DeclareInt = [&xBuilder](const char* szName, int32_t iValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(iValue);
		xBuilder.Variable(szName, xValue);
	};
	auto DeclareBool = [&xBuilder](const char* szName, bool bValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetBool(bValue);
		xBuilder.Variable(szName, xValue);
	};

	DeclareInt(ScriptTest::Vars::szBONUS, 0);
	DeclareInt(ScriptTest::Vars::szDISPENSED, 0);
	DeclareInt(ScriptTest::Vars::szSCORE, 0);
	DeclareInt(ScriptTest::Vars::szMODE, 0);
	DeclareInt(ScriptTest::Vars::szLABEL_INDEX, -1);
	DeclareInt(ScriptTest::Vars::szBAG_COUNT, 0);
	DeclareInt(ScriptTest::Vars::szVISITED, 0);
	DeclareInt(ScriptTest::Vars::szIDX, -1);
	DeclareInt(ScriptTest::Vars::szHEAD, -1);
	DeclareInt(ScriptTest::Vars::szSENTINEL, -1);
	DeclareInt(ScriptTest::Vars::szNORMAL_RUNS, 0);
	DeclareInt(ScriptTest::Vars::szALARM_RUNS, 0);
	DeclareBool(ScriptTest::Vars::szREADY, false);
	DeclareBool(ScriptTest::Vars::szARMED, false);
	DeclareBool(ScriptTest::Vars::szJAMMED, false);
	DeclareBool(ScriptTest::Vars::szALARM, false);
	DeclareBool(ScriptTest::Vars::szNOT_JAMMED, false);
	DeclareBool(ScriptTest::Vars::szCAN_DISPENSE, false);

	Zenith_EngineGraphBuilder xB(xBuilder);

	// --- Chain 1: the one-off bonus.
	// Once returns FAILURE forever after its first pass, so even a re-driven
	// anchor could not move the counter twice -- which is why the test asserts
	// == 1 rather than >= 1.
	Zenith_GraphChain xBonus = xB.OnStart();
	const u_int uOnce = xB.Node("Once");
	const u_int uBonus = xB.Node("AddBlackboardInt");
	xB.ParamString(uBonus, "m_strVariable", ScriptTest::Vars::szBONUS);
	xB.ParamInt(uBonus, "m_iDelta", 1);
	xBonus.Then(uOnce).Then(uBonus);

	// --- Chain 2: arm the dispenser, whenever the plate gets around to it.
	// WaitForCondition returns RUNNING while 'ready' is false, which suspends
	// this one-shot OnStart chain; the ON_UPDATE dispatch re-drives it every
	// frame until the plate's event flips the flag.
	Zenith_GraphChain xArm = xB.OnStart();
	const u_int uWaitReady = xB.Node("WaitForCondition");
	xB.ParamString(uWaitReady, "m_strConditionVar", ScriptTest::Vars::szREADY);
	const u_int uSetArmed = xB.SetBlackboardBool(ScriptTest::Vars::szARMED, true);
	xArm.Then(uWaitReady).Then(uSetArmed);

	// --- Chain 3: the plate's event, arriving from ANOTHER entity's graph.
	Zenith_GraphChain xPlate = xB.OnCustomEvent(ScriptTest::Events::szPLATE_ARMED);
	const u_int uSetReady = xB.SetBlackboardBool(ScriptTest::Vars::szREADY, true);
	xPlate.Then(uSetReady);

	// --- Chain 4: dispense. Cooldown first, then the gate, then the effects.
	// ListAdd pushes the value of 'dispensed' AFTER the increment, so the bag
	// reads [1, 2, 3, ...] and GetListCount tracks the counter exactly.
	Zenith_GraphChain xDispense = xB.OnKeyPressed(ZENITH_KEY_SPACE);
	const u_int uCooldown = xB.Node("Cooldown");
	xB.ParamFloat(uCooldown, "m_fSeconds", 0.75f);
	const u_int uGateArmed = xB.Gate(ScriptTest::Vars::szARMED);
	const u_int uCount = xB.Node("AddBlackboardInt");
	xB.ParamString(uCount, "m_strVariable", ScriptTest::Vars::szDISPENSED);
	xB.ParamInt(uCount, "m_iDelta", 1);
	const u_int uRecord = xB.ListAdd(ScriptTest::Vars::szBAG, ScriptTest::Vars::szDISPENSED);
	const u_int uScore = xB.Node("CallGraph");
	xB.ParamString(uScore, "m_strGraphAssetPath", ScriptTest::Graphs::szFLOW_SCORE);
	xDispense.Then(uCooldown).Then(uGateArmed).Then(uCount).Then(uRecord).Then(uScore);

	// --- Chains 5-8: the four single-key state pokes the test drives.
	Zenith_GraphChain xJam = xB.OnKeyPressed(ZENITH_KEY_J);
	xJam.Then(xB.SetBlackboardBool(ScriptTest::Vars::szJAMMED, true));

	Zenith_GraphChain xUnarm = xB.OnKeyPressed(ZENITH_KEY_U);
	xUnarm.Then(xB.SetBlackboardBool(ScriptTest::Vars::szARMED, false));

	Zenith_GraphChain xAlarm = xB.OnKeyPressed(ZENITH_KEY_A);
	xAlarm.Then(xB.SetBlackboardBool(ScriptTest::Vars::szALARM, true));

	Zenith_GraphChain xCycle = xB.OnKeyPressed(ZENITH_KEY_M);
	const u_int uNextMode = xB.Node("AddBlackboardInt");
	xB.ParamString(uNextMode, "m_strVariable", ScriptTest::Vars::szMODE);
	xB.ParamInt(uNextMode, "m_iDelta", 1);
	xCycle.Then(uNextMode);

	// --- Chain 9: walk the bag. The counter is RESET first, so the assertion is
	// "one pass visited exactly three elements", not "the body ran at least
	// three times ever".
	Zenith_GraphChain xWalk = xB.OnKeyPressed(ZENITH_KEY_F);
	const u_int uResetVisited = xB.SetBlackboardInt(ScriptTest::Vars::szVISITED, 0);
	const u_int uForEach = xB.ForEach(ScriptTest::Vars::szBAG, ScriptTest::Vars::szITEM, ScriptTest::Vars::szIDX);
	const u_int uVisit = xB.Node("AddBlackboardInt");
	xB.ParamString(uVisit, "m_strVariable", ScriptTest::Vars::szVISITED);
	xB.ParamInt(uVisit, "m_iDelta", 1);
	xWalk.Then(uResetVisited).Then(uForEach);
	xB.Edge(uForEach, 0, uVisit);	// pin 0 = body; pin 1 (done) deliberately unwired

	// --- Chain 10: drop the head of the bag, then READ the new head.
	// [1,2,3] minus index 0 must leave [2,3], so 'head' becomes 2. A swap-remove
	// would leave [3,2] and make it 3 -- which is the whole reason ListRemoveAt
	// is order-preserving, asserted here through the graph rather than only in
	// the node's own unit.
	Zenith_GraphChain xDrop = xB.OnKeyPressed(ZENITH_KEY_R);
	const u_int uRemove = xB.ListRemoveAt(ScriptTest::Vars::szBAG, 0);
	const u_int uHead = xB.GetListElement(ScriptTest::Vars::szBAG, 0, ScriptTest::Vars::szHEAD);
	xDrop.Then(uRemove).Then(uHead);

	// --- Chain 11: empty the bag, and prove the emptiness by ABORTING.
	// GetListElement on an empty list returns FAILURE, so the SetBlackboardInt
	// after it never runs and 'sentinel' keeps its declared -1. A graph has no
	// status variable, so an unwritten sentinel is the ONLY way one can observe
	// "that node failed".
	Zenith_GraphChain xEmpty = xB.OnKeyPressed(ZENITH_KEY_C);
	const u_int uClear = xB.ListClear(ScriptTest::Vars::szBAG);
	const u_int uProbe = xB.GetListElement(ScriptTest::Vars::szBAG, 0, ScriptTest::Vars::szITEM);
	const u_int uSentinel = xB.SetBlackboardInt(ScriptTest::Vars::szSENTINEL, 99);
	xEmpty.Then(uClear).Then(uProbe).Then(uSentinel);

	// --- Chain 12: the per-frame logic + the mode switch.
	//   notJammed   = NOT jammed              (one operand + m_bInvert)
	//   canDispense = armed AND notJammed     (the N-ary form, at two operands)
	//
	// ★ THE OPERAND LIST IS COMPOSED FROM THE TWO VARIABLE CONSTANTS, not typed
	// out as "armed,notJammed". The list is parsed VERBATIM -- no trimming --
	// so a hand-written copy that drifted from Vars:: by one character would
	// look up a variable that does not exist and silently read `false`.
	Zenith_GraphChain xTick = xB.OnUpdate();
	const u_int uNotJammed = xB.LogicBool(
		ScriptTest::Vars::szJAMMED, GRAPH_LOGIC_BOOL_OP_AND, ScriptTest::Vars::szNOT_JAMMED, /*invert*/ true);
	const std::string strDispenseOperands =
		std::string(ScriptTest::Vars::szARMED) + "," + ScriptTest::Vars::szNOT_JAMMED;
	const u_int uCanDispense = xB.LogicBool(
		strDispenseOperands.c_str(), GRAPH_LOGIC_BOOL_OP_AND, ScriptTest::Vars::szCAN_DISPENSE);
	const u_int uBagCount = xB.GetListCount(ScriptTest::Vars::szBAG, ScriptTest::Vars::szBAG_COUNT);
	const u_int uMode = xB.SwitchOnInt(ScriptTest::Vars::szMODE, 3);
	xTick.Then(uNotJammed).Then(uCanDispense).Then(uBagCount).Then(uMode);

	// Each case writes its OWN nozzle scale, so the scale identifies WHICH pin
	// ran -- four pins scaling to the same value would prove only that some pin
	// ran. Pin 3 is the DEFAULT (m_iCaseCount == 3).
	ST_BuildModeCase(xB, uMode, 0, ScriptTest::Labels::szRED,   1.0f);
	ST_BuildModeCase(xB, uMode, 1, ScriptTest::Labels::szGREEN, 1.4f);
	ST_BuildModeCase(xB, uMode, 2, ScriptTest::Labels::szBLUE,  1.8f);
	ST_BuildModeCase(xB, uMode, 3, ScriptTest::Labels::szNONE,  0.6f);

	// --- Chain 13: the label, back into an index. Its own OnUpdate anchor
	// because SwitchOnInt above has no pass-through exec output to chain from.
	// The case list is composed from the same three label constants chain 12
	// writes, for the reason spelled out there.
	Zenith_GraphChain xLabelTick = xB.OnUpdate();
	const std::string strModeCases = std::string(ScriptTest::Labels::szRED) + ","
		+ ScriptTest::Labels::szGREEN + "," + ScriptTest::Labels::szBLUE;
	const u_int uLabelSwitch = xB.Node("SwitchOnString");
	xB.ParamString(uLabelSwitch, "m_strVar", ScriptTest::Vars::szLABEL);
	xB.ParamString(uLabelSwitch, "m_strCases", strModeCases.c_str());
	xLabelTick.Then(uLabelSwitch);

	const u_int uIndexRed   = xB.SetBlackboardInt(ScriptTest::Vars::szLABEL_INDEX, 0);
	const u_int uIndexGreen = xB.SetBlackboardInt(ScriptTest::Vars::szLABEL_INDEX, 1);
	const u_int uIndexBlue  = xB.SetBlackboardInt(ScriptTest::Vars::szLABEL_INDEX, 2);
	const u_int uIndexNone  = xB.SetBlackboardInt(ScriptTest::Vars::szLABEL_INDEX, -1);
	xB.Edge(uLabelSwitch, 0, uIndexRed);
	xB.Edge(uLabelSwitch, 1, uIndexGreen);
	xB.Edge(uLabelSwitch, 2, uIndexBlue);
	xB.Edge(uLabelSwitch, 3, uIndexNone);	// pin 3 = default (3 cases)

	// --- Chain 14: priority. Pin 0 is the alarm branch, gated; pin 1 is normal.
	// While 'alarm' is false the gate FAILS pin 0 and the Selector falls through
	// to pin 1, so normalRuns climbs. The moment it is true, pin 0 SUCCEEDS and
	// pin 1 is never reached -- and preemption is observable ONLY as normalRuns
	// going flat, because nothing in the alarm branch touches it.
	Zenith_GraphChain xPriority = xB.OnUpdate();
	const u_int uSelector = xB.Node("Selector");
	xB.ParamInt(uSelector, "m_iBranchCount", 2);
	xPriority.Then(uSelector);

	const u_int uAlarmGate = xB.Gate(ScriptTest::Vars::szALARM);
	const u_int uAlarmRuns = xB.Node("AddBlackboardInt");
	xB.ParamString(uAlarmRuns, "m_strVariable", ScriptTest::Vars::szALARM_RUNS);
	xB.ParamInt(uAlarmRuns, "m_iDelta", 1);
	xB.Edge(uSelector, 0, uAlarmGate);
	xB.Chain(uAlarmGate, uAlarmRuns);

	const u_int uNormalRuns = xB.Node("AddBlackboardInt");
	xB.ParamString(uNormalRuns, "m_strVariable", ScriptTest::Vars::szNORMAL_RUNS);
	xB.ParamInt(uNormalRuns, "m_iDelta", 1);
	xB.Edge(uSelector, 1, uNormalRuns);

	// --- Chain 15: the HUD, on its own anchor so a missing element can never
	// abort the logic chain above it.
	Zenith_GraphChain xHud = xB.OnUpdate();
	const u_int uHudText = xB.Node("SetUIText");
	xB.ParamString(uHudText, "m_strElement", ScriptTest::UINames::szDISPENSED);
	xB.ParamString(uHudText, "m_strText", "Dispensed: {}");
	xB.ParamString(uHudText, "m_strValueVar", ScriptTest::Vars::szDISPENSED);
	xHud.Then(uHudText);
}

// --- 17. ST_FlowScore -------------------------------------------------------
// The CallGraph child. Deliberately trivial: what it proves is SCOPE, not
// behaviour. It writes 'score' with no target var and no payload, and the value
// shows up on the CALLER'S blackboard -- the whole contract of CallGraph's
// shared-blackboard model. A child with a board of its own would leave the
// caller's score at zero and nothing else would look any different.
void BuildGraph_ST_FlowScore(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	const u_int uCall = xB.Node("OnGraphCall");
	const u_int uAdd = xB.Node("AddBlackboardInt");
	xB.ParamString(uAdd, "m_strVariable", ScriptTest::Vars::szSCORE);
	xB.ParamInt(uAdd, "m_iDelta", 10);
	xB.Chain(uCall, uAdd);
}

// --- 18. ST_FlowPlate -------------------------------------------------------
// A second ENTITY arming the dispenser, so the WaitForCondition it unblocks is
// waiting on something outside its own graph. The plate does not know what
// "armed" means -- it names an entity and fires an event at it, exactly like
// ST_PressurePlate and its door.
void BuildGraph_ST_FlowPlate(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	Zenith_GraphChain xPress = xB.OnKeyPressed(ZENITH_KEY_P);
	const u_int uFind = xB.Node("FindEntityByName");
	xB.ParamString(uFind, "m_strName", ScriptTest::Entities::szGAME_MANAGER);
	xB.ParamString(uFind, "m_strResultVar", ScriptTest::Vars::szMANAGER_REF);
	const u_int uFire = xB.FireCustomEvent(ScriptTest::Events::szPLATE_ARMED, ScriptTest::Vars::szMANAGER_REF);
	xPress.Then(uFind).Then(uFire);
}

#ifdef ZENITH_TOOLS

//=============================================================================
// Generated meshes (tools only)
//
// Two primitives, exported once and then reused by every scene. Each is a
// .zasset (the CPU mesh) plus a .zmodel (the bundle AddStep_LoadModel takes
// and the saved scene persists by path). A non-tools build assumes a previous
// tools run produced both.
//=============================================================================

namespace
{
	void ST_EnsureGeneratedModel(const char* szMeshAssetPath, const char* szModelPath, const char* szName, bool bSphere)
	{
		std::filesystem::create_directories(std::filesystem::path(szMeshAssetPath).parent_path());

		if (!std::filesystem::exists(szMeshAssetPath))
		{
			Zenith_MeshAsset xMesh;
			if (bSphere)
			{
				Zenith_MeshAsset::GenerateUnitSphere(xMesh);	// default 16 segments
			}
			else
			{
				Zenith_MeshAsset::GenerateUnitCube(xMesh);
			}
			xMesh.Export(szMeshAssetPath);
		}

		if (!std::filesystem::exists(szModelPath))
		{
			auto xhModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
			Zenith_ModelAsset* pxModel = xhModel.GetDirect();
			if (pxModel == nullptr)
			{
				Zenith_Error(LOG_CATEGORY_MESH, "[ScriptTest] failed to create model asset for %s", szModelPath);
				return;
			}
			pxModel->SetName(szName);
			// No bundled material: the scenes override slot 0 per entity with
			// AddStep_SetModelMaterial, which is what lets one cube mesh serve
			// floors, props, lamps and the player.
			Zenith_Vector<std::string> xNoMaterials;
			pxModel->AddMeshByPath(szMeshAssetPath, xNoMaterials);
			pxModel->Export(szModelPath);
		}
	}
}

void Project_InitializeResources()
{
	ST_EnsureGeneratedModel(
		ScriptTest::Meshes::szUNIT_CUBE_ASSET, ScriptTest::Meshes::szUNIT_CUBE_MODEL, "UnitCube", false);
	ST_EnsureGeneratedModel(
		ScriptTest::Meshes::szUNIT_SPHERE_ASSET, ScriptTest::Meshes::szUNIT_SPHERE_MODEL, "UnitSphere", true);
}

//=============================================================================
// Scene authoring (tools only)
//
// Eight scenes, regenerated from scratch on every tools boot and saved over
// their committed .zscen files -- the same contract RenderTest and Combat use.
//
// Deterministic-FP for the whole block below: these functions compute the
// values they hand to the authoring steps (the camera FOVs go through
// Zenith_Maths::AuthoringRadians rather than glm::radians for exactly this
// reason), and everything they produce is SERIALIZED. Under the project's
// /fp:fast a Debug tools boot and a Release one would otherwise be free to
// author different bytes into a tracked asset, and the difference is 1-2 ULP,
// so every tolerance-based guard would stay green while it happened.
//=============================================================================

ZENITH_AUTHORING_DETERMINISM_BEGIN

namespace
{
	// Every camera in the game: same FOV, per-scene position and pitch. Pitch is
	// set explicitly even when it is zero, so the authored value never depends on
	// a component default that might move.
	void ST_AddSceneCamera(Zenith_EditorAutomation& xAuto, float fX, float fY, float fZ, float fPitch)
	{
		xAuto.AddStep_AddCamera();
		xAuto.AddStep_SetCameraPosition(fX, fY, fZ);
		xAuto.AddStep_SetCameraPitch(fPitch);
		// Yaw 0 faces +Z in this engine (Zenith_CameraComponent::GetFacingDir:
		// z = cos(yaw)*cos(pitch)). Every scene here authors its camera at
		// POSITIVE Z looking back at content around the origin, so all of them
		// need the half-turn — without it the whole scene sits BEHIND the
		// camera and the frustum cull (correctly) rejects every object.
		xAuto.AddStep_SetCameraYaw(Zenith_Maths::AuthoringRadians(180.0f));
		xAuto.AddStep_SetCameraFOV(Zenith_Maths::AuthoringRadians(60.0f));
		xAuto.AddStep_SetAsMainCamera();
	}

	void ST_AddTitleText(Zenith_EditorAutomation& xAuto, const char* szText, float fFontSize)
	{
		xAuto.AddStep_CreateUIText(ScriptTest::UINames::szTITLE, szText);
		xAuto.AddStep_SetUIAnchor(ScriptTest::UINames::szTITLE, static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIPosition(ScriptTest::UINames::szTITLE, 0.0f, -260.0f);
		xAuto.AddStep_SetUIFontSize(ScriptTest::UINames::szTITLE, fFontSize);
	}

	void ST_AddHintText(Zenith_EditorAutomation& xAuto, const char* szText)
	{
		xAuto.AddStep_CreateUIText(ScriptTest::UINames::szHINT, szText);
		xAuto.AddStep_SetUIAnchor(ScriptTest::UINames::szHINT, static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIPosition(ScriptTest::UINames::szHINT, 0.0f, 300.0f);
		xAuto.AddStep_SetUIFontSize(ScriptTest::UINames::szHINT, 18.0f);
	}

	// A HUD readout pinned to the top-left corner (the ball counters).
	void ST_AddCornerReadout(Zenith_EditorAutomation& xAuto, const char* szName, const char* szText, float fY)
	{
		xAuto.AddStep_CreateUIText(szName, szText);
		xAuto.AddStep_SetUIAnchor(szName, static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		xAuto.AddStep_SetUIPosition(szName, 20.0f, fY);
		xAuto.AddStep_SetUIFontSize(szName, 22.0f);
	}

	// One hub menu row. The element NAME is what ST_HubFlow's OnUIButtonClicked
	// watches; the label is only ever read by a human.
	void ST_AddHubButton(Zenith_EditorAutomation& xAuto, const char* szName, const char* szLabel, float fY)
	{
		xAuto.AddStep_CreateUIButton(szName, szLabel);
		xAuto.AddStep_SetUIAnchor(szName, static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIPosition(szName, 0.0f, fY);
		xAuto.AddStep_SetUISize(szName, 260.0f, 48.0f);
		xAuto.AddStep_SetUIButtonFontSize(szName, 22.0f);
	}

	// The key light every gym carries. No model -- it is a light, not a prop.
	void ST_AddKeyLight(Zenith_EditorAutomation& xAuto)
	{
		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szKEY_LIGHT);
		xAuto.AddStep_AddComponent("Light");
		xAuto.AddStep_SetTransformPosition(6.0f, 10.0f, 6.0f);
		xAuto.AddStep_SetLightColor(1.0f, 0.96f, 0.9f);
		xAuto.AddStep_SetLightIntensity(4.0f);
		xAuto.AddStep_SetLightRange(100.0f);
	}

	// The scene-authored daylight EVERY scene carries. Without a Sun component
	// the environment authority falls back to a near-horizon default: the sky
	// still renders, but the direct key + IBL derived from it light geometry
	// at dawn levels and every prop reads near-black on screen (the DP recipe
	// documents the same derivation for its deliberate midnight). 55 degrees =
	// late morning (0 = sunrise, 90 = noon -- Zenith_SunComponent.h:53).
	void ST_AddSun(Zenith_EditorAutomation& xAuto)
	{
		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szSUN);
		xAuto.AddStep_AddComponent("Sun");
		xAuto.AddStep_SetSunTimeOfDay(55.0f, 35.0f);
	}

	// A visible prop: model + material + transform, no collider. CreateEntity
	// selects what it creates, so every step after it targets this entity until
	// the next CreateEntity/SelectEntity.
	void ST_AddProp(
		Zenith_EditorAutomation& xAuto,
		const char* szName,
		const char* szModelPath,
		Zenith_MaterialAsset* pxMaterial,
		float fPosX, float fPosY, float fPosZ,
		float fScaleX, float fScaleY, float fScaleZ)
	{
		xAuto.AddStep_CreateEntity(szName);
		xAuto.AddStep_AddModel();
		xAuto.AddStep_LoadModel(szModelPath);
		xAuto.AddStep_SetModelMaterial(0, pxMaterial);
		xAuto.AddStep_SetTransformPosition(fPosX, fPosY, fPosZ);
		xAuto.AddStep_SetTransformScale(fScaleX, fScaleY, fScaleZ);
	}

	// ---- Scene 0: Hub ------------------------------------------------------
	// Camera and UI only, plus the shared sun so the sky backdrop matches the
	// gyms. No floor and no point light: nothing here is a lit 3D object.
	void ST_AuthorHubScene(Zenith_EditorAutomation& xAuto)
	{
		xAuto.AddStep_CreateScene("Hub");

		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szGAME_MANAGER);
		ST_AddSceneCamera(xAuto, 0.0f, 3.0f, 8.0f, -0.2f);
		xAuto.AddStep_AddUI();

		ST_AddTitleText(xAuto, "ScriptTest Gyms", 54.0f);

		// Row pitch 64 px from y = -140. Spelled out rather than computed: these
		// are serialized floats, and -140 + 64*i is one more expression inside
		// the determinism pin for no benefit.
		ST_AddHubButton(xAuto, ScriptTest::UINames::szBTN_MOTION,  "1. Motion",  -140.0f);
		ST_AddHubButton(xAuto, ScriptTest::UINames::szBTN_INPUT,   "2. Input",    -76.0f);
		ST_AddHubButton(xAuto, ScriptTest::UINames::szBTN_PHYSICS, "3. Physics",  -12.0f);
		ST_AddHubButton(xAuto, ScriptTest::UINames::szBTN_EVENTS,  "4. Events",    52.0f);
		ST_AddHubButton(xAuto, ScriptTest::UINames::szBTN_STATE,   "5. State",    116.0f);
		ST_AddHubButton(xAuto, ScriptTest::UINames::szBTN_UI,      "6. UI",       180.0f);
		ST_AddHubButton(xAuto, ScriptTest::UINames::szBTN_FLOW,    "7. Flow",     244.0f);

		ST_AddHintText(xAuto, "Click a gym or press 1-7");

		// The hub is the one scene with no ST_EscToHub: it IS the hub.
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szHUB_FLOW);

		ST_AddSun(xAuto);

		xAuto.AddStep_SaveScene(ScriptTest::Scenes::szHUB_PATH);
		xAuto.AddStep_UnloadScene();
	}

	// ---- Scene 1: Gym_Motion -----------------------------------------------
	// Three transform-driven graphs side by side: a spin, a tween ping-pong and
	// a blackboard-maths bob. None of the three has a collider -- they move by
	// writing the transform, which is only correct for non-physics entities.
	void ST_AuthorGymMotionScene(Zenith_EditorAutomation& xAuto)
	{
		xAuto.AddStep_CreateScene("Gym_Motion");

		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szGAME_MANAGER);
		ST_AddSceneCamera(xAuto, 0.0f, 6.0f, 14.0f, -0.35f);
		xAuto.AddStep_AddUI();
		ST_AddTitleText(xAuto, "Gym 1 - Motion", 40.0f);
		ST_AddHintText(xAuto, "Esc = hub");
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szESC_TO_HUB);

		ST_AddProp(xAuto, ScriptTest::Entities::szFLOOR, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_FLOOR], 0.0f, -0.5f, 0.0f, 12.0f, 0.5f, 12.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

		ST_AddProp(xAuto, ScriptTest::Entities::szSPINNER, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PROP], -4.0f, 1.0f, 0.0f, 1.5f, 0.25f, 1.5f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szSPIN);

		ST_AddProp(xAuto, ScriptTest::Entities::szPING_PONG, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PROP], -3.0f, 1.0f, 4.0f, 1.0f, 1.0f, 1.0f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szPING_PONG);

		ST_AddProp(xAuto, ScriptTest::Entities::szBOBBER, ScriptTest::Meshes::szUNIT_SPHERE_MODEL,
			g_apxMaterials[ST_MATERIAL_PROP], 4.0f, 2.0f, 0.0f, 1.0f, 1.0f, 1.0f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szSINE_BOB);

		ST_AddSun(xAuto);
		ST_AddKeyLight(xAuto);

		xAuto.AddStep_SaveScene(ScriptTest::Scenes::szGYM_MOTION_PATH);
		xAuto.AddStep_UnloadScene();
	}

	// ---- Scene 2: Gym_Input ------------------------------------------------
	// A dynamic cube driven entirely by input nodes. Two graphs on one entity:
	// slot 0 owns XZ velocity, slot 1 owns the jump impulse, and they compose
	// because ST_PlayerMove leaves Y alone.
	void ST_AuthorGymInputScene(Zenith_EditorAutomation& xAuto)
	{
		xAuto.AddStep_CreateScene("Gym_Input");

		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szGAME_MANAGER);
		ST_AddSceneCamera(xAuto, 0.0f, 8.0f, 16.0f, -0.4f);
		xAuto.AddStep_AddUI();
		ST_AddTitleText(xAuto, "Gym 2 - Input", 40.0f);
		ST_AddHintText(xAuto, "WASD move, Space jump, Esc hub");
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szESC_TO_HUB);

		ST_AddProp(xAuto, ScriptTest::Entities::szFLOOR, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_FLOOR], 0.0f, -0.5f, 0.0f, 15.0f, 0.5f, 15.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

		ST_AddProp(xAuto, ScriptTest::Entities::szPLAYER_CUBE, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PLAYER], 0.0f, 1.5f, 0.0f, 1.0f, 1.0f, 1.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szPLAYER_MOVE);	// slot 0
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szJUMP);			// slot 1

		ST_AddSun(xAuto);
		ST_AddKeyLight(xAuto);

		xAuto.AddStep_SaveScene(ScriptTest::Scenes::szGYM_INPUT_PATH);
		xAuto.AddStep_UnloadScene();
	}

	// ---- Scene 3: Gym_Physics ----------------------------------------------
	// A spawn/despawn loop with a HUD. The spawner sits OVER THE VOID at x = 8
	// while the platform is 8 wide -- so every ball misses it, falls to the
	// sensor slab at y = -8, and the two counters keep pace with each other.
	void ST_AuthorGymPhysicsScene(Zenith_EditorAutomation& xAuto)
	{
		xAuto.AddStep_CreateScene("Gym_Physics");

		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szGAME_MANAGER);
		ST_AddSceneCamera(xAuto, 0.0f, 10.0f, 20.0f, -0.45f);
		xAuto.AddStep_AddUI();
		ST_AddTitleText(xAuto, "Gym 3 - Physics", 40.0f);
		ST_AddHintText(xAuto, "Space = spawn ball");
		ST_AddCornerReadout(xAuto, ScriptTest::UINames::szSPAWNED, "Spawned: 0", 20.0f);
		ST_AddCornerReadout(xAuto, ScriptTest::UINames::szKILLED, "Killed: 0", 50.0f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szESC_TO_HUB);

		ST_AddProp(xAuto, ScriptTest::Entities::szPLATFORM, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_FLOOR], 0.0f, -0.5f, 0.0f, 8.0f, 0.5f, 8.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

		ST_AddProp(xAuto, ScriptTest::Entities::szSPAWNER, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PROP], 8.0f, 6.0f, 0.0f, 0.5f, 0.5f, 0.5f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szBALL_SPAWNER);

		// No model on the kill volume: it is a trigger, and drawing a 40x40 slab
		// under the world would just occlude the camera on the way past.
		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szKILL_VOLUME);
		xAuto.AddStep_SetTransformPosition(0.0f, -8.0f, 0.0f);
		xAuto.AddStep_SetTransformScale(40.0f, 1.0f, 40.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szKILL_VOLUME);

		ST_AddSun(xAuto);
		ST_AddKeyLight(xAuto);

		xAuto.AddStep_SaveScene(ScriptTest::Scenes::szGYM_PHYSICS_PATH);
		xAuto.AddStep_UnloadScene();
	}

	// ---- Scene 4: Gym_Events -----------------------------------------------
	// Both cross-entity messaging shapes at once: a TARGETED FireCustomEvent
	// (the plate opens one named door) and a BROADCAST (the bell reaches three
	// listeners that nothing names).
	void ST_AuthorGymEventsScene(Zenith_EditorAutomation& xAuto)
	{
		xAuto.AddStep_CreateScene("Gym_Events");

		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szGAME_MANAGER);
		ST_AddSceneCamera(xAuto, 0.0f, 8.0f, 18.0f, -0.4f);
		xAuto.AddStep_AddUI();
		ST_AddTitleText(xAuto, "Gym 4 - Events", 40.0f);
		ST_AddHintText(xAuto, "Walk onto the plate; B = bell");
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szESC_TO_HUB);	// slot 0
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szBELL_RING);	// slot 1

		ST_AddProp(xAuto, ScriptTest::Entities::szFLOOR, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_FLOOR], 0.0f, -0.5f, 0.0f, 15.0f, 0.5f, 15.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

		ST_AddProp(xAuto, ScriptTest::Entities::szPLAYER_CUBE, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PLAYER], 0.0f, 1.5f, -4.0f, 1.0f, 1.0f, 1.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szPLAYER_MOVE);	// slot 0
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szJUMP);			// slot 1

		// A static sensor. It only ever hears from DYNAMIC bodies, which is
		// exactly the player cube and nothing else in this scene.
		ST_AddProp(xAuto, ScriptTest::Entities::szPRESSURE_PLATE, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PROP], 4.0f, 0.1f, 0.0f, 2.0f, 0.2f, 2.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szPRESSURE_PLATE);

		ST_AddProp(xAuto, ScriptTest::Entities::szGYM_DOOR, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PROP], 8.0f, 1.5f, 0.0f, 0.4f, 3.0f, 3.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szDOOR);

		// Three entities, one graph asset. Nothing addresses them by name.
		ST_AddProp(xAuto, ScriptTest::Entities::szBELL_LISTENER_A, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PROP], -4.0f, 1.0f, -2.0f, 0.8f, 0.8f, 0.8f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szBELL_LISTENER);

		ST_AddProp(xAuto, ScriptTest::Entities::szBELL_LISTENER_B, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PROP], -5.0f, 1.0f, 0.0f, 0.8f, 0.8f, 0.8f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szBELL_LISTENER);

		ST_AddProp(xAuto, ScriptTest::Entities::szBELL_LISTENER_C, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PROP], -4.0f, 1.0f, 2.0f, 0.8f, 0.8f, 0.8f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szBELL_LISTENER);

		ST_AddSun(xAuto);
		ST_AddKeyLight(xAuto);

		xAuto.AddStep_SaveScene(ScriptTest::Scenes::szGYM_EVENTS_PATH);
		xAuto.AddStep_UnloadScene();
	}

	// ---- Scene 5: Gym_State ------------------------------------------------
	// Three lamps and no lamp logic. The lamps carry NO graph at all: the
	// StateMachine on the GameManager finds them by name and scales them, so
	// the whole cycle is one graph on one entity.
	void ST_AuthorGymStateScene(Zenith_EditorAutomation& xAuto)
	{
		xAuto.AddStep_CreateScene("Gym_State");

		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szGAME_MANAGER);
		ST_AddSceneCamera(xAuto, 0.0f, 4.0f, 10.0f, -0.2f);
		xAuto.AddStep_AddUI();
		ST_AddTitleText(xAuto, "Gym 5 - State", 40.0f);
		ST_AddHintText(xAuto, "Esc = hub");
		// Seeded to match the state variable's default (0 = Red), so the readout
		// is right for the three seconds before the first transition fires.
		xAuto.AddStep_CreateUIText(ScriptTest::UINames::szSTATE_NAME, "RED");
		xAuto.AddStep_SetUIAnchor(ScriptTest::UINames::szSTATE_NAME, static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIPosition(ScriptTest::UINames::szSTATE_NAME, 0.0f, -200.0f);
		xAuto.AddStep_SetUIFontSize(ScriptTest::UINames::szSTATE_NAME, 36.0f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szESC_TO_HUB);		// slot 0
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szTRAFFIC_LIGHT);	// slot 1

		ST_AddProp(xAuto, ScriptTest::Entities::szFLOOR, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_FLOOR], 0.0f, -0.5f, 0.0f, 6.0f, 0.5f, 6.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

		ST_AddProp(xAuto, ScriptTest::Entities::szLAMP_RED, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_RED], 0.0f, 4.0f, 0.0f, 1.0f, 1.0f, 1.0f);
		ST_AddProp(xAuto, ScriptTest::Entities::szLAMP_AMBER, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_AMBER], 0.0f, 2.75f, 0.0f, 1.0f, 1.0f, 1.0f);
		ST_AddProp(xAuto, ScriptTest::Entities::szLAMP_GREEN, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_GREEN], 0.0f, 1.5f, 0.0f, 1.0f, 1.0f, 1.0f);

		ST_AddSun(xAuto);
		ST_AddKeyLight(xAuto);

		xAuto.AddStep_SaveScene(ScriptTest::Scenes::szGYM_STATE_PATH);
		xAuto.AddStep_UnloadScene();
	}

	// ---- Scene 6: Gym_UI ---------------------------------------------------
	// Camera and UI only, like the hub (plus the shared sun for a matching sky
	// backdrop): every element here is driven by ST_UIPlayground.
	void ST_AuthorGymUIScene(Zenith_EditorAutomation& xAuto)
	{
		xAuto.AddStep_CreateScene("Gym_UI");

		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szGAME_MANAGER);
		ST_AddSceneCamera(xAuto, 0.0f, 2.0f, 6.0f, 0.0f);
		xAuto.AddStep_AddUI();

		ST_AddTitleText(xAuto, "Gym 6 - UI", 40.0f);

		// The three readouts the graph writes. Their seeded text matches what
		// the graph will print on its first tick.
		xAuto.AddStep_CreateUIText(ScriptTest::UINames::szCOUNTER, "Count: 0");
		xAuto.AddStep_SetUIAnchor(ScriptTest::UINames::szCOUNTER, static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIPosition(ScriptTest::UINames::szCOUNTER, 0.0f, -80.0f);
		xAuto.AddStep_SetUIFontSize(ScriptTest::UINames::szCOUNTER, 30.0f);

		xAuto.AddStep_CreateUIText(ScriptTest::UINames::szCLOCK, "t = 0.0s");
		xAuto.AddStep_SetUIAnchor(ScriptTest::UINames::szCLOCK, static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIPosition(ScriptTest::UINames::szCLOCK, 0.0f, -20.0f);
		xAuto.AddStep_SetUIFontSize(ScriptTest::UINames::szCLOCK, 24.0f);

		// SetUIFillAmount only accepts a Rect -- a wrong element type FAILS the
		// node rather than casting blindly, so this has to be a Rect.
		xAuto.AddStep_CreateUIRect(ScriptTest::UINames::szBAR_FILL);
		xAuto.AddStep_SetUIAnchor(ScriptTest::UINames::szBAR_FILL, static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIPosition(ScriptTest::UINames::szBAR_FILL, 0.0f, 40.0f);
		xAuto.AddStep_SetUISize(ScriptTest::UINames::szBAR_FILL, 300.0f, 24.0f);

		xAuto.AddStep_CreateUIButton(ScriptTest::UINames::szBTN_PLUS, "+");
		xAuto.AddStep_SetUIAnchor(ScriptTest::UINames::szBTN_PLUS, static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIPosition(ScriptTest::UINames::szBTN_PLUS, 80.0f, 100.0f);
		xAuto.AddStep_SetUISize(ScriptTest::UINames::szBTN_PLUS, 96.0f, 40.0f);

		xAuto.AddStep_CreateUIButton(ScriptTest::UINames::szBTN_MINUS, "-");
		xAuto.AddStep_SetUIAnchor(ScriptTest::UINames::szBTN_MINUS, static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIPosition(ScriptTest::UINames::szBTN_MINUS, -80.0f, 100.0f);
		xAuto.AddStep_SetUISize(ScriptTest::UINames::szBTN_MINUS, 96.0f, 40.0f);

		ST_AddHintText(xAuto, "Up/buttons change count");

		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szESC_TO_HUB);		// slot 0
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szUI_PLAYGROUND);	// slot 1

		ST_AddSun(xAuto);

		xAuto.AddStep_SaveScene(ScriptTest::Scenes::szGYM_UI_PATH);
		xAuto.AddStep_UnloadScene();
	}

	// ---- Scene 7: Gym_Flow -------------------------------------------------
	// The multi-way flow constructs, themed as a dispenser. Three entities carry
	// behaviour: the GameManager runs ST_Dispenser (fifteen chains), the Plate
	// arms it from OUTSIDE its graph, and the Nozzle is passive -- it is scaled
	// by whichever SwitchOnInt pin is live, which is how the switch's choice
	// becomes observable in the world rather than only on a blackboard.
	void ST_AuthorGymFlowScene(Zenith_EditorAutomation& xAuto)
	{
		xAuto.AddStep_CreateScene("Gym_Flow");

		xAuto.AddStep_CreateEntity(ScriptTest::Entities::szGAME_MANAGER);
		ST_AddSceneCamera(xAuto, 0.0f, 5.0f, 12.0f, -0.3f);
		xAuto.AddStep_AddUI();
		ST_AddTitleText(xAuto, "Gym 7 - Flow", 40.0f);
		ST_AddHintText(xAuto, "P arm, Space dispense, M mode, A alarm, F/R/C bag");
		ST_AddCornerReadout(xAuto, ScriptTest::UINames::szDISPENSED, "Dispensed: 0", 20.0f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szESC_TO_HUB);	// slot 0
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szDISPENSER);	// slot 1

		ST_AddProp(xAuto, ScriptTest::Entities::szFLOOR, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_FLOOR], 0.0f, -0.5f, 0.0f, 10.0f, 0.5f, 10.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

		// No collider on either of these two: nothing in this gym is physical.
		// The nozzle is moved only by SetEntityScale, and the plate is driven by
		// a key, not by a body entering it.
		ST_AddProp(xAuto, ScriptTest::Entities::szNOZZLE, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_PROP], 0.0f, 2.0f, 0.0f, 1.0f, 1.0f, 1.0f);

		ST_AddProp(xAuto, ScriptTest::Entities::szPLATE, ScriptTest::Meshes::szUNIT_CUBE_MODEL,
			g_apxMaterials[ST_MATERIAL_RED], -4.0f, 0.1f, 2.0f, 2.0f, 0.2f, 2.0f);
		xAuto.AddStep_AttachGraph(ScriptTest::Graphs::szFLOW_PLATE);

		ST_AddSun(xAuto);
		ST_AddKeyLight(xAuto);

		xAuto.AddStep_SaveScene(ScriptTest::Scenes::szGYM_FLOW_PATH);
		xAuto.AddStep_UnloadScene();
	}
}

void Project_RegisterEditorAutomationSteps()
{
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();

	// ---- 1. The eighteen graphs, regenerated from their builders every boot.
	// Before the scenes, because AddStep_AttachGraph resolves an asset PATH and
	// a scene authored first would attach a slot pointing at a stale .bgraph.
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szESC_TO_HUB,     &BuildGraph_ST_EscToHub);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szHUB_FLOW,       &BuildGraph_ST_HubFlow);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szSPIN,           &BuildGraph_ST_Spin);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szPING_PONG,      &BuildGraph_ST_PingPong);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szSINE_BOB,       &BuildGraph_ST_SineBob);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szPLAYER_MOVE,    &BuildGraph_ST_PlayerMove);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szJUMP,           &BuildGraph_ST_Jump);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szBALL_SPAWNER,   &BuildGraph_ST_BallSpawner);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szKILL_VOLUME,    &BuildGraph_ST_KillVolume);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szPRESSURE_PLATE, &BuildGraph_ST_PressurePlate);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szDOOR,           &BuildGraph_ST_Door);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szBELL_RING,      &BuildGraph_ST_BellRing);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szBELL_LISTENER,  &BuildGraph_ST_BellListener);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szTRAFFIC_LIGHT,  &BuildGraph_ST_TrafficLight);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szUI_PLAYGROUND,  &BuildGraph_ST_UIPlayground);
	// ST_FlowScore before ST_Dispenser: CallGraph resolves its child by ASSET
	// PATH at runtime rather than at build time, so the order is not strictly
	// load-bearing -- but every other producer in this list is written before
	// its consumer, and a reader should not have to check which.
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szFLOW_SCORE,     &BuildGraph_ST_FlowScore);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szDISPENSER,      &BuildGraph_ST_Dispenser);
	xAuto.AddStep_GraphBuild(ScriptTest::Graphs::szFLOW_PLATE,     &BuildGraph_ST_FlowPlate);

	// ---- 2. The ST_Ball prefab.
	// CreatePrefabFromSelected needs a live entity to capture, so it gets a
	// scratch scene -- which is NEVER saved and is unloaded immediately. An
	// authored-and-saved scratch scene would be an eighth .zscen in the repo
	// that nothing loads.
	xAuto.AddStep_CreateScene(ScriptTest::Scenes::szPREFAB_SCRATCH);
	xAuto.AddStep_CreateEntity(ScriptTest::Entities::szBALL_TEMPLATE);
	xAuto.AddStep_AddModel();
	xAuto.AddStep_LoadModel(ScriptTest::Meshes::szUNIT_SPHERE_MODEL);
	xAuto.AddStep_SetModelMaterial(0, g_apxMaterials[ST_MATERIAL_BALL]);
	xAuto.AddStep_SetTransformScale(0.5f, 0.5f, 0.5f);
	xAuto.AddStep_AddCollider();
	xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
	xAuto.AddStep_CreatePrefabFromSelected(ScriptTest::Prefabs::szBALL_NAME, ScriptTest::Prefabs::szBALL_SAVE_PATH);
	xAuto.AddStep_UnloadScene();

	// ---- 3. The eight scenes, in build-index order.
	ST_AuthorHubScene(xAuto);
	ST_AuthorGymMotionScene(xAuto);
	ST_AuthorGymInputScene(xAuto);
	ST_AuthorGymPhysicsScene(xAuto);
	ST_AuthorGymEventsScene(xAuto);
	ST_AuthorGymStateScene(xAuto);
	ST_AuthorGymUIScene(xAuto);
	ST_AuthorGymFlowScene(xAuto);

	// ---- 4. And only then boot into the hub.
	xAuto.AddStep_LoadInitialScene(&Project_LoadInitialScene);
}

ZENITH_AUTHORING_DETERMINISM_END

#endif	// ZENITH_TOOLS

//=============================================================================
// Initial scene load
//
// Unconditional: a _False boot runs no automation, so this is the ONLY place
// the eight build indices get registered. Every LoadSceneByIndex node in
// ST_HubFlow and ST_EscToHub depends on this table, which is why the indices
// are constants in ScriptTest_Graphs.h rather than literals in two places.
//=============================================================================

void Project_LoadInitialScene()
{
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();

	xScenes.RegisterSceneBuildIndex(ScriptTest::Scenes::iHUB,         ScriptTest::Scenes::szHUB_PATH);
	xScenes.RegisterSceneBuildIndex(ScriptTest::Scenes::iGYM_MOTION,  ScriptTest::Scenes::szGYM_MOTION_PATH);
	xScenes.RegisterSceneBuildIndex(ScriptTest::Scenes::iGYM_INPUT,   ScriptTest::Scenes::szGYM_INPUT_PATH);
	xScenes.RegisterSceneBuildIndex(ScriptTest::Scenes::iGYM_PHYSICS, ScriptTest::Scenes::szGYM_PHYSICS_PATH);
	xScenes.RegisterSceneBuildIndex(ScriptTest::Scenes::iGYM_EVENTS,  ScriptTest::Scenes::szGYM_EVENTS_PATH);
	xScenes.RegisterSceneBuildIndex(ScriptTest::Scenes::iGYM_STATE,   ScriptTest::Scenes::szGYM_STATE_PATH);
	xScenes.RegisterSceneBuildIndex(ScriptTest::Scenes::iGYM_UI,      ScriptTest::Scenes::szGYM_UI_PATH);
	xScenes.RegisterSceneBuildIndex(ScriptTest::Scenes::iGYM_FLOW,    ScriptTest::Scenes::szGYM_FLOW_PATH);

	xScenes.LoadSceneByIndex(ScriptTest::Scenes::iHUB, SCENE_LOAD_SINGLE);
}
