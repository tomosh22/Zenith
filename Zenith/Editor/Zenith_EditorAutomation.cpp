#include "Zenith.h"

#include "Core/Zenith_PlatformStdio.h"
#include "Core/Zenith_Engine.h"
#ifdef ZENITH_TOOLS

#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_SunComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_NavMeshComponent.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Editor/Panels/Zenith_EditorPanel_GraphEditor.h"
#include "Editor/Panels/Zenith_EditorPanel_MaterialEditor.h"
#include "Editor/Panels/Zenith_EditorPanel_Animation.h"
#include "Editor/Panels/Zenith_EditorPanel_AnimStateMachine.h"
#include "Flux/Flux_ModelInstance.h"
#include "UI/Zenith_UI.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "AssetHandling/Zenith_GrassTypeTableAsset.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "EntityComponent/Zenith_GraphReload.h"
#include "Prefab/Zenith_Prefab.h"
#include "Maths/Zenith_Maths.h"
#include "DataStream/Zenith_DataStream.h"
// Boot-tail attribution: the timebase + the milestone sink (reached through the
// INJECTED profiler pointer, never g_xEngine), and the dump path for .tail.txt.
#include "Core/Zenith_CommandLine.h"
#include "Profiling/Zenith_Profiling.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

bool Zenith_EditorAutomation::IsRunning()  { return Zenith_EditorAutomation::m_bRunning; }
bool Zenith_EditorAutomation::IsComplete() { return Zenith_EditorAutomation::m_bComplete; }

//=============================================================================
// Execution
//=============================================================================

// A step slow enough to be worth naming in the log as it happens, rather than only in
// the completion summary. Automation runs one step per FRAME, so anything past this is
// a visible hitch on the way to the first interactive frame.
static constexpr double fAUTOMATION_SLOW_STEP_MS = 100.0;

void Zenith_EditorAutomation::Begin(bool bProductionTail, Zenith_Profiling* pxProfiling)
{
	m_uCurrentAction = 0;
	m_bRunning = true;
	m_bComplete = false;

	m_bProductionTail = bProductionTail;
	m_pxProfiling = pxProfiling;
	m_xStepTimings.Clear();
	m_fTotalStepMs = 0.0;
	m_uUntrackedSteps = 0;

	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Begin: %u steps queued%s",
		m_axActions.GetSize(), bProductionTail ? " (production tail: timing enabled)" : "");
}

// The queue just drained. For the production session this is the end of the "boot
// tail" — the stretch after Zenith_Init returns during which the game is still
// building itself one step per frame, and which the boot report alone cannot see.
void Zenith_EditorAutomation::FinishSession()
{
	m_bRunning = false;
	m_bComplete = true;
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Complete: all %u steps executed", m_axActions.GetSize());
	m_axActions.Clear();

	if (!m_bProductionTail) return;

	WriteTailReport(stdout);
	fflush(stdout);

	if (m_pxProfiling != nullptr)
	{
		m_pxProfiling->RecordBootMilestone("AutomationQueueDrained");
	}

	// Second bounded artifact, alongside the boot dump, when one was requested.
	const char* szBootDump = Zenith_CommandLine::GetBootProfileDumpPath();
	if (szBootDump != nullptr)
	{
		const std::string strTailPath = std::string(szBootDump) + ".tail.txt";
		FILE* pxTail = Zenith_PlatformStdio::OpenFile(strTailPath.c_str(), "w");
		if (pxTail != nullptr)
		{
			WriteTailReport(pxTail);
			fclose(pxTail);
			Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Tail report -> %s", strTailPath.c_str());
		}
	}
}

void Zenith_EditorAutomation::ExecuteNextStep()
{
	if (!m_bRunning || m_bComplete)
		return;

	if (m_uCurrentAction >= m_axActions.GetSize())
	{
		FinishSession();
		return;
	}

	const Zenith_EditorAction& xAction = m_axActions.Get(m_uCurrentAction);
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] Step %u/%u", m_uCurrentAction + 1, m_axActions.GetSize());

	// Wall clock, not a profile zone: a step spans a whole frame's worth of editor
	// work and the boot capture is already sealed by the time any of this runs.
	const u_int64 uStepBegin = m_bProductionTail ? Zenith_Profiling_Detail::GetTimestamp() : 0;

	ExecuteAction(xAction);

	if (m_bProductionTail)
	{
		const double fStepMs = static_cast<double>(Zenith_Profiling_Detail::GetTimestamp() - uStepBegin)
			* Zenith_Profiling_Detail::GetTicksToNs() / 1.0e6;
		RecordStepTiming(xAction, m_uCurrentAction, fStepMs);
	}

	m_uCurrentAction++;

	// Detect completion immediately after executing the last step
	if (m_uCurrentAction >= m_axActions.GetSize())
	{
		FinishSession();
	}
}

void Zenith_EditorAutomation::RecordStepTiming(const Zenith_EditorAction& xAction, const u_int uIndex, const double fStepMs)
{
	m_fTotalStepMs += fStepMs;

	if (fStepMs > fAUTOMATION_SLOW_STEP_MS)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorAutomation] SLOW step %u '%s' took %.1f ms",
			uIndex, DescribeStep(xAction, uIndex).c_str(), fStepMs);
	}

	if (m_xStepTimings.GetSize() >= uMAX_TRACKED_STEPS)
	{
		++m_uUntrackedSteps;   // never silently: WriteTailReport prints the count
		return;
	}

	StepTiming xTiming;
	xTiming.m_strName = DescribeStep(xAction, uIndex);
	xTiming.m_fMilliseconds = fStepMs;
	xTiming.m_uIndex = uIndex;
	m_xStepTimings.PushBack(xTiming);
}

// An explicit name when the author gave one, otherwise index + action-type id — enough
// to find the step in Project_RegisterEditorAutomationSteps without naming them all.
std::string Zenith_EditorAutomation::DescribeStep(const Zenith_EditorAction& xAction, const u_int uIndex)
{
	if (!xAction.m_szStepName.empty()) return xAction.m_szStepName;

	char acName[64];
	snprintf(acName, sizeof(acName), "step %u (type %d)", uIndex, static_cast<int>(xAction.m_eType));
	return std::string(acName);
}

namespace
{
	// Descending by cost. Returns indices so the stored order stays execution order.
	Zenith_Vector<u_int> SortStepsByCost(const Zenith_Vector<Zenith_EditorAutomation::StepTiming>& xTimings)
	{
		Zenith_Vector<u_int> xOrder;
		for (u_int u = 0; u < xTimings.GetSize(); ++u) xOrder.PushBack(u);
		std::sort(xOrder.GetDataPointer(), xOrder.GetDataPointer() + xOrder.GetSize(),
			[&xTimings](const u_int uA, const u_int uB) { return xTimings.Get(uA).m_fMilliseconds > xTimings.Get(uB).m_fMilliseconds; });
		return xOrder;
	}
}

void Zenith_EditorAutomation::WriteStepsSoFar(FILE* pxFile) const
{
	if (pxFile == nullptr) return;

	fprintf(pxFile, "\n=== Automation steps so far (%u executed, %.1f ms total) ===\n",
		m_uCurrentAction, m_fTotalStepMs);

	if (!m_bProductionTail)
	{
		fprintf(pxFile, "(no production automation session — nothing to attribute)\n");
		return;
	}

	fprintf(pxFile, "%-52s %12s\n", "Step", "ms");
	fprintf(pxFile, "---------------------------------------------------- ------------\n");
	for (u_int u = 0; u < m_xStepTimings.GetSize(); ++u)
	{
		const StepTiming& xTiming = m_xStepTimings.Get(u);
		fprintf(pxFile, "%-52s %12.3f\n", xTiming.m_strName.c_str(), xTiming.m_fMilliseconds);
	}
	if (m_uUntrackedSteps > 0)
	{
		fprintf(pxFile, "(+%u further steps executed but not tracked — cap %u)\n", m_uUntrackedSteps, uMAX_TRACKED_STEPS);
	}
}

void Zenith_EditorAutomation::WriteTailReport(FILE* pxFile) const
{
	if (pxFile == nullptr) return;

	fprintf(pxFile, "\n=== Automation tail (post-Zenith_Init, one step per frame) ===\n");
	if (!m_bProductionTail)
	{
		fprintf(pxFile, "(no production automation session — nothing to attribute)\n\n");
		return;
	}

	fprintf(pxFile, "Steps: %u tracked", m_xStepTimings.GetSize());
	if (m_uUntrackedSteps > 0) fprintf(pxFile, " (+%u untracked, cap %u)", m_uUntrackedSteps, uMAX_TRACKED_STEPS);
	fprintf(pxFile, " | Total: %.1f ms\n", m_fTotalStepMs);

	const Zenith_Vector<u_int> xOrder = SortStepsByCost(m_xStepTimings);
	const u_int uShow = xOrder.GetSize() < 10u ? xOrder.GetSize() : 10u;

	fprintf(pxFile, "\n%-52s %12s %10s\n", "Slowest steps", "ms", "% of tail");
	fprintf(pxFile, "---------------------------------------------------- ------------ ----------\n");
	for (u_int u = 0; u < uShow; ++u)
	{
		const StepTiming& xTiming = m_xStepTimings.Get(xOrder.Get(u));
		const double fShare = (m_fTotalStepMs > 0.0) ? (xTiming.m_fMilliseconds / m_fTotalStepMs) * 100.0 : 0.0;
		fprintf(pxFile, "%-52s %12.3f %10.1f\n", xTiming.m_strName.c_str(), xTiming.m_fMilliseconds, fShare);
	}
	fprintf(pxFile, "\n");
}

void Zenith_EditorAutomation::Reset()
{
	m_axActions.Clear();
	m_uCurrentAction = 0;
	m_bRunning = false;
	m_bComplete = false;
	m_bProductionTail = false;
	m_pxProfiling = nullptr;
	m_xStepTimings.Clear();
	m_fTotalStepMs = 0.0;
	m_uUntrackedSteps = 0;
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

	// nullptr-safe: Zenith_EditorAction's string members own their storage, so
	// a null caller pointer (e.g. AddStep_MaterialSetParent(nullptr) to clear
	// the parent) becomes an empty string rather than dereferencing null.
	inline std::string SafeStr(const char* sz) { return sz ? std::string(sz) : std::string(); }

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
		xAction.m_szArg1 = SafeStr(sz);
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz1, const char* sz2)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = SafeStr(sz1);
		xAction.m_szArg2 = SafeStr(sz2);
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, bool b)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = SafeStr(sz);
		xAction.m_bArg = b;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, int i)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = SafeStr(sz);
		xAction.m_aiArgs[0] = i;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = SafeStr(sz);
		xAction.m_afArgs[0] = f;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f1, float f2)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = SafeStr(sz);
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f1, float f2, float f3)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = SafeStr(sz);
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xAction.m_afArgs[2] = f3;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f1, float f2, float f3, float f4)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = SafeStr(sz);
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
		xAction.m_szArg1 = SafeStr(sz);
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xAction.m_bArg = b;
		xActions.PushBack(xAction);
	}
	inline void Push(ActionList& xActions, ActionType eType, const char* sz, float f1, float f2, float f3, bool b)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_szArg1 = SafeStr(sz);
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
		xAction.m_szArg1 = SafeStr(sz);
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
	inline void Push(ActionList& xActions, ActionType eType, float f1, float f2)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
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
	inline void Push(ActionList& xActions, ActionType eType, float f1, float f2, float f3, float f4)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xAction.m_afArgs[2] = f3;
		xAction.m_afArgs[3] = f4;
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
	inline void Push(ActionList& xActions, ActionType eType, float f1, float f2, int i1)
	{
		Zenith_EditorAction xAction = {};
		xAction.m_eType = eType;
		xAction.m_afArgs[0] = f1;
		xAction.m_afArgs[1] = f2;
		xAction.m_aiArgs[0] = i1;
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
void Zenith_EditorAutomation::AddStep_SetTransformRotationEuler(float fXDeg, float fYDeg, float fZDeg) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_TRANSFORM_ROTATION, fXDeg, fYDeg, fZDeg); }
void Zenith_EditorAutomation::AddStep_SetTransformRotationQuat(float fX, float fY, float fZ, float fW) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_TRANSFORM_ROTATION_QUAT, fX, fY, fZ, fW); }

// ATTACH_TO_BONE packs 2 names + 6 floats (pos[0..2], euler[3..5]) — no Push overload
// covers that shape, so the action is constructed directly.
void Zenith_EditorAutomation::AddStep_AttachToBone(const char* szTargetEntityName, const char* szBone,
	float fPosX, float fPosY, float fPosZ,
	float fEulerXDeg, float fEulerYDeg, float fEulerZDeg)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ATTACH_TO_BONE;
	xAction.m_szArg1 = SafeStr(szTargetEntityName);
	xAction.m_szArg2 = SafeStr(szBone);
	xAction.m_afArgs[0] = fPosX;
	xAction.m_afArgs[1] = fPosY;
	xAction.m_afArgs[2] = fPosZ;
	xAction.m_afArgs[3] = fEulerXDeg;
	xAction.m_afArgs[4] = fEulerYDeg;
	xAction.m_afArgs[5] = fEulerZDeg;
	Zenith_EditorAutomation::m_axActions.PushBack(xAction);
}

// -- Authoring math (shared by the executors; pure, unit-testable) --
//
// Deterministic-FP: these two build the rotations that get SERIALIZED into scene
// files, so they must produce the same bytes in a Debug and a Release tools build.
ZENITH_AUTHORING_DETERMINISM_BEGIN

Zenith_Maths::Quat Zenith_EditorAutomation::BuildEulerRotation(float fEulerXDeg, float fEulerYDeg, float fEulerZDeg)
{
	// Ry * Rx * Rz (quaternion product = the same composition glm::rotate applies in
	// RT_BuildJetpackMount: yaw about Y, then pitch about X, then roll about Z).
	// Zenith_Maths::Authoring* rather than glm: same values, but compiled once under
	// a pinned FP model, so this rotation lands in a committed scene as the same
	// bytes from a Debug and a Release tools boot.
	const Zenith_Maths::Quat xYaw   = Zenith_Maths::AuthoringRotationY(Zenith_Maths::AuthoringRadians(fEulerYDeg));
	const Zenith_Maths::Quat xPitch = Zenith_Maths::AuthoringRotationX(Zenith_Maths::AuthoringRadians(fEulerXDeg));
	const Zenith_Maths::Quat xRoll  = Zenith_Maths::AuthoringRotationZ(Zenith_Maths::AuthoringRadians(fEulerZDeg));
	return Zenith_Maths::AuthoringQuatMul(Zenith_Maths::AuthoringQuatMul(xYaw, xPitch), xRoll);
}

Zenith_Maths::Matrix4 Zenith_EditorAutomation::BuildEulerOffsetMatrix(float fPosX, float fPosY, float fPosZ,
	float fEulerXDeg, float fEulerYDeg, float fEulerZDeg)
{
	// M = T(pos) * Ry * Rx * Rz — identical to RT_BuildJetpackMount.
	return Zenith_Maths::AuthoringTRS(Zenith_Maths::Vector3(fPosX, fPosY, fPosZ),
		BuildEulerRotation(fEulerXDeg, fEulerYDeg, fEulerZDeg),
		Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));
}

ZENITH_AUTHORING_DETERMINISM_END

// -- Light --

void Zenith_EditorAutomation::AddStep_SetLightIntensity(float fLumens)              { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_LIGHT_INTENSITY, fLumens); }
void Zenith_EditorAutomation::AddStep_SetLightRange    (float fMetres)              { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_LIGHT_RANGE,     fMetres); }
void Zenith_EditorAutomation::AddStep_SetLightColor    (float fR, float fG, float fB) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_LIGHT_COLOR, fR, fG, fB); }
void Zenith_EditorAutomation::AddStep_SetLightPositionOffset(float fX, float fY, float fZ) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_LIGHT_POSITION_OFFSET, fX, fY, fZ); }

// -- Sun --

void Zenith_EditorAutomation::AddStep_SetSunDirection(float fX, float fY, float fZ) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_SUN_DIRECTION, fX, fY, fZ); }
void Zenith_EditorAutomation::AddStep_SetSunTimeOfDay(float fAngleDegrees, float fOrbitAzimuthDegrees) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_SUN_TIME_OF_DAY, fAngleDegrees, fOrbitAzimuthDegrees); }
void Zenith_EditorAutomation::AddStep_SetNavMeshAsset(const char* szAssetRef) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_NAVMESH_ASSET, szAssetRef); }

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
	xAction.m_szArg1 = SafeStr(szElement);
	xAction.m_szArg2 = SafeStr(szUp);
	xAction.m_szArg3 = SafeStr(szDown);
	xAction.m_szArg4 = SafeStr(szLeft);
	xAction.m_szArg5 = SafeStr(szRight);
	m_axActions.PushBack(xAction);
}

// -- UI ScrollView --

void Zenith_EditorAutomation::AddStep_CreateUIScrollView         (const char* szName)                                            { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_SCROLL_VIEW,             szName); }
void Zenith_EditorAutomation::AddStep_SetUIScrollViewContentSize (const char* szElement, float fW, float fH)                    { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_SCROLL_VIEW_CONTENT_SIZE,   szElement, fW, fH); }

// -- UI on-screen controls (B9) --

void Zenith_EditorAutomation::AddStep_CreateUIVirtualStick           (const char* szName)                                     { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_VIRTUAL_STICK,              szName); }
void Zenith_EditorAutomation::AddStep_SetUIVirtualStickAction        (const char* szElement, const char* szActionName)        { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_VIRTUAL_STICK_ACTION,          szElement, szActionName); }
void Zenith_EditorAutomation::AddStep_SetUIVirtualStickMode          (const char* szElement, int iMode)                       { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_VIRTUAL_STICK_MODE,            szElement, iMode); }
void Zenith_EditorAutomation::AddStep_SetUIVirtualStickRadius        (const char* szElement, float fLogicalPx)                { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_VIRTUAL_STICK_RADIUS,          szElement, fLogicalPx); }
void Zenith_EditorAutomation::AddStep_SetUIVirtualStickDeadzone      (const char* szElement, float fFraction)                 { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_VIRTUAL_STICK_DEADZONE,        szElement, fFraction); }
void Zenith_EditorAutomation::AddStep_SetUIVirtualStickActivationSlop(const char* szElement, float fLogicalPx)                { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_VIRTUAL_STICK_ACTIVATION_SLOP, szElement, fLogicalPx); }

void Zenith_EditorAutomation::AddStep_CreateUIVirtualButton          (const char* szName)                                     { Push(Zenith_EditorAutomation::m_axActions, ActionType::CREATE_UI_VIRTUAL_BUTTON,             szName); }
void Zenith_EditorAutomation::AddStep_SetUIVirtualButtonAction       (const char* szElement, const char* szActionName)        { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_VIRTUAL_BUTTON_ACTION,         szElement, szActionName); }
void Zenith_EditorAutomation::AddStep_SetUIVirtualButtonHitSlop      (const char* szElement, float fLogicalPx)                { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_UI_VIRTUAL_BUTTON_HIT_SLOP,       szElement, fLogicalPx); }

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

// -- Graph --

void Zenith_EditorAutomation::AddStep_AttachGraph                 (const char* szGraphAssetPath)    { Push(Zenith_EditorAutomation::m_axActions, ActionType::ATTACH_GRAPH, szGraphAssetPath); }

// Graph authoring (each step = one atomic editor action; see header).

void Zenith_EditorAutomation::AddStep_GraphOpenFresh        (const char* szAssetPath)               { Push(Zenith_EditorAutomation::m_axActions, ActionType::GRAPH_OPEN_FRESH, szAssetPath); }
void Zenith_EditorAutomation::AddStep_GraphAddNode          (const char* szTypeName)                { Push(Zenith_EditorAutomation::m_axActions, ActionType::GRAPH_ADD_NODE, szTypeName); }
void Zenith_EditorAutomation::AddStep_GraphSave             ()                                      { Push(Zenith_EditorAutomation::m_axActions, ActionType::GRAPH_SAVE); }
void Zenith_EditorAutomation::AddStep_GraphClose            ()                                      { Push(Zenith_EditorAutomation::m_axActions, ActionType::GRAPH_CLOSE); }

// ---- Material editor authoring steps ----
void Zenith_EditorAutomation::AddStep_MaterialCreate        (const char* szAssetPath)               { Push(Zenith_EditorAutomation::m_axActions, ActionType::MATERIAL_CREATE, szAssetPath); }
void Zenith_EditorAutomation::AddStep_MaterialOpen          (const char* szAssetPath)               { Push(Zenith_EditorAutomation::m_axActions, ActionType::MATERIAL_OPEN, szAssetPath); }
void Zenith_EditorAutomation::AddStep_MaterialSave          (const char* szAssetPath)               { Push(Zenith_EditorAutomation::m_axActions, ActionType::MATERIAL_SAVE, szAssetPath); }
void Zenith_EditorAutomation::AddStep_MaterialSetParent     (const char* szParentAssetPath)         { Push(Zenith_EditorAutomation::m_axActions, ActionType::MATERIAL_SET_PARENT, szParentAssetPath); }
void Zenith_EditorAutomation::AddStep_MaterialSetTexture    (const char* szSlotName, const char* szTexturePath) { Push(Zenith_EditorAutomation::m_axActions, ActionType::MATERIAL_SET_TEXTURE, szSlotName, szTexturePath); }
void Zenith_EditorAutomation::AddStep_MaterialSetParamFloat (const char* szParamName, float fValue) { Push(Zenith_EditorAutomation::m_axActions, ActionType::MATERIAL_SET_PARAM_FLOAT, szParamName, fValue); }
void Zenith_EditorAutomation::AddStep_MaterialSetParamInt   (const char* szParamName, int iValue)   { Push(Zenith_EditorAutomation::m_axActions, ActionType::MATERIAL_SET_PARAM_INT, szParamName, iValue); }
void Zenith_EditorAutomation::AddStep_MaterialSetOverride   (const char* szParamName, bool bOverridden) { Push(Zenith_EditorAutomation::m_axActions, ActionType::MATERIAL_SET_OVERRIDE, szParamName, bOverridden); }

void Zenith_EditorAutomation::AddStep_MaterialSetParamColor(const char* szParamName, float fR, float fG, float fB, float fA)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::MATERIAL_SET_PARAM_COLOR;
	xAction.m_szArg1 = SafeStr(szParamName);
	xAction.m_afArgs[0] = fR; xAction.m_afArgs[1] = fG; xAction.m_afArgs[2] = fB; xAction.m_afArgs[3] = fA;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_MaterialSetPreviewMesh(int iMesh)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::MATERIAL_SET_PREVIEW_MESH;
	xAction.m_aiArgs[0] = iMesh;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_MaterialSetPreviewLight(float fYaw, float fPitch)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::MATERIAL_SET_PREVIEW_LIGHT;
	xAction.m_afArgs[0] = fYaw; xAction.m_afArgs[1] = fPitch;
	m_axActions.PushBack(xAction);
}

// ---- Grass-type authoring steps ----
// The type INDEX rides aiArgs[0] on every step that names one, so the executor
// reads it from one place; the param name rides szArg1 like the material verbs'.

void Zenith_EditorAutomation::AddStep_GrassTypesCreate() { Push(Zenith_EditorAutomation::m_axActions, ActionType::GRASS_TYPES_CREATE); }
void Zenith_EditorAutomation::AddStep_GrassTypesSave  () { Push(Zenith_EditorAutomation::m_axActions, ActionType::GRASS_TYPES_SAVE); }

void Zenith_EditorAutomation::AddStep_GrassTypesSetCount(int iCount)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRASS_TYPES_SET_COUNT;
	xAction.m_aiArgs[0] = iCount;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GrassTypesSetName(int iType, const char* szName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRASS_TYPES_SET_NAME;
	xAction.m_szArg1 = SafeStr(szName);
	xAction.m_aiArgs[0] = iType;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GrassTypesSetParamFloat(int iType, const char* szParam, float fValue)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRASS_TYPES_SET_PARAM_FLOAT;
	xAction.m_szArg1 = SafeStr(szParam);
	xAction.m_aiArgs[0] = iType;
	xAction.m_afArgs[0] = fValue;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GrassTypesSetParamColor(int iType, const char* szParam, float fR, float fG, float fB)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRASS_TYPES_SET_PARAM_COLOR;
	xAction.m_szArg1 = SafeStr(szParam);
	xAction.m_aiArgs[0] = iType;
	xAction.m_afArgs[0] = fR; xAction.m_afArgs[1] = fG; xAction.m_afArgs[2] = fB;
	m_axActions.PushBack(xAction);
}

// ---- Animation dope-sheet authoring steps (WU-3.4) ----
// The payload contract, in one place so the executor reads it from one place
// too: szArg1 is the BONE NAME (empty = root motion) or the asset path,
// aiArgs[0] the Flux_AnimTrack, aiArgs[1] the KEY INDEX (resolved to a stable
// id at execution — see the header), aiArgs[2] the Zenith_AnimSelectMode, and
// afArgs the seconds / pixels the verb takes.

void Zenith_EditorAutomation::AddStep_AnimOpenClip (const char* szAssetPath) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_OPEN_CLIP, szAssetPath); }
void Zenith_EditorAutomation::AddStep_AnimCloseClip()                        { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_CLOSE_CLIP); }
void Zenith_EditorAutomation::AddStep_AnimDeleteSelection()                  { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_DELETE_SELECTION); }
void Zenith_EditorAutomation::AddStep_AnimDuplicateSelection()               { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_DUPLICATE_SELECTION); }
void Zenith_EditorAutomation::AddStep_AnimCopySelection()                    { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_COPY_SELECTION); }
void Zenith_EditorAutomation::AddStep_AnimUndo()                             { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_UNDO); }
void Zenith_EditorAutomation::AddStep_AnimRedo()                             { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_REDO); }
void Zenith_EditorAutomation::AddStep_AnimScrub      (float fTimeSeconds)     { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SCRUB, fTimeSeconds); }
void Zenith_EditorAutomation::AddStep_AnimSetDuration(float fDurationSeconds) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SET_DURATION, fDurationSeconds); }
void Zenith_EditorAutomation::AddStep_AnimRippleRetime(float fFromSeconds, float fDeltaSeconds) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_RIPPLE_RETIME, fFromSeconds, fDeltaSeconds); }
void Zenith_EditorAutomation::AddStep_AnimPasteToBone(const char* szBone, float fTimeOffsetSeconds) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_PASTE_TO_BONE, szBone, fTimeOffsetSeconds); }

void Zenith_EditorAutomation::AddStep_AnimSelectKey(const char* szBone, int iTrack, int iKeyIndex, int iSelectMode)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_SELECT_KEY;
	xAction.m_szArg1 = SafeStr(szBone);
	xAction.m_aiArgs[0] = iTrack;
	xAction.m_aiArgs[1] = iKeyIndex;
	xAction.m_aiArgs[2] = iSelectMode;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimBoxSelect(float fX0, float fY0, float fX1, float fY1, int iSelectMode)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_BOX_SELECT;
	xAction.m_afArgs[0] = fX0; xAction.m_afArgs[1] = fY0;
	xAction.m_afArgs[2] = fX1; xAction.m_afArgs[3] = fY1;
	xAction.m_aiArgs[2] = iSelectMode;	// aiArgs[2] carries the mode on EVERY select verb
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimMoveSelection(float fDeltaSeconds, bool bSnap)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_MOVE_SELECTION;
	xAction.m_afArgs[0] = fDeltaSeconds;
	xAction.m_bArg = bSnap;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimExpectKeyTime(const char* szBone, int iTrack, int iKeyIndex,
	float fExpectedSeconds, float fToleranceSeconds)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_EXPECT_KEY_TIME;
	xAction.m_szArg1 = SafeStr(szBone);
	xAction.m_aiArgs[0] = iTrack;
	xAction.m_aiArgs[1] = iKeyIndex;
	xAction.m_afArgs[0] = fExpectedSeconds;
	xAction.m_afArgs[1] = fToleranceSeconds;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimExpectSelectedCount(int iExpectedCount)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_EXPECT_SELECTED_COUNT;
	xAction.m_aiArgs[0] = iExpectedCount;
	m_axActions.PushBack(xAction);
}

// ---- Animation POSE authoring steps (WU-4.3) ----
// The payload contract for the ANIM_POSE block, in one place so the executor
// reads it from one place too: aiArgs[0] is the BONE INDEX, afArgs[0..2] the
// rotation AXIS, afArgs[3] the angle in DEGREES, afArgs[0..3] the expected
// quaternion in SERIALIZED (x, y, z, w) order on the assertion step with
// afArgs[4] its tolerance, and bArg the auto-key flag.
//
// ★ A BONE INDEX, NOT A NAME, and that is the opposite choice from the key
// verbs above (which take a bone NAME and a key INDEX). A key index is unstable
// because a retime reorders a track; a bone index is not — the skeleton's bone
// order is fixed by the asset, it is what Zenith_AnimationPreviewSession's
// selection and pick set both speak, and a rig swap that changes it drops the
// selection outright rather than retargeting it.

void Zenith_EditorAutomation::AddStep_AnimSetKeyForSelectedBone() { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_POSE_SET_KEY_FOR_SELECTED_BONE); }

void Zenith_EditorAutomation::AddStep_AnimSelectBone(int iBoneIndex)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_POSE_SELECT_BONE;
	xAction.m_aiArgs[0] = iBoneIndex;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimRotateSelectedBoneWorld(float fAxisX, float fAxisY, float fAxisZ,
	float fAngleDegrees)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_POSE_ROTATE_SELECTED_BONE_WORLD;
	xAction.m_afArgs[0] = fAxisX;
	xAction.m_afArgs[1] = fAxisY;
	xAction.m_afArgs[2] = fAxisZ;
	xAction.m_afArgs[3] = fAngleDegrees;
	m_axActions.PushBack(xAction);
}

// ---- Animator-controller STATE MACHINE steps (WU-6.5) ----
// The payload contract for the ANIM_SM block, in one place so the executor reads
// it from one place too:
//   szArg1  — the asset path, the STATE NAME (empty = the any-state list) or the
//             parameter name, depending on the verb
//   szArg2  — the second name: a transition TARGET, a new state name, a clip
//             name, or the condition's parameter name
//   aiArgs[0] — the transition INDEX, the layer id, the parameter TYPE or the
//               expected state count
//   aiArgs[1] — the condition index
//   afArgs[0] — the seconds / normalized exit time / threshold / default value
//   bArg      — the boolean flag (has-exit-time, interruptible)

void Zenith_EditorAutomation::AddStep_AnimSmOpen(const char* szAssetPath)      { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_OPEN, szAssetPath); }
void Zenith_EditorAutomation::AddStep_AnimSmOpenFresh(const char* szAssetPath) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_OPEN_FRESH, szAssetPath); }
void Zenith_EditorAutomation::AddStep_AnimSmClose()                            { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_CLOSE); }
void Zenith_EditorAutomation::AddStep_AnimSmUndo()                             { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_UNDO); }
void Zenith_EditorAutomation::AddStep_AnimSmRedo()                             { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_REDO); }
void Zenith_EditorAutomation::AddStep_AnimSmSave()                             { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_SAVE); }
void Zenith_EditorAutomation::AddStep_AnimSmApply()                            { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_APPLY); }

void Zenith_EditorAutomation::AddStep_AnimSmAddClipPath(const char* szClipAssetPath) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_ADD_CLIP_PATH, szClipAssetPath); }
void Zenith_EditorAutomation::AddStep_AnimSmAddState(const char* szStateName)        { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_ADD_STATE, szStateName); }
void Zenith_EditorAutomation::AddStep_AnimSmRemoveState(const char* szStateName)     { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_REMOVE_STATE, szStateName); }
void Zenith_EditorAutomation::AddStep_AnimSmSetDefaultState(const char* szStateName) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_SET_DEFAULT_STATE, szStateName); }
void Zenith_EditorAutomation::AddStep_AnimSmRemoveParameter(const char* szName)      { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_REMOVE_PARAMETER, szName); }
void Zenith_EditorAutomation::AddStep_AnimSmExpectDefaultState(const char* szStateName) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_EXPECT_DEFAULT_STATE, szStateName); }

void Zenith_EditorAutomation::AddStep_AnimSmRenameState(const char* szOldName, const char* szNewName)   { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_RENAME_STATE, szOldName, szNewName); }
void Zenith_EditorAutomation::AddStep_AnimSmSetStateClip(const char* szStateName, const char* szClipName) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_SET_STATE_CLIP, szStateName, szClipName); }
void Zenith_EditorAutomation::AddStep_AnimSmAddTransition(const char* szFromState, const char* szToState) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_SM_ADD_TRANSITION, szFromState, szToState); }

void Zenith_EditorAutomation::AddStep_AnimSmSelectLayer(int iLayerId)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_SM_SELECT_LAYER;
	xAction.m_aiArgs[0] = iLayerId;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimSmRemoveTransition(const char* szFromState, int iIndex)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_SM_REMOVE_TRANSITION;
	xAction.m_szArg1 = SafeStr(szFromState);
	xAction.m_aiArgs[0] = iIndex;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimSmSetTransitionDuration(const char* szFromState, int iIndex, float fSeconds)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_SM_SET_TRANSITION_DURATION;
	xAction.m_szArg1 = SafeStr(szFromState);
	xAction.m_aiArgs[0] = iIndex;
	xAction.m_afArgs[0] = fSeconds;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimSmSetTransitionExitTime(const char* szFromState, int iIndex,
	bool bHasExitTime, float fNormalizedExitTime)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_SM_SET_TRANSITION_EXIT_TIME;
	xAction.m_szArg1 = SafeStr(szFromState);
	xAction.m_aiArgs[0] = iIndex;
	xAction.m_afArgs[0] = fNormalizedExitTime;
	xAction.m_bArg = bHasExitTime;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimSmSetTransitionInterruptible(const char* szFromState, int iIndex,
	bool bInterruptible)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_SM_SET_TRANSITION_INTERRUPTIBLE;
	xAction.m_szArg1 = SafeStr(szFromState);
	xAction.m_aiArgs[0] = iIndex;
	xAction.m_bArg = bInterruptible;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimSmAddCondition(const char* szFromState, int iIndex,
	const char* szParameterName, int iCompareOp, float fThreshold)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_SM_ADD_CONDITION;
	xAction.m_szArg1 = SafeStr(szFromState);
	xAction.m_szArg2 = SafeStr(szParameterName);
	xAction.m_aiArgs[0] = iIndex;
	xAction.m_aiArgs[1] = iCompareOp;
	xAction.m_afArgs[0] = fThreshold;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimSmRemoveCondition(const char* szFromState, int iIndex, int iConditionIndex)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_SM_REMOVE_CONDITION;
	xAction.m_szArg1 = SafeStr(szFromState);
	xAction.m_aiArgs[0] = iIndex;
	xAction.m_aiArgs[1] = iConditionIndex;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimSmAddParameter(const char* szName, int iType, float fDefault)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_SM_ADD_PARAMETER;
	xAction.m_szArg1 = SafeStr(szName);
	xAction.m_aiArgs[0] = iType;
	xAction.m_afArgs[0] = fDefault;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimSmExpectStateCount(int iExpectedCount)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_SM_EXPECT_STATE_COUNT;
	xAction.m_aiArgs[0] = iExpectedCount;
	m_axActions.PushBack(xAction);
}

// ---- BONE MASK steps (WU-7.1) ----
// The payload contract for the ANIM_MASK block, in one place so the executor
// reads it from one place too:
//   szArg1    — the asset path, or the BONE NAME, depending on the verb
//   afArgs[0] — the weight, or the expected weight
//   afArgs[1] — the tolerance (the EXPECT verb only)
//   bArg      — the has-avatar-mask flag

void Zenith_EditorAutomation::AddStep_AnimMaskOpen(const char* szAssetPath)      { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_MASK_OPEN, szAssetPath); }
void Zenith_EditorAutomation::AddStep_AnimMaskOpenFresh(const char* szAssetPath) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_MASK_OPEN_FRESH, szAssetPath); }
void Zenith_EditorAutomation::AddStep_AnimMaskClose()                            { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_MASK_CLOSE); }
void Zenith_EditorAutomation::AddStep_AnimMaskUndo()                             { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_MASK_UNDO); }
void Zenith_EditorAutomation::AddStep_AnimMaskRedo()                             { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_MASK_REDO); }
void Zenith_EditorAutomation::AddStep_AnimMaskSave()                             { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_MASK_SAVE); }

void Zenith_EditorAutomation::AddStep_AnimMaskSetWeight(const char* szBoneName, float fWeight)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_MASK_SET_WEIGHT;
	xAction.m_szArg1 = SafeStr(szBoneName);
	xAction.m_afArgs[0] = fWeight;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimMaskSetSubtree(const char* szBoneName, float fWeight)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_MASK_SET_SUBTREE;
	xAction.m_szArg1 = SafeStr(szBoneName);
	xAction.m_afArgs[0] = fWeight;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimMaskSetHasAvatar(bool bHasAvatarMask)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_MASK_SET_HAS_AVATAR;
	xAction.m_bArg = bHasAvatarMask;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimMaskExpectWeight(const char* szBoneName, float fExpectedWeight,
	float fTolerance)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_MASK_EXPECT_WEIGHT;
	xAction.m_szArg1 = SafeStr(szBoneName);
	xAction.m_afArgs[0] = fExpectedWeight;
	xAction.m_afArgs[1] = fTolerance;
	m_axActions.PushBack(xAction);
}

// ---- ANIMATOR LAYER steps (WU-7.2) ----
// The payload contract for the ANIM_LAYER block, in one place so the executor
// reads it from one place too:
//   szArg1    — the layer NAME (ADD / RENAME), the mask asset PATH
//               (SET_MASK_PATH) or the expected name (EXPECT_ORDER)
//   aiArgs[0] — the stable LAYER ID on every verb except ADD; the INDEX on
//               EXPECT_ORDER, which addresses a position rather than a layer
//   aiArgs[1] — the destination INDEX (MOVE) or the blend mode (SET_BLEND_MODE)
//   afArgs[0] — the weight
//   bArg      — the emit-events flag

void Zenith_EditorAutomation::AddStep_AnimLayerAdd(const char* szLayerName) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ANIM_LAYER_ADD, szLayerName); }

void Zenith_EditorAutomation::AddStep_AnimLayerRemove(int iLayerId)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_LAYER_REMOVE;
	xAction.m_aiArgs[0] = iLayerId;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimLayerRename(int iLayerId, const char* szNewName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_LAYER_RENAME;
	xAction.m_szArg1 = SafeStr(szNewName);
	xAction.m_aiArgs[0] = iLayerId;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimLayerSetWeight(int iLayerId, float fWeight)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_LAYER_SET_WEIGHT;
	xAction.m_aiArgs[0] = iLayerId;
	xAction.m_afArgs[0] = fWeight;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimLayerSetBlendMode(int iLayerId, int iBlendMode)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_LAYER_SET_BLEND_MODE;
	xAction.m_aiArgs[0] = iLayerId;
	xAction.m_aiArgs[1] = iBlendMode;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimLayerSetEmitEvents(int iLayerId, bool bEmitEvents)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_LAYER_SET_EMIT_EVENTS;
	xAction.m_aiArgs[0] = iLayerId;
	xAction.m_bArg = bEmitEvents;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimLayerSetMaskPath(int iLayerId, const char* szMaskAssetPath)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_LAYER_SET_MASK_PATH;
	xAction.m_szArg1 = SafeStr(szMaskAssetPath);
	xAction.m_aiArgs[0] = iLayerId;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimLayerMove(int iLayerId, int iNewIndex)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_LAYER_MOVE;
	xAction.m_aiArgs[0] = iLayerId;
	xAction.m_aiArgs[1] = iNewIndex;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimLayerSelect(int iLayerId)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_LAYER_SELECT;
	xAction.m_aiArgs[0] = iLayerId;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimLayerExpectOrder(int iIndex, const char* szExpectedName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_LAYER_EXPECT_ORDER;
	xAction.m_szArg1 = SafeStr(szExpectedName);
	xAction.m_aiArgs[0] = iIndex;
	m_axActions.PushBack(xAction);
}

//------------------------------------------------------------------------------
// BLEND-TREE authoring (WU-7.3), the ANIM_BLEND_* block. The packing contract,
// stated ONCE here and read back by ExecuteAnimBlendAction:
//
//   szArg1    — the STATE name (every verb except SELECT_POINT)
//   szArg2    — the CLIP name (ADD_POINT / SET_POINT_CLIP) or the PARAMETER name
//               (SET_PARAMETER)
//   aiArgs[0] — the point INDEX, or the tree KIND, or the AXIS, or the expected
//               COUNT — one slot, because no verb needs two of them
//   afArgs[0] — the position x
//   afArgs[1] — the position y
//   afArgs[2] — the tolerance (EXPECT_POINT_POSITION only)
//------------------------------------------------------------------------------

void Zenith_EditorAutomation::AddStep_AnimBlendSetTreeKind(const char* szStateName, int iKind)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_BLEND_SET_TREE_KIND;
	xAction.m_szArg1 = SafeStr(szStateName);
	xAction.m_aiArgs[0] = iKind;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimBlendSetParameter(const char* szStateName, int iAxis,
	const char* szParameterName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_BLEND_SET_PARAMETER;
	xAction.m_szArg1 = SafeStr(szStateName);
	xAction.m_szArg2 = SafeStr(szParameterName);
	xAction.m_aiArgs[0] = iAxis;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimBlendAddPoint(const char* szStateName, const char* szClipName,
	float fX, float fY)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_BLEND_ADD_POINT;
	xAction.m_szArg1 = SafeStr(szStateName);
	xAction.m_szArg2 = SafeStr(szClipName);
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimBlendRemovePoint(const char* szStateName, int iIndex)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_BLEND_REMOVE_POINT;
	xAction.m_szArg1 = SafeStr(szStateName);
	xAction.m_aiArgs[0] = iIndex;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimBlendSetPointClip(const char* szStateName, int iIndex,
	const char* szClipName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_BLEND_SET_POINT_CLIP;
	xAction.m_szArg1 = SafeStr(szStateName);
	xAction.m_szArg2 = SafeStr(szClipName);
	xAction.m_aiArgs[0] = iIndex;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimBlendSetPointPosition(const char* szStateName, int iIndex,
	float fX, float fY)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_BLEND_SET_POINT_POSITION;
	xAction.m_szArg1 = SafeStr(szStateName);
	xAction.m_aiArgs[0] = iIndex;
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimBlendSelectPoint(int iIndex)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_BLEND_SELECT_POINT;
	xAction.m_aiArgs[0] = iIndex;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimBlendExpectPointCount(const char* szStateName, int iExpectedCount)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_BLEND_EXPECT_POINT_COUNT;
	xAction.m_szArg1 = SafeStr(szStateName);
	xAction.m_aiArgs[0] = iExpectedCount;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimBlendExpectPointPosition(const char* szStateName, int iIndex,
	float fX, float fY, float fTolerance)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_BLEND_EXPECT_POINT_POSITION;
	xAction.m_szArg1 = SafeStr(szStateName);
	xAction.m_aiArgs[0] = iIndex;
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	xAction.m_afArgs[2] = fTolerance;
	m_axActions.PushBack(xAction);
}

//------------------------------------------------------------------------------
// CURVE-EDITOR authoring (WU-8.2), the ANIM_CURVE_* block. The packing contract,
// stated ONCE here and read back by ExecuteAnimCurveAction:
//
//   szArg1    — the BONE name. EMPTY would mean root motion, which every verb in
//               this family refuses: root motion has no tangent array (D17)
//   aiArgs[0] — the Flux_AnimTrack
//   aiArgs[1] — the KEY INDEX, resolved to a stable id at execution time
//   aiArgs[2] — the COMPONENT (0 x, 1 y, 2 z), DRAG only
//   bArg      — the toggle's value, or bIn on the drag / expect verbs
//   afArgs[0..2] — the IN tangent, the expected tangent, or the drop pixel (x, y)
//   afArgs[3..5] — the OUT tangent
//   afArgs[6] — the tolerance (EXPECT_KEY_TANGENT only)
//------------------------------------------------------------------------------

void Zenith_EditorAutomation::AddStep_AnimCurveSetView(bool bShow)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_CURVE_SET_VIEW;
	xAction.m_bArg = bShow;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimCurveSetUnified(bool bUnified)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_CURVE_SET_UNIFIED;
	xAction.m_bArg = bUnified;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimCurveSetKeyTangents(const char* szBone, int iTrack, int iKeyIndex,
	float fInX, float fInY, float fInZ, float fOutX, float fOutY, float fOutZ)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_CURVE_SET_KEY_TANGENTS;
	xAction.m_szArg1 = SafeStr(szBone);
	xAction.m_aiArgs[0] = iTrack;
	xAction.m_aiArgs[1] = iKeyIndex;
	xAction.m_afArgs[0] = fInX;
	xAction.m_afArgs[1] = fInY;
	xAction.m_afArgs[2] = fInZ;
	xAction.m_afArgs[3] = fOutX;
	xAction.m_afArgs[4] = fOutY;
	xAction.m_afArgs[5] = fOutZ;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimCurveSetSelectionAuto()
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_CURVE_SET_SELECTION_AUTO;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimCurveSetSelectionLinear()
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_CURVE_SET_SELECTION_LINEAR;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimCurveDragHandleToPixel(const char* szBone, int iTrack, int iKeyIndex,
	int iComponent, bool bIn, float fX, float fY)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_CURVE_DRAG_HANDLE_TO_PIXEL;
	xAction.m_szArg1 = SafeStr(szBone);
	xAction.m_aiArgs[0] = iTrack;
	xAction.m_aiArgs[1] = iKeyIndex;
	xAction.m_aiArgs[2] = iComponent;
	xAction.m_bArg = bIn;
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimCurveFitToSelection()
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_CURVE_FIT_TO_SELECTION;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimCurveExpectKeyTangent(const char* szBone, int iTrack, int iKeyIndex,
	bool bIn, float fExpectedX, float fExpectedY, float fExpectedZ, float fTolerance)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_CURVE_EXPECT_KEY_TANGENT;
	xAction.m_szArg1 = SafeStr(szBone);
	xAction.m_aiArgs[0] = iTrack;
	xAction.m_aiArgs[1] = iKeyIndex;
	xAction.m_bArg = bIn;
	xAction.m_afArgs[0] = fExpectedX;
	xAction.m_afArgs[1] = fExpectedY;
	xAction.m_afArgs[2] = fExpectedZ;
	xAction.m_afArgs[6] = fTolerance;
	m_axActions.PushBack(xAction);
}

//------------------------------------------------------------------------------
// TANGENT-MODE authoring (B3), the ANIM_TANGENT_* block. The packing contract,
// stated ONCE here and read back by ExecuteAnimTangentAction:
//
//   szArg1    — the BONE name, on the per-KEY verbs. EMPTY would mean root
//               motion, which has no tangent array at all (D17)
//   aiArgs[0] — the Flux_AnimTrack
//   aiArgs[1] — the KEY INDEX, resolved to a stable id at execution time
//   aiArgs[2] — the END (Zenith_AnimTangentEnd: 0 In, 1 Out, 2 Both)
//   aiArgs[3] — the MODE (Flux_TangentMode: 0 Linear, 1 Flat, 2 Auto, 3 Custom)
//
// ★ [2] AND [3] RATHER THAN m_bArg AND A FLOAT. m_bArg carries bIn on the curve
// family, which is a TWO-valued end — and an end is three-valued here, because
// "both, as one undo step" is the gesture a user actually performs. aiArgs[3] was
// free on every step in the file; the struct did not have to grow.
//------------------------------------------------------------------------------

void Zenith_EditorAutomation::AddStep_AnimTangentSetKeyMode(const char* szBone, int iTrack, int iKeyIndex,
	int iEnd, int iMode)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_TANGENT_SET_KEY_MODE;
	xAction.m_szArg1 = SafeStr(szBone);
	xAction.m_aiArgs[0] = iTrack;
	xAction.m_aiArgs[1] = iKeyIndex;
	xAction.m_aiArgs[2] = iEnd;
	xAction.m_aiArgs[3] = iMode;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimTangentSetSelectionMode(int iEnd, int iMode)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_TANGENT_SET_SELECTION_MODE;
	xAction.m_aiArgs[2] = iEnd;
	xAction.m_aiArgs[3] = iMode;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimTangentExpectKeyMode(const char* szBone, int iTrack, int iKeyIndex,
	int iEnd, int iExpectedMode)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_TANGENT_EXPECT_KEY_MODE;
	xAction.m_szArg1 = SafeStr(szBone);
	xAction.m_aiArgs[0] = iTrack;
	xAction.m_aiArgs[1] = iKeyIndex;
	xAction.m_aiArgs[2] = iEnd;
	xAction.m_aiArgs[3] = iExpectedMode;
	m_axActions.PushBack(xAction);
}

//------------------------------------------------------------------------------
// IK POSING (E1), the one-verb ANIM_IK_* block. The packing contract, stated
// ONCE here and read back by ExecuteAnimIkAction:
//
//   afArgs[0..2] — the MODEL-SPACE target, VERBATIM. Nothing on this path
//                  performs arithmetic on it: what the recipe types is what
//                  Zenith_AnimationPoseIK is handed, which is the strongest form
//                  of the FP-determinism rule (see the header).
//
// Nothing else is packed. The chain is derived from the SELECTED bone, so a
// recipe addresses it with AnimSelectBone rather than by naming bones here —
// one address for one thing, the way the pose block already works.
//------------------------------------------------------------------------------

void Zenith_EditorAutomation::AddStep_AnimBakeIK(float fTargetModelX, float fTargetModelY, float fTargetModelZ)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_IK_BAKE_TO_TARGET;
	xAction.m_afArgs[0] = fTargetModelX;
	xAction.m_afArgs[1] = fTargetModelY;
	xAction.m_afArgs[2] = fTargetModelZ;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimSetAutoKey(bool bEnabled)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_POSE_SET_AUTO_KEY;
	xAction.m_bArg = bEnabled;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AnimExpectBoneLocalRotation(int iBoneIndex, float fX, float fY, float fZ,
	float fW, float fTolerance)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION;
	xAction.m_aiArgs[0] = iBoneIndex;
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	xAction.m_afArgs[2] = fZ;
	xAction.m_afArgs[3] = fW;
	xAction.m_afArgs[4] = fTolerance;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GraphSelectNode(const char* szTypeName, int iOccurrence)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRAPH_SELECT_NODE;
	xAction.m_szArg1 = SafeStr(szTypeName);
	xAction.m_aiArgs[0] = iOccurrence;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GraphSetNodeParamFloat(const char* szPropertyName, float fValue)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRAPH_SET_NODE_PARAM_FLOAT;
	xAction.m_szArg1 = SafeStr(szPropertyName);
	xAction.m_afArgs[0] = fValue;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GraphSetNodeParamString(const char* szPropertyName, const char* szValue)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRAPH_SET_NODE_PARAM_STRING;
	xAction.m_szArg1 = SafeStr(szPropertyName);
	xAction.m_szArg2 = SafeStr(szValue);
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GraphSetNodeParamInt(const char* szPropertyName, int iValue)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRAPH_SET_NODE_PARAM_INT;
	xAction.m_szArg1 = SafeStr(szPropertyName);
	xAction.m_aiArgs[0] = iValue;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GraphSetNodeParamVec3(const char* szPropertyName, float fX, float fY, float fZ)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRAPH_SET_NODE_PARAM_VEC3;
	xAction.m_szArg1 = SafeStr(szPropertyName);
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	xAction.m_afArgs[2] = fZ;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GraphSetNodeParamBool(const char* szPropertyName, bool bValue)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRAPH_SET_NODE_PARAM_BOOL;
	xAction.m_szArg1 = SafeStr(szPropertyName);
	xAction.m_bArg = bValue;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GraphBuild(const char* szAssetPath, void (*pfnBuild)(Zenith_GraphBuilder&))
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRAPH_BUILD;
	xAction.m_szArg1 = SafeStr(szAssetPath);
	xAction.m_pfnGraphBuild = pfnBuild;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GraphConnect(const char* szSrcTypeName, int iSrcOccurrence, int iSrcPin, const char* szDstTypeName, int iDstOccurrence)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRAPH_CONNECT;
	xAction.m_szArg1 = SafeStr(szSrcTypeName);
	xAction.m_szArg2 = SafeStr(szDstTypeName);
	xAction.m_aiArgs[0] = iSrcOccurrence;
	xAction.m_aiArgs[1] = iDstOccurrence;
	xAction.m_afArgs[0] = static_cast<float>(iSrcPin);
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_GraphAddVariable(const char* szName, const char* szTypeName, float fDefaultNumeric)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = ActionType::GRAPH_ADD_VARIABLE;
	xAction.m_szArg1 = SafeStr(szName);
	xAction.m_szArg2 = SafeStr(szTypeName);
	xAction.m_afArgs[0] = fDefaultNumeric;
	m_axActions.PushBack(xAction);
}

// -- Particles --

void Zenith_EditorAutomation::AddStep_SetParticleConfig      (Flux_ParticleEmitterConfig* pxConfig) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_PARTICLE_CONFIG,         pxConfig); }
void Zenith_EditorAutomation::AddStep_SetParticleConfigByName(const char* szConfigName)              { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_PARTICLE_CONFIG_BY_NAME, szConfigName); }
void Zenith_EditorAutomation::AddStep_SetParticleEmitting    (bool bEmitting)                        { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_PARTICLE_EMITTING,       bEmitting); }

// -- Collider --

void Zenith_EditorAutomation::AddStep_AddColliderShape(int iVolumeType, int iBodyType) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ADD_COLLIDER_SHAPE, iVolumeType, iBodyType); }
void Zenith_EditorAutomation::AddStep_AddCapsuleCollider(float fRadius, float fHalfHeight, int iBodyType) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ADD_CAPSULE_COLLIDER, fRadius, fHalfHeight, iBodyType); }

// -- Model --

void Zenith_EditorAutomation::AddStep_AddMeshEntry(Flux_MeshGeometry* pxGeometry, Zenith_MaterialAsset* pxMaterial) { Push(Zenith_EditorAutomation::m_axActions, ActionType::ADD_MESH_ENTRY, pxGeometry, pxMaterial); }
void Zenith_EditorAutomation::AddStep_LoadModel(const char* szPath)                                                { Push(Zenith_EditorAutomation::m_axActions, ActionType::LOAD_MODEL, szPath); }
void Zenith_EditorAutomation::AddStep_SetModelMaterial(int iIndex, Zenith_MaterialAsset* pxMaterial)               { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_MODEL_MATERIAL, iIndex, pxMaterial); }

// -- Terrain --

void Zenith_EditorAutomation::AddStep_SetTerrainMaterial(int iSlot, Zenith_MaterialAsset* pxMaterial) { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_TERRAIN_MATERIAL, iSlot, pxMaterial); }
void Zenith_EditorAutomation::AddStep_SetTerrainSplatmapPath(const char* szPath)                      { Push(Zenith_EditorAutomation::m_axActions, ActionType::SET_TERRAIN_SPLATMAP_PATH, szPath); }

// -- Terrain-Editor Authoring --

void Zenith_EditorAutomation::AddStep_TerrainSetAssetSet(const char* szSet)
{
	Push(m_axActions, ActionType::TERRAIN_EDITOR_SET_ASSET_SET, szSet);
}

void Zenith_EditorAutomation::AddStep_TerrainSetDimensions(float fChunkSizeMetres,
	float fVertexSpacingMetres, int iGridChunksX, int iGridChunksZ)
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_SET_DIMENSIONS;
	xAction.m_afArgs[0] = fChunkSizeMetres;
	xAction.m_afArgs[1] = fVertexSpacingMetres;
	xAction.m_aiArgs[0] = iGridChunksX;
	xAction.m_aiArgs[1] = iGridChunksZ;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainResetSession()
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_RESET;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainGenerateProcedural(int iSeed, float fBaseHeight, float fAmplitude,
	float fFrequency, int iOctaves, float fLacunarity, float fGain, float fRidgedBlend)
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_GENERATE_PROCEDURAL;
	xAction.m_aiArgs[0] = iSeed;
	xAction.m_aiArgs[1] = iOctaves;
	xAction.m_afArgs[0] = fBaseHeight;
	xAction.m_afArgs[1] = fAmplitude;
	xAction.m_afArgs[2] = fFrequency;
	xAction.m_afArgs[3] = fLacunarity;
	xAction.m_afArgs[4] = fGain;
	xAction.m_afArgs[5] = fRidgedBlend;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainBrushStroke(int iTool, float fWorldX, float fWorldZ,
	float fRadius, float fStrength, float fToolValue)
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_BRUSH_STROKE;
	xAction.m_aiArgs[0] = iTool;
	xAction.m_afArgs[0] = fWorldX;
	xAction.m_afArgs[1] = fWorldZ;
	xAction.m_afArgs[2] = fRadius;
	xAction.m_afArgs[3] = fStrength;
	xAction.m_afArgs[4] = fToolValue;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainSampleStamp(float fWorldX, float fWorldZ, float fRadius)
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_SAMPLE_STAMP;
	xAction.m_afArgs[0] = fWorldX;
	xAction.m_afArgs[1] = fWorldZ;
	xAction.m_afArgs[2] = fRadius;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainAutoSplatRule(int iSlot, float fHeightMin, float fHeightMax,
	float fSlopeMinDeg, float fSlopeMaxDeg, float fWeight, float fJitter)
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_AUTO_SPLAT_RULE;
	xAction.m_aiArgs[0] = iSlot;
	xAction.m_afArgs[0] = fHeightMin;
	xAction.m_afArgs[1] = fHeightMax;
	xAction.m_afArgs[2] = fSlopeMinDeg;
	xAction.m_afArgs[3] = fSlopeMaxDeg;
	xAction.m_afArgs[4] = fWeight;
	xAction.m_afArgs[5] = fJitter;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainRunAutoSplat()
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_RUN_AUTO_SPLAT;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainErode(int iHydraulicDroplets, int iThermalIterations, int iSeed)
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_ERODE;
	xAction.m_aiArgs[0] = iHydraulicDroplets;
	xAction.m_aiArgs[1] = iThermalIterations;
	xAction.m_afArgs[0] = static_cast<float>(iSeed);
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainSetTreeBrush(int iTreesPerDab, float fScaleMin,
	float fScaleMax, float fSpacing, float fMaxSlopeDeg, int iSeed)
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_SET_TREE_BRUSH;
	xAction.m_aiArgs[0] = iTreesPerDab;
	xAction.m_aiArgs[1] = iSeed;
	xAction.m_afArgs[0] = fScaleMin;
	xAction.m_afArgs[1] = fScaleMax;
	xAction.m_afArgs[2] = fSpacing;
	xAction.m_afArgs[3] = fMaxSlopeDeg;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainSaveTextures()
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_SAVE_TEXTURES;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainExportChunks()
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_EXPORT_CHUNKS;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_TerrainExportChunksRect(
	int iMinX, int iMinY, int iMaxX, int iMaxY)
{
	Zenith_EditorAction xAction;
	xAction.m_eType = ActionType::TERRAIN_EDITOR_EXPORT_CHUNKS_RECT;
	xAction.m_aiArgs[0] = iMinX;
	xAction.m_aiArgs[1] = iMinY;
	xAction.m_aiArgs[2] = iMaxX;
	xAction.m_aiArgs[3] = iMaxY;
	m_axActions.PushBack(xAction);
}

// -- Prefab Variant Authoring --

void Zenith_EditorAutomation::AddStep_CreatePrefabFromSelected(const char* szPrefabName, const char* szSavePath)
{
	Push(m_axActions, ActionType::CREATE_PREFAB_FROM_SELECTED, szPrefabName, szSavePath);
}

void Zenith_EditorAutomation::AddStep_CreatePrefabVariant(
	const char* szVariantName,
	const char* szBasePath,
	const char* szSavePath)
{
	// CREATE_PREFAB_VARIANT needs THREE strings (name + base path + save path),
	// one more than the two-string Push helper covers — the third lives in m_szArg3.
	Zenith_EditorAction xAction = {};
	xAction.m_eType  = ActionType::CREATE_PREFAB_VARIANT;
	xAction.m_szArg1 = SafeStr(szVariantName);
	xAction.m_szArg2 = SafeStr(szBasePath);
	xAction.m_szArg3 = SafeStr(szSavePath);
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_AddPrefabVariantOverrideVec3(
	const char* szPrefabPath,
	const char* szComponentName,
	const char* szPropertyName,
	float fX, float fY, float fZ)
{
	// Uses m_szArg3 for the property name — same pattern as CREATE_PREFAB_VARIANT —
	// plus the float triple.
	Zenith_EditorAction xAction = {};
	xAction.m_eType     = ActionType::ADD_PREFAB_VARIANT_OVERRIDE_VEC3;
	xAction.m_szArg1    = SafeStr(szPrefabPath);
	xAction.m_szArg2    = SafeStr(szComponentName);
	xAction.m_szArg3    = SafeStr(szPropertyName);
	xAction.m_afArgs[0] = fX;
	xAction.m_afArgs[1] = fY;
	xAction.m_afArgs[2] = fZ;
	m_axActions.PushBack(xAction);
}

void Zenith_EditorAutomation::AddStep_InstantiatePrefab(const char* szPrefabPath, const char* szEntityName,
	float fPosX, float fPosY, float fPosZ,
	float fRotW, float fRotX, float fRotY, float fRotZ,
	float fScaleX, float fScaleY, float fScaleZ)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::INSTANTIATE_PREFAB;
	xAction.m_szArg1 = SafeStr(szPrefabPath);
	xAction.m_szArg2 = SafeStr(szEntityName);
	// pos[0..2], quat[3..6] (wxyz), scale[7..9] — see INSTANTIATE_PREFAB executor.
	xAction.m_afArgs[0] = fPosX;   xAction.m_afArgs[1] = fPosY;   xAction.m_afArgs[2] = fPosZ;
	xAction.m_afArgs[3] = fRotW;   xAction.m_afArgs[4] = fRotX;   xAction.m_afArgs[5] = fRotY;   xAction.m_afArgs[6] = fRotZ;
	xAction.m_afArgs[7] = fScaleX; xAction.m_afArgs[8] = fScaleY; xAction.m_afArgs[9] = fScaleZ;
	m_axActions.PushBack(xAction);
}

// -- Scene Loading --

void Zenith_EditorAutomation::AddStep_LoadInitialScene(void (*pfnCallback)())
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::LOAD_INITIAL_SCENE;
	xAction.m_pfnFunc = pfnCallback;
	m_axActions.PushBack(xAction);
}

// -- Custom --

void Zenith_EditorAutomation::AddStep_Custom(void (*pfnFunc)())
{
	AddStep_Custom(pfnFunc, nullptr);
}

void Zenith_EditorAutomation::AddStep_Custom(void (*pfnFunc)(), const char* szStepName)
{
	Zenith_EditorAction xAction = {};
	xAction.m_eType = Zenith_EditorActionType::CUSTOM_STEP;
	xAction.m_pfnFunc = pfnFunc;
	xAction.m_szStepName = SafeStr(szStepName);
	m_axActions.PushBack(xAction);
}

//=============================================================================
// Action Execution
//=============================================================================

// Terrain-editor authoring actions (TERRAIN_EDITOR_*). Split out of
// ExecuteAction: they share standalone-session bootstrapping and would push
// the main switch over the complexity gate. Relies on the TERRAIN_EDITOR_*
// enum values being contiguous (they are declared as one block).
enum class TerrainRectExecutionMode
{
	Production,
	PreflightOnly
};

static bool TryCreateTerrainExportRectFromAction(const Zenith_EditorAction& xAction,
	TerrainRectExecutionMode eRectMode, Flux_TerrainExportRect& xRectOut)
{
	const bool bValidRect = Flux_TerrainExportRect::TryCreate(
		static_cast<int32_t>(xAction.m_aiArgs[0]),
		static_cast<int32_t>(xAction.m_aiArgs[1]),
		static_cast<int32_t>(xAction.m_aiArgs[2]),
		static_cast<int32_t>(xAction.m_aiArgs[3]), xRectOut);
	if (!bValidRect && eRectMode == TerrainRectExecutionMode::Production)
	{
		Zenith_Assert(false,
			"TERRAIN_EDITOR_EXPORT_CHUNKS_RECT rejected bounds [%d,%d]-[%d,%d]",
			xAction.m_aiArgs[0], xAction.m_aiArgs[1],
			xAction.m_aiArgs[2], xAction.m_aiArgs[3]);
	}
	return bValidRect;
}

static bool ExecuteTerrainRectExport(const Zenith_EditorAction& xAction,
	Zenith_TerrainEditor& xTerrainEditor, const Flux_TerrainExportRect& xRect,
	TerrainRectExecutionMode eRectMode)
{
	if (eRectMode == TerrainRectExecutionMode::PreflightOnly)
	{
		return true;
	}

	const bool bExported = xTerrainEditor.BakeMeshesRect(xRect);
	Zenith_Assert(bExported,
		"TERRAIN_EDITOR_EXPORT_CHUNKS_RECT failed for bounds [%d,%d]-[%d,%d]",
		xAction.m_aiArgs[0], xAction.m_aiArgs[1],
		xAction.m_aiArgs[2], xAction.m_aiArgs[3]);
	return bExported;
}

// Vertex SPACING is what an authoring step spells; quads-per-chunk-edge is what
// the format stores. Snap down to the nearest power of two so the divisor-4 LOW
// and physics bakes stay integral -- an exact spacing (64/64, 64/128) is
// unchanged by the snap, and a spacing that does not divide cleanly resolves to
// the next coarser legal one rather than silently rounding the chunk size.
static u_int ResolveQuadsPerChunkEdgeFromSpacing(float fChunkWorldSize, float fVertexSpacing)
{
	if (!(fChunkWorldSize > 0.0f) || !(fVertexSpacing > 0.0f))
	{
		return 0u;
	}
	const float fRawQuads = fChunkWorldSize / fVertexSpacing;
	u_int uQuads = Zenith_TerrainDimensionsLimits::uMIN_QUADS_PER_CHUNK_EDGE;
	while (uQuads < Zenith_TerrainDimensionsLimits::uMAX_QUADS_PER_CHUNK_EDGE &&
		static_cast<float>(uQuads) * 2.0f <= fRawQuads)
	{
		uQuads *= 2u;
	}
	return uQuads;
}

static bool ExecuteTerrainEditorAction(const Zenith_EditorAction& xAction,
	Zenith_TerrainEditor& xTerrainEditor, TerrainRectExecutionMode eRectMode)
{
	Flux_TerrainExportRect xExportRect;
	if (xAction.m_eType == Zenith_EditorActionType::TERRAIN_EDITOR_SET_ASSET_SET)
	{
		// Validate and preflight every selected-component constraint BEFORE
		// changing the editor's staged target. A refused retarget is transactional:
		// neither the editor nor the live component observes the candidate.
		std::string strResolvedCandidateDirectory;
		const bool bValidCandidate = Zenith_TerrainComponent::TryResolveTerrainAssetDirectory(
			xAction.m_szArg1, strResolvedCandidateDirectory);
		if (!bValidCandidate)
		{
			// Route through the staging API only to expose its validation/status
			// error; invalid input is guaranteed to preserve the current stage.
			xTerrainEditor.SetAssetSet(xAction.m_szArg1);
			Zenith_Assert(false, "TERRAIN_EDITOR_SET_ASSET_SET rejected invalid set '%s'",
				xAction.m_szArg1.c_str());
			return false;
		}

		Zenith_Entity* pxSelected = g_xEngine.Editor().GetSelectedEntity();
		Zenith_TerrainComponent* pxSelectedTerrain = pxSelected
			? pxSelected->TryGetComponent<Zenith_TerrainComponent>()
			: nullptr;
		if (pxSelectedTerrain != nullptr)
		{
			if (pxSelectedTerrain->IsTerrainInitializedForEditor() &&
				pxSelectedTerrain->GetTerrainAssetSet() != xAction.m_szArg1)
			{
				Zenith_Assert(false,
					"TERRAIN_EDITOR_SET_ASSET_SET cannot retarget an initialized terrain; use TerrainEditor::BakeFull");
				return false;
			}
		}

		const bool bStaged = xTerrainEditor.SetAssetSet(xAction.m_szArg1);
		Zenith_Assert(bStaged, "Validated terrain asset set unexpectedly failed to stage");
		if (!bStaged)
		{
			return false;
		}

		if (pxSelectedTerrain != nullptr && !pxSelectedTerrain->IsTerrainInitializedForEditor())
		{
			// A fresh component has no live buffers to invalidate. Stamp the
			// validated set so a following SaveScene persists the authoring target.
			const bool bStamped = pxSelectedTerrain->SetTerrainAssetSet(xAction.m_szArg1);
			Zenith_Assert(bStamped,
				"TERRAIN_EDITOR_SET_ASSET_SET failed to stamp validated set on fresh terrain");
			if (!bStamped)
			{
				return false;
			}
		}
	}
	if (xAction.m_eType == Zenith_EditorActionType::TERRAIN_EDITOR_SET_DIMENSIONS)
	{
		// Same transactional shape as SET_ASSET_SET above: validate the payload
		// and every selected-component constraint BEFORE anything is staged, so a
		// refused spec leaves neither the editor nor the live component changed.
		Zenith_TerrainDimensions xCandidate;
		xCandidate.m_fChunkWorldSize = xAction.m_afArgs[0];
		xCandidate.m_uQuadsPerChunkEdge = ResolveQuadsPerChunkEdgeFromSpacing(
			xAction.m_afArgs[0], xAction.m_afArgs[1]);
		xCandidate.m_uGridChunksX = static_cast<u_int>(xAction.m_aiArgs[0]);
		xCandidate.m_uGridChunksZ = static_cast<u_int>(xAction.m_aiArgs[1]);
		if (!xCandidate.IsValid())
		{
			Zenith_Assert(false,
				"TERRAIN_EDITOR_SET_DIMENSIONS rejected chunk=%.3fm spacing=%.3fm grid=%dx%d "
				"(spacing must divide the chunk size into a power-of-two quad count in [4, 256], "
				"and each grid axis must be in [1, 64])",
				xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_aiArgs[0], xAction.m_aiArgs[1]);
			return false;
		}

		Zenith_Entity* pxSelected = g_xEngine.Editor().GetSelectedEntity();
		Zenith_TerrainComponent* pxSelectedTerrain = pxSelected
			? pxSelected->TryGetComponent<Zenith_TerrainComponent>()
			: nullptr;
		if (pxSelectedTerrain != nullptr &&
			pxSelectedTerrain->IsTerrainInitializedForEditor() &&
			!(pxSelectedTerrain->GetTerrainDimensions() == xCandidate))
		{
			Zenith_Assert(false,
				"TERRAIN_EDITOR_SET_DIMENSIONS cannot re-shape an initialized terrain; use TerrainEditor::BakeFull");
			return false;
		}

		if (!xTerrainEditor.IsActive())
		{
			xTerrainEditor.OpenStandalone();
		}
		const bool bStaged = xTerrainEditor.SetDimensions(xCandidate);
		Zenith_Assert(bStaged, "Validated terrain dimensions unexpectedly failed to stage");
		if (!bStaged)
		{
			return false;
		}

		if (pxSelectedTerrain != nullptr && !pxSelectedTerrain->IsTerrainInitializedForEditor())
		{
			// A fresh component has no live buffers to invalidate. Stamp the
			// validated spec so a following SaveScene persists it (v5 tail).
			const bool bStamped = pxSelectedTerrain->SetTerrainDimensions(xCandidate);
			Zenith_Assert(bStamped,
				"TERRAIN_EDITOR_SET_DIMENSIONS failed to stamp validated dimensions on fresh terrain");
			if (!bStamped)
			{
				return false;
			}
		}
	}
	// Validate the exact signed payload before OpenStandalone can allocate or
	// load any editor state. Rejected bounds therefore have no side effects.
	if (xAction.m_eType == Zenith_EditorActionType::TERRAIN_EDITOR_EXPORT_CHUNKS_RECT &&
		!TryCreateTerrainExportRectFromAction(xAction, eRectMode, xExportRect))
	{
		return false;
	}
	if (!xTerrainEditor.IsActive())
	{
		xTerrainEditor.OpenStandalone();
	}

	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::TERRAIN_EDITOR_SET_ASSET_SET:
	case Zenith_EditorActionType::TERRAIN_EDITOR_SET_DIMENSIONS:
		// Both are fully handled above, transactionally, before the session was
		// opened. Nothing left to do once it is.
		break;

	case Zenith_EditorActionType::TERRAIN_EDITOR_RESET:
		xTerrainEditor.ResetImagesToDefaults();
		break;

	case Zenith_EditorActionType::TERRAIN_EDITOR_GENERATE_PROCEDURAL:
	{
		Zenith_TerrainProceduralParams xParams;
		xParams.m_uSeed = static_cast<u_int>(xAction.m_aiArgs[0]);
		xParams.m_uOctaves = static_cast<u_int>(xAction.m_aiArgs[1]);
		xParams.m_fBaseHeight = xAction.m_afArgs[0];
		xParams.m_fAmplitude = xAction.m_afArgs[1];
		xParams.m_fFrequency = xAction.m_afArgs[2];
		xParams.m_fLacunarity = xAction.m_afArgs[3];
		xParams.m_fGain = xAction.m_afArgs[4];
		xParams.m_fRidgedBlend = xAction.m_afArgs[5];
		xTerrainEditor.GenerateProcedural(xParams);
		break;
	}

	case Zenith_EditorActionType::TERRAIN_EDITOR_BRUSH_STROKE:
		// Direct dab (no stroke bracketing): automation needs no undo capture.
		xTerrainEditor.ApplyBrushDab(static_cast<Zenith_TerrainBrushTool>(xAction.m_aiArgs[0]),
			xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3], xAction.m_afArgs[4]);
		break;

	case Zenith_EditorActionType::TERRAIN_EDITOR_SAMPLE_STAMP:
		xTerrainEditor.SampleStamp(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]);
		break;

	case Zenith_EditorActionType::TERRAIN_EDITOR_AUTO_SPLAT_RULE:
	{
		Zenith_TerrainAutoSplatRule xRule;
		xRule.m_bEnabled = true;
		xRule.m_fHeightMin = xAction.m_afArgs[0];
		xRule.m_fHeightMax = xAction.m_afArgs[1];
		xRule.m_fSlopeMinDeg = xAction.m_afArgs[2];
		xRule.m_fSlopeMaxDeg = xAction.m_afArgs[3];
		xRule.m_fWeight = xAction.m_afArgs[4];
		xRule.m_fNoiseJitter = xAction.m_afArgs[5];
		xTerrainEditor.SetAutoSplatRule(static_cast<u_int>(xAction.m_aiArgs[0]), xRule);
		break;
	}

	case Zenith_EditorActionType::TERRAIN_EDITOR_RUN_AUTO_SPLAT:
		xTerrainEditor.RunAutoSplat();
		break;

	case Zenith_EditorActionType::TERRAIN_EDITOR_ERODE:
	{
		Zenith_TerrainErosionParams xParams;
		xParams.m_uHydraulicDroplets = static_cast<u_int>(xAction.m_aiArgs[0]);
		xParams.m_uThermalIterations = static_cast<u_int>(xAction.m_aiArgs[1]);
		xParams.m_uSeed = static_cast<u_int>(xAction.m_afArgs[0]);
		xTerrainEditor.RunErosion(xParams, true /* synchronous */);
		break;
	}

	case Zenith_EditorActionType::TERRAIN_EDITOR_SET_TREE_BRUSH:
		xTerrainEditor.SetTreeBrushSettings(
			static_cast<u_int>(xAction.m_aiArgs[0]),
			xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2],
			xAction.m_afArgs[3], static_cast<u_int>(xAction.m_aiArgs[1]));
		break;

	case Zenith_EditorActionType::TERRAIN_EDITOR_SAVE_TEXTURES:
		xTerrainEditor.SaveTextures();
		break;

	case Zenith_EditorActionType::TERRAIN_EDITOR_EXPORT_CHUNKS:
		xTerrainEditor.BakeMeshes();
		break;

	case Zenith_EditorActionType::TERRAIN_EDITOR_EXPORT_CHUNKS_RECT:
		return ExecuteTerrainRectExport(xAction, xTerrainEditor, xExportRect, eRectMode);

	default:
		Zenith_Assert(false, "Non-terrain action routed to ExecuteTerrainEditorAction");
		break;
	}
	return true;
}

static bool TryRouteTerrainEditorAction(const Zenith_EditorAction& xAction,
	Zenith_TerrainEditor& xTerrainEditor, TerrainRectExecutionMode eRectMode,
	bool& bSucceededOut)
{
	if (xAction.m_eType < Zenith_EditorActionType::TERRAIN_EDITOR_SET_ASSET_SET ||
		xAction.m_eType > Zenith_EditorActionType::TERRAIN_EDITOR_SET_DIMENSIONS)
	{
		return false;
	}

	bSucceededOut = ExecuteTerrainEditorAction(xAction, xTerrainEditor, eRectMode);
	return true;
}

namespace
{
	// Graph authoring steps assert on failure so a bad boot-authoring sequence
	// (typo'd node type, wrong occurrence, invalid pin) surfaces immediately.
	void GraphActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation graph step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}

	// Material authoring steps assert on failure for the same reason (a typo'd
	// param/slot name or a cyclic parent surfaces at boot, not as a silent no-op).
	void MaterialActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation material step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}

	// Grass-type steps, same contract: an unknown parameter name or an
	// out-of-range type index is an authoring typo, and a silent no-op would
	// ship grass that merely looks slightly wrong.
	void GrassTypeActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation grass-type step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}

	// Animation dope-sheet steps (WU-3.4), same contract again: a bone the clip
	// has no channel for, a key index past the end of a track, a move a D11
	// collision refused — every one of those is an authoring mistake, and the
	// panel reports all of them as a bare `false` that nothing downstream reads.
	void AnimActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation animation step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}

	// Shared boilerplate for the many field-edit actions that all start with
	// "get the selected entity, assert it exists". szActionName goes straight
	// into the assert message, matching what every case used to spell out by
	// hand (e.g. "No entity selected for SET_CAMERA_POSITION"). Callers that
	// also require a specific component still assert that separately.
	Zenith_Entity& GetSelectedEntityChecked(const char* szActionName)
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for %s", szActionName);
		return *pxEntity;
	}
}

// All material authoring actions (MATERIAL_CREATE .. MATERIAL_SAVE, kept
// CONTIGUOUS in the enum) live in their own executor, mirroring the graph /
// terrain-editor / UI splits — ExecuteAction routes the whole range here.
static void ExecuteMaterialAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::MATERIAL_CREATE:           MaterialActionChecked(Zenith_MaterialEditorPanel::Action_CreateMaterial(xAction.m_szArg1.c_str()), "MaterialCreate", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::MATERIAL_OPEN:             MaterialActionChecked(Zenith_MaterialEditorPanel::Action_OpenMaterial(xAction.m_szArg1.c_str()), "MaterialOpen", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::MATERIAL_SAVE:             MaterialActionChecked(Zenith_MaterialEditorPanel::Action_SaveMaterial(xAction.m_szArg1.c_str()), "MaterialSave", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::MATERIAL_SET_PARAM_FLOAT:  MaterialActionChecked(Zenith_MaterialEditorPanel::Action_SetParamFloat(xAction.m_szArg1.c_str(), xAction.m_afArgs[0]), "MaterialSetParamFloat", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::MATERIAL_SET_PARAM_COLOR:  MaterialActionChecked(Zenith_MaterialEditorPanel::Action_SetParamColor(xAction.m_szArg1.c_str(), xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]), "MaterialSetParamColor", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::MATERIAL_SET_PARAM_INT:    MaterialActionChecked(Zenith_MaterialEditorPanel::Action_SetParamInt(xAction.m_szArg1.c_str(), xAction.m_aiArgs[0]), "MaterialSetParamInt", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::MATERIAL_SET_TEXTURE:      MaterialActionChecked(Zenith_MaterialEditorPanel::Action_SetTexture(xAction.m_szArg1.c_str(), xAction.m_szArg2.c_str()), "MaterialSetTexture", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::MATERIAL_SET_PARENT:       MaterialActionChecked(Zenith_MaterialEditorPanel::Action_SetParent(xAction.m_szArg1.c_str()), "MaterialSetParent", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::MATERIAL_SET_OVERRIDE:     MaterialActionChecked(Zenith_MaterialEditorPanel::Action_SetOverride(xAction.m_szArg1.c_str(), xAction.m_bArg), "MaterialSetOverride", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::MATERIAL_SET_PREVIEW_MESH: MaterialActionChecked(Zenith_MaterialEditorPanel::Action_SetPreviewMesh(xAction.m_aiArgs[0]), "MaterialSetPreviewMesh", nullptr); break;
	case Zenith_EditorActionType::MATERIAL_SET_PREVIEW_LIGHT:MaterialActionChecked(Zenith_MaterialEditorPanel::Action_SetPreviewLight(xAction.m_afArgs[0], xAction.m_afArgs[1]), "MaterialSetPreviewLight", nullptr); break;
	default:
		Zenith_Assert(false, "Non-material action routed to ExecuteMaterialAction");
		break;
	}
}

namespace
{
	// Bounds-check BEFORE Flux_GrassTypeTable::Get, which asserts on an
	// out-of-range index and then clamps to entry 0 — a bad index would
	// otherwise silently rewrite the default type.
	bool GrassTypeIndexValid(int iType)
	{
		return iType >= 0 && iType < static_cast<int>(uFLUX_GRASS_MAX_TYPES);
	}

	bool GrassTypeSetCount(Zenith_TerrainEditor& xEditor, int iCount)
	{
		// SetCount clamps, so this range test is the ONLY thing that turns an
		// authoring typo (0, or 20 on a 16-slot table) into a failed step.
		if (iCount < 1 || iCount > static_cast<int>(uFLUX_GRASS_MAX_TYPES))
		{
			return false;
		}
		xEditor.GrassTypes().SetCount(static_cast<u_int>(iCount));
		return true;
	}

	bool GrassTypeSetName(Zenith_TerrainEditor& xEditor, int iType, const char* szName)
	{
		if (!GrassTypeIndexValid(iType) || szName == nullptr || szName[0] == '\0')
		{
			return false;
		}
		xEditor.GrassTypes().SetName(static_cast<u_int>(iType), szName);
		return true;
	}

	bool GrassTypeSetParamFloat(Zenith_TerrainEditor& xEditor, int iType, const char* szParam, float fValue)
	{
		return GrassTypeIndexValid(iType) &&
			xEditor.GrassTypes().Get(static_cast<u_int>(iType)).SetFloatParamByName(szParam, fValue);
	}

	bool GrassTypeSetParamColour(Zenith_TerrainEditor& xEditor, int iType, const char* szParam,
		const Zenith_Maths::Vector3& xColour)
	{
		return GrassTypeIndexValid(iType) &&
			xEditor.GrassTypes().Get(static_cast<u_int>(iType)).SetColourParamByName(szParam, xColour);
	}
}

// All grass-type authoring actions (GRASS_TYPES_CREATE .. GRASS_TYPES_SAVE,
// kept CONTIGUOUS in the enum) live in their own executor, mirroring the
// material / graph / terrain-editor / UI splits — ExecuteAction routes the
// whole range here.
//
// These deliberately do NOT auto-open a terrain-editor session the way the
// TERRAIN_EDITOR_* steps do: the working table is valid before any session
// (default-constructed == the built-in set), and opening one would load 80 MB
// of CPU images to author a 2 KB table.
static void ExecuteGrassTypeAction(const Zenith_EditorAction& xAction, Zenith_TerrainEditor& xTerrainEditor)
{
	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::GRASS_TYPES_CREATE:          xTerrainEditor.GrassTypes_Reset(); break;
	case Zenith_EditorActionType::GRASS_TYPES_SET_COUNT:       GrassTypeActionChecked(GrassTypeSetCount(xTerrainEditor, xAction.m_aiArgs[0]), "GrassTypesSetCount", nullptr); break;
	case Zenith_EditorActionType::GRASS_TYPES_SET_NAME:        GrassTypeActionChecked(GrassTypeSetName(xTerrainEditor, xAction.m_aiArgs[0], xAction.m_szArg1.c_str()), "GrassTypesSetName", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRASS_TYPES_SET_PARAM_FLOAT: GrassTypeActionChecked(GrassTypeSetParamFloat(xTerrainEditor, xAction.m_aiArgs[0], xAction.m_szArg1.c_str(), xAction.m_afArgs[0]), "GrassTypesSetParamFloat", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRASS_TYPES_SET_PARAM_COLOR: GrassTypeActionChecked(GrassTypeSetParamColour(xTerrainEditor, xAction.m_aiArgs[0], xAction.m_szArg1.c_str(), Zenith_Maths::Vector3(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2])), "GrassTypesSetParamColor", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRASS_TYPES_SAVE:            GrassTypeActionChecked(xTerrainEditor.GrassTypes_Save(), "GrassTypesSave", szZENITH_GRASS_TYPE_TABLE_ASSET_PATH); break;
	default:
		Zenith_Assert(false, "Non-grass-type action routed to ExecuteGrassTypeAction");
		break;
	}
}
// The selected entity's canvas, or null with an assert naming the step.
//
// ONE engine-singleton reach for every case that uses it, because this file's
// singleton count is a RATCHET: a block of nine new cases each spelling out the
// selected-entity lookup for itself raises the count by nine and fails the
// gate, and the answer to that is always to hoist, never to bump the budget.
static Zenith_UIComponent* GetSelectedUICanvasComponent(const char* szStepName)
{
	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	Zenith_Assert(pxEntity, "No entity selected for %s", szStepName);
	if (pxEntity == nullptr)
	{
		return nullptr;
	}
	Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(),
		"Selected entity has no UIComponent for %s", szStepName);
	if (!pxEntity->HasComponent<Zenith_UIComponent>())
	{
		return nullptr;
	}
	return &pxEntity->GetComponent<Zenith_UIComponent>();
}

// The two on-screen-control lookups, once each rather than once per setter:
// m_szArg1 is always the element name for these steps, so the whole body of a
// field-edit case is a resolve, a null check and the setter.
static Zenith_UI::Zenith_UIVirtualStick* FindSelectedVirtualStick(const Zenith_EditorAction& xAction,
	const char* szStepName)
{
	Zenith_UIComponent* pxUI = GetSelectedUICanvasComponent(szStepName);
	if (pxUI == nullptr)
	{
		return nullptr;
	}
	Zenith_UI::Zenith_UIVirtualStick* pxStick =
		pxUI->FindElement<Zenith_UI::Zenith_UIVirtualStick>(xAction.m_szArg1.c_str());
	Zenith_Assert(pxStick, "UI virtual stick not found: %s (%s)", xAction.m_szArg1.c_str(), szStepName);
	return pxStick;
}

static Zenith_UI::Zenith_UIVirtualButton* FindSelectedVirtualButton(const Zenith_EditorAction& xAction,
	const char* szStepName)
{
	Zenith_UIComponent* pxUI = GetSelectedUICanvasComponent(szStepName);
	if (pxUI == nullptr)
	{
		return nullptr;
	}
	Zenith_UI::Zenith_UIVirtualButton* pxButton =
		pxUI->FindElement<Zenith_UI::Zenith_UIVirtualButton>(xAction.m_szArg1.c_str());
	Zenith_Assert(pxButton, "UI virtual button not found: %s (%s)", xAction.m_szArg1.c_str(), szStepName);
	return pxButton;
}

// All UI authoring actions (CREATE_UI_TEXT .. SET_UI_VIRTUAL_BUTTON_HIT_SLOP,
// kept CONTIGUOUS in the enum) live in their own executor, mirroring the
// terrain-editor split above - ExecuteAction routes the whole range here.
static void ExecuteUIAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	//--------------------------------------------------------------------------
	// UI element creation and field edits
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_UI_TEXT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_TEXT");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateText(xAction.m_szArg1.c_str(), xAction.m_szArg2.c_str());
		break;
	}

	case Zenith_EditorActionType::CREATE_UI_BUTTON:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_BUTTON");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateButton(xAction.m_szArg1.c_str(), xAction.m_szArg2.c_str());
		break;
	}

	case Zenith_EditorActionType::CREATE_UI_RECT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_RECT");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateRect(xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::CREATE_UI_IMAGE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for CREATE_UI_IMAGE");
		Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Selected entity has no UIComponent");
		pxEntity->GetComponent<Zenith_UIComponent>().CreateImage(xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_UI_IMAGE_TEXTURE_PATH:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_IMAGE_TEXTURE_PATH");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIImage* pxImage = xUI.FindElement<Zenith_UI::Zenith_UIImage>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxImage, "UI image not found: %s", xAction.m_szArg1.c_str());
		pxImage->SetTexturePath(xAction.m_szArg2.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_UI_ANCHOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_ANCHOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1.c_str());
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1.c_str());
		pxElement->SetAnchorAndPivot(static_cast<Zenith_UI::AnchorPreset>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_POSITION:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_POSITION");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1.c_str());
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1.c_str());
		pxElement->SetPosition(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1.c_str());
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1.c_str());
		pxElement->SetSize(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_FONT_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_FONT_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxText, "UI text element not found: %s", xAction.m_szArg1.c_str());
		pxText->SetFontSize(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1.c_str());
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1.c_str());
		pxElement->SetColor(Zenith_Maths::Vector4(
			xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_ALIGNMENT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_ALIGNMENT");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxText, "UI text element not found: %s", xAction.m_szArg1.c_str());
		pxText->SetAlignment(static_cast<Zenith_UI::TextAlignment>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_VISIBLE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_VISIBLE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1.c_str());
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1.c_str());
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
		pxEntity->GetComponent<Zenith_UIComponent>().CreateLayoutGroup(xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::ADD_UI_CHILD:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for ADD_UI_CHILD");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxParent = xUI.FindElement(xAction.m_szArg1.c_str());
		Zenith_Assert(pxParent, "UI parent element not found: %s", xAction.m_szArg1.c_str());
		Zenith_UI::Zenith_UIElement* pxChild = xUI.FindElement(xAction.m_szArg2.c_str());
		Zenith_Assert(pxChild, "UI child element not found: %s", xAction.m_szArg2.c_str());
		xUI.GetCanvas().ReparentElement(pxChild, pxParent);
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_DIRECTION:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_DIRECTION");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1.c_str());
		pxLayout->SetDirection(static_cast<Zenith_UI::LayoutDirection>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_SPACING:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_SPACING");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1.c_str());
		pxLayout->SetSpacing(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_CHILD_ALIGNMENT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_CHILD_ALIGNMENT");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1.c_str());
		pxLayout->SetChildAlignment(static_cast<Zenith_UI::ChildAlignment>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_PADDING:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_PADDING");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1.c_str());
		pxLayout->SetPadding(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_FIT_TO_CONTENT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_FIT_TO_CONTENT");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1.c_str());
		pxLayout->SetFitToContent(xAction.m_bArg);
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_CHILD_FORCE_EXPAND:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_CHILD_FORCE_EXPAND");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1.c_str());
		pxLayout->SetChildForceExpandWidth(xAction.m_aiArgs[0] != 0);
		pxLayout->SetChildForceExpandHeight(xAction.m_aiArgs[1] != 0);
		break;
	}

	case Zenith_EditorActionType::SET_UI_LAYOUT_REVERSE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_LAYOUT_REVERSE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxLayout, "UI layout group not found: %s", xAction.m_szArg1.c_str());
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
		pxEntity->GetComponent<Zenith_UIComponent>().CreateToggle(xAction.m_szArg1.c_str(), xAction.m_szArg2.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_UI_TOGGLE_ON_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_TOGGLE_ON_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIToggle* pxToggle = xUI.FindElement<Zenith_UI::Zenith_UIToggle>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxToggle, "UI toggle not found: %s", xAction.m_szArg1.c_str());
		pxToggle->SetOnColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_TOGGLE_OFF_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_TOGGLE_OFF_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIToggle* pxToggle = xUI.FindElement<Zenith_UI::Zenith_UIToggle>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxToggle, "UI toggle not found: %s", xAction.m_szArg1.c_str());
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
		pxEntity->GetComponent<Zenith_UIComponent>().CreateOverlay(xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_UI_OVERLAY_DIM_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_OVERLAY_DIM_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIOverlay* pxOverlay = xUI.FindElement<Zenith_UI::Zenith_UIOverlay>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxOverlay, "UI overlay not found: %s", xAction.m_szArg1.c_str());
		pxOverlay->SetDimColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_OVERLAY_CONTENT_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_OVERLAY_CONTENT_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIOverlay* pxOverlay = xUI.FindElement<Zenith_UI::Zenith_UIOverlay>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxOverlay, "UI overlay not found: %s", xAction.m_szArg1.c_str());
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
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1.c_str());
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1.c_str());

		Zenith_UI::Zenith_UIElement* pxUp = !xAction.m_szArg2.empty() ? xUI.FindElement(xAction.m_szArg2.c_str()) : nullptr;
		Zenith_UI::Zenith_UIElement* pxDown = !xAction.m_szArg3.empty() ? xUI.FindElement(xAction.m_szArg3.c_str()) : nullptr;
		Zenith_UI::Zenith_UIElement* pxLeft = !xAction.m_szArg4.empty() ? xUI.FindElement(xAction.m_szArg4.c_str()) : nullptr;
		Zenith_UI::Zenith_UIElement* pxRight = !xAction.m_szArg5.empty() ? xUI.FindElement(xAction.m_szArg5.c_str()) : nullptr;

		pxElement->SetNavigation(pxUp, pxDown, pxLeft, pxRight);
		break;
	}

	//--------------------------------------------------------------------------
	// UI scroll view
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_UI_SCROLL_VIEW:
	{
		Zenith_UIComponent* pxUI = GetSelectedUICanvasComponent("CREATE_UI_SCROLL_VIEW");
		if (pxUI == nullptr) break;
		pxUI->CreateScrollView(xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_UI_SCROLL_VIEW_CONTENT_SIZE:
	{
		Zenith_UIComponent* pxUI = GetSelectedUICanvasComponent("SET_UI_SCROLL_VIEW_CONTENT_SIZE");
		if (pxUI == nullptr) break;
		Zenith_UI::Zenith_UIScrollView* pxScrollView = pxUI->FindElement<Zenith_UI::Zenith_UIScrollView>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxScrollView, "UI scroll view not found: %s", xAction.m_szArg1.c_str());
		if (pxScrollView == nullptr) break;
		pxScrollView->SetContentSize(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	//--------------------------------------------------------------------------
	// UI on-screen controls (B9)
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_UI_VIRTUAL_STICK:
	{
		Zenith_UIComponent* pxUI = GetSelectedUICanvasComponent("CREATE_UI_VIRTUAL_STICK");
		if (pxUI == nullptr) break;
		pxUI->CreateVirtualStick(xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_UI_VIRTUAL_STICK_ACTION:
	{
		Zenith_UI::Zenith_UIVirtualStick* pxStick = FindSelectedVirtualStick(xAction, "SET_UI_VIRTUAL_STICK_ACTION");
		if (pxStick == nullptr) break;
		pxStick->SetAction(xAction.m_szArg2.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_UI_VIRTUAL_STICK_MODE:
	{
		Zenith_UI::Zenith_UIVirtualStick* pxStick = FindSelectedVirtualStick(xAction, "SET_UI_VIRTUAL_STICK_MODE");
		if (pxStick == nullptr) break;
		pxStick->SetMode(static_cast<Zenith_UI::Zenith_UIVirtualStick::StickMode>(xAction.m_aiArgs[0]));
		break;
	}

	case Zenith_EditorActionType::SET_UI_VIRTUAL_STICK_RADIUS:
	{
		Zenith_UI::Zenith_UIVirtualStick* pxStick = FindSelectedVirtualStick(xAction, "SET_UI_VIRTUAL_STICK_RADIUS");
		if (pxStick == nullptr) break;
		pxStick->SetRadius(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_VIRTUAL_STICK_DEADZONE:
	{
		Zenith_UI::Zenith_UIVirtualStick* pxStick = FindSelectedVirtualStick(xAction, "SET_UI_VIRTUAL_STICK_DEADZONE");
		if (pxStick == nullptr) break;
		pxStick->SetDeadzoneFraction(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_VIRTUAL_STICK_ACTIVATION_SLOP:
	{
		Zenith_UI::Zenith_UIVirtualStick* pxStick = FindSelectedVirtualStick(xAction, "SET_UI_VIRTUAL_STICK_ACTIVATION_SLOP");
		if (pxStick == nullptr) break;
		pxStick->SetActivationSlop(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::CREATE_UI_VIRTUAL_BUTTON:
	{
		Zenith_UIComponent* pxUI = GetSelectedUICanvasComponent("CREATE_UI_VIRTUAL_BUTTON");
		if (pxUI == nullptr) break;
		pxUI->CreateVirtualButton(xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_UI_VIRTUAL_BUTTON_ACTION:
	{
		Zenith_UI::Zenith_UIVirtualButton* pxButton = FindSelectedVirtualButton(xAction, "SET_UI_VIRTUAL_BUTTON_ACTION");
		if (pxButton == nullptr) break;
		pxButton->SetAction(xAction.m_szArg2.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_UI_VIRTUAL_BUTTON_HIT_SLOP:
	{
		Zenith_UI::Zenith_UIVirtualButton* pxButton = FindSelectedVirtualButton(xAction, "SET_UI_VIRTUAL_BUTTON_HIT_SLOP");
		if (pxButton == nullptr) break;
		pxButton->SetHitSlop(xAction.m_afArgs[0]);
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
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetNormalColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_HOVER_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_HOVER_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetHoverColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_PRESSED_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_PRESSED_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetPressedColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_FONT_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_FONT_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetFontSize(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_ICON:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_ICON");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetIconTexturePath(xAction.m_szArg2.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_ICON_SIZE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_ICON_SIZE");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetIconSize(xAction.m_afArgs[0], xAction.m_afArgs[1]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_ICON_PLACEMENT:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_ICON_PLACEMENT");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
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
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1.c_str());
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1.c_str());
		pxElement->SetBackgroundColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BACKGROUND_CORNER_RADIUS:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BACKGROUND_CORNER_RADIUS");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1.c_str());
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1.c_str());
		pxElement->SetBackgroundCornerRadius(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BACKGROUND_BORDER:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BACKGROUND_BORDER");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(xAction.m_szArg1.c_str());
		Zenith_Assert(pxElement, "UI element not found: %s", xAction.m_szArg1.c_str());
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
		Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxRect, "UI rect not found: %s", xAction.m_szArg1.c_str());
		pxRect->SetCornerRadius(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_GRADIENT_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_GRADIENT_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxRect, "UI rect not found: %s", xAction.m_szArg1.c_str());
		pxRect->SetGradientColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_SHADOW:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_SHADOW");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxRect, "UI rect not found: %s", xAction.m_szArg1.c_str());
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
		Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxRect, "UI rect not found: %s", xAction.m_szArg1.c_str());
		pxRect->SetShadowColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_RECT_BORDER:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_RECT_BORDER");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxRect, "UI rect not found: %s", xAction.m_szArg1.c_str());
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
		Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxText, "UI text not found: %s", xAction.m_szArg1.c_str());
		pxText->SetShadowEnabled(xAction.m_bArg);
		pxText->SetShadowOffset({xAction.m_afArgs[0], xAction.m_afArgs[1]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_TEXT_SHADOW_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_TEXT_SHADOW_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxText, "UI text not found: %s", xAction.m_szArg1.c_str());
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
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetCornerRadius(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_SHADOW:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_SHADOW");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
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
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetShadowColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_GRADIENT_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_GRADIENT_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetGradientColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_BORDER_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_BORDER_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetBorderColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_BORDER_THICKNESS:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_BORDER_THICKNESS");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetBorderThickness(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_TRANSITION_DURATION:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_TRANSITION_DURATION");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetTransitionDuration(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_TEXT_SHADOW:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_TEXT_SHADOW");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetTextShadowEnabled(xAction.m_bArg);
		pxButton->SetTextShadowOffset({xAction.m_afArgs[0], xAction.m_afArgs[1]});
		break;
	}

	case Zenith_EditorActionType::SET_UI_BUTTON_TEXT_SHADOW_COLOR:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for SET_UI_BUTTON_TEXT_SHADOW_COLOR");
		Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxButton, "UI button not found: %s", xAction.m_szArg1.c_str());
		pxButton->SetTextShadowColor({xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3]});
		break;
	}

	default:
		Zenith_Assert(false, "Non-UI action routed to ExecuteUIAction");
		break;
	}
}

bool Zenith_EditorAutomation::TryPreflightTerrainExportChunksRectAction(
	const Zenith_EditorAction& xAction, Zenith_TerrainEditor& xTerrainEditor)
{
	if (xAction.m_eType != Zenith_EditorActionType::TERRAIN_EDITOR_EXPORT_CHUNKS_RECT)
	{
		return false;
	}

	bool bSucceeded = false;
	return TryRouteTerrainEditorAction(xAction, xTerrainEditor,
		TerrainRectExecutionMode::PreflightOnly, bSucceeded) && bSucceeded;
}

//-----------------------------------------------------------------------------
// Field-edit sub-executors. Each covers one CONTIGUOUS enum range, mirroring
// the Terrain/UI/Material splits above — ExecuteAction routes the whole range
// here instead of carrying every case in its own switch. Keep each range
// contiguous in Zenith_EditorAutomation.h when adding action types.
//-----------------------------------------------------------------------------

// Camera field edits (SET_CAMERA_POSITION .. SET_MAIN_CAMERA).
static void ExecuteCameraAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::SET_CAMERA_POSITION:
		GetSelectedEntityChecked("SET_CAMERA_POSITION").GetComponent<Zenith_CameraComponent>().SetPosition(
			{xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]});
		break;

	case Zenith_EditorActionType::SET_CAMERA_PITCH:
		GetSelectedEntityChecked("SET_CAMERA_PITCH").GetComponent<Zenith_CameraComponent>().SetPitch(xAction.m_afArgs[0]);
		break;

	case Zenith_EditorActionType::SET_CAMERA_YAW:
		GetSelectedEntityChecked("SET_CAMERA_YAW").GetComponent<Zenith_CameraComponent>().SetYaw(xAction.m_afArgs[0]);
		break;

	case Zenith_EditorActionType::SET_CAMERA_FOV:
		GetSelectedEntityChecked("SET_CAMERA_FOV").GetComponent<Zenith_CameraComponent>().SetFOV(xAction.m_afArgs[0]);
		break;

	case Zenith_EditorActionType::SET_CAMERA_NEAR:
		GetSelectedEntityChecked("SET_CAMERA_NEAR").GetComponent<Zenith_CameraComponent>().SetNearPlane(xAction.m_afArgs[0]);
		break;

	case Zenith_EditorActionType::SET_CAMERA_FAR:
		GetSelectedEntityChecked("SET_CAMERA_FAR").GetComponent<Zenith_CameraComponent>().SetFarPlane(xAction.m_afArgs[0]);
		break;

	case Zenith_EditorActionType::SET_CAMERA_ASPECT:
		GetSelectedEntityChecked("SET_CAMERA_ASPECT").GetComponent<Zenith_CameraComponent>().SetAspectRatio(xAction.m_afArgs[0]);
		break;

	case Zenith_EditorActionType::SET_MAIN_CAMERA:
		g_xEngine.Editor().SetSelectedAsMainCamera();
		break;

	default:
		Zenith_Assert(false, "Non-camera action routed to ExecuteCameraAction");
		break;
	}
}

// Transform field edits (SET_TRANSFORM_POSITION .. SET_TRANSFORM_ROTATION_QUAT).
static void ExecuteTransformAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::SET_TRANSFORM_POSITION:
		GetSelectedEntityChecked("SET_TRANSFORM_POSITION").GetComponent<Zenith_TransformComponent>().SetPosition(
			{xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]});
		break;

	case Zenith_EditorActionType::SET_TRANSFORM_SCALE:
		GetSelectedEntityChecked("SET_TRANSFORM_SCALE").GetComponent<Zenith_TransformComponent>().SetScale(
			{xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]});
		break;

	case Zenith_EditorActionType::SET_TRANSFORM_ROTATION_YAW:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_TRANSFORM_ROTATION_YAW");
		const float fYaw = xAction.m_afArgs[0];
		const Zenith_Maths::Quat xRot = Zenith_Maths::AuthoringRotationY(fYaw);
		xEntity.GetComponent<Zenith_TransformComponent>().SetRotation(xRot);
		break;
	}

	case Zenith_EditorActionType::SET_TRANSFORM_ROTATION:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_TRANSFORM_ROTATION");
		const Zenith_Maths::Quat xRot = Zenith_EditorAutomation::BuildEulerRotation(
			xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]);
		xEntity.GetComponent<Zenith_TransformComponent>().SetRotation(xRot);
		break;
	}

	case Zenith_EditorActionType::SET_TRANSFORM_ROTATION_QUAT:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_TRANSFORM_ROTATION_QUAT");
		// NO MATH, DELIBERATELY -- see the header. The args arrive in SERIALIZED
		// order (x, y, z, w) and glm::quat's constructor takes (w, x, y, z), so the
		// reorder here is the whole body of this case. SetRotation stores the value
		// verbatim (it does not normalize), which is what makes this step the only
		// way to land a chosen bit pattern in a committed scene file.
		const Zenith_Maths::Quat xRot(
			xAction.m_afArgs[3], xAction.m_afArgs[0],
			xAction.m_afArgs[1], xAction.m_afArgs[2]);
		xEntity.GetComponent<Zenith_TransformComponent>().SetRotation(xRot);
		break;
	}

	default:
		Zenith_Assert(false, "Non-transform action routed to ExecuteTransformAction");
		break;
	}
}

// Light + sun field edits (SET_LIGHT_INTENSITY .. SET_SUN_TIME_OF_DAY).
static void ExecuteLightAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::SET_LIGHT_INTENSITY:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_LIGHT_INTENSITY");
		Zenith_Assert(xEntity.HasComponent<Zenith_LightComponent>(),
			"SET_LIGHT_INTENSITY: selected entity has no LightComponent");
		xEntity.GetComponent<Zenith_LightComponent>().SetIntensity(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_LIGHT_RANGE:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_LIGHT_RANGE");
		Zenith_Assert(xEntity.HasComponent<Zenith_LightComponent>(),
			"SET_LIGHT_RANGE: selected entity has no LightComponent");
		xEntity.GetComponent<Zenith_LightComponent>().SetRange(xAction.m_afArgs[0]);
		break;
	}

	case Zenith_EditorActionType::SET_LIGHT_COLOR:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_LIGHT_COLOR");
		Zenith_Assert(xEntity.HasComponent<Zenith_LightComponent>(),
			"SET_LIGHT_COLOR: selected entity has no LightComponent");
		xEntity.GetComponent<Zenith_LightComponent>().SetColor(
			Zenith_Maths::Vector3(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]));
		break;
	}

	case Zenith_EditorActionType::SET_LIGHT_POSITION_OFFSET:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_LIGHT_POSITION_OFFSET");
		Zenith_Assert(xEntity.HasComponent<Zenith_LightComponent>(),
			"SET_LIGHT_POSITION_OFFSET: selected entity has no LightComponent");
		Zenith_LightComponent& xLight = xEntity.GetComponent<Zenith_LightComponent>();
		xLight.SetLocalPositionOffset(
			Zenith_Maths::Vector3(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]));
		// Enabled here rather than left to a second verb: see the header.
		xLight.SetUsePositionOffset(true);
		break;
	}

	case Zenith_EditorActionType::SET_SUN_DIRECTION:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_SUN_DIRECTION");
		Zenith_Assert(xEntity.HasComponent<Zenith_SunComponent>(),
			"SET_SUN_DIRECTION: selected entity has no SunComponent");
		xEntity.GetComponent<Zenith_SunComponent>().SetDirection(
			Zenith_Maths::Vector3(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]));
		break;
	}

	case Zenith_EditorActionType::SET_SUN_TIME_OF_DAY:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_SUN_TIME_OF_DAY");
		Zenith_Assert(xEntity.HasComponent<Zenith_SunComponent>(),
			"SET_SUN_TIME_OF_DAY: selected entity has no SunComponent");
		Zenith_SunComponent& xSun = xEntity.GetComponent<Zenith_SunComponent>();
		xSun.SetOrbitAzimuthDegrees(xAction.m_afArgs[1]);
		xSun.SetTimeOfDayAngleDegrees(xAction.m_afArgs[0]);
		break;
	}

	default:
		Zenith_Assert(false, "Non-light action routed to ExecuteLightAction");
		break;
	}
}

// Graph authoring actions (GRAPH_OPEN_FRESH .. GRAPH_CLOSE). ATTACH_GRAPH is
// NOT part of this range (the enum has the UI/Material blocks between them)
// and stays in ExecuteAction's own switch.
static void ExecuteGraphAuthoringAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::GRAPH_OPEN_FRESH:            Zenith_GraphEditorPanel::OpenAssetFresh(xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRAPH_ADD_NODE:              GraphActionChecked(Zenith_GraphEditorPanel::Action_AddNode(xAction.m_szArg1.c_str()), "GraphAddNode", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRAPH_SELECT_NODE:           GraphActionChecked(Zenith_GraphEditorPanel::Action_SelectNode(xAction.m_szArg1.c_str(), static_cast<u_int>(xAction.m_aiArgs[0])), "GraphSelectNode", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRAPH_SET_NODE_PARAM_FLOAT:  GraphActionChecked(Zenith_GraphEditorPanel::Action_SetSelectedNodeParamFloat(xAction.m_szArg1.c_str(), xAction.m_afArgs[0]), "GraphSetNodeParamFloat", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRAPH_SET_NODE_PARAM_STRING: GraphActionChecked(Zenith_GraphEditorPanel::Action_SetSelectedNodeParamString(xAction.m_szArg1.c_str(), xAction.m_szArg2.c_str()), "GraphSetNodeParamString", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRAPH_SET_NODE_PARAM_INT:    GraphActionChecked(Zenith_GraphEditorPanel::Action_SetSelectedNodeParamInt(xAction.m_szArg1.c_str(), xAction.m_aiArgs[0]), "GraphSetNodeParamInt", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRAPH_SET_NODE_PARAM_BOOL:   GraphActionChecked(Zenith_GraphEditorPanel::Action_SetSelectedNodeParamBool(xAction.m_szArg1.c_str(), xAction.m_bArg), "GraphSetNodeParamBool", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRAPH_SET_NODE_PARAM_VEC3:   GraphActionChecked(Zenith_GraphEditorPanel::Action_SetSelectedNodeParamVec3(xAction.m_szArg1.c_str(), xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]), "GraphSetNodeParamVec3", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRAPH_CONNECT:               GraphActionChecked(Zenith_GraphEditorPanel::Action_Connect(xAction.m_szArg1.c_str(), static_cast<u_int>(xAction.m_aiArgs[0]), static_cast<u_int>(xAction.m_afArgs[0]), xAction.m_szArg2.c_str(), static_cast<u_int>(xAction.m_aiArgs[1])), "GraphConnect", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRAPH_ADD_VARIABLE:          GraphActionChecked(Zenith_GraphEditorPanel::Action_AddVariable(xAction.m_szArg1.c_str(), xAction.m_szArg2.c_str(), xAction.m_afArgs[0]), "GraphAddVariable", xAction.m_szArg1.c_str()); break;
	case Zenith_EditorActionType::GRAPH_SAVE:                  Zenith_GraphEditorPanel::Save(); break;
	case Zenith_EditorActionType::GRAPH_BUILD:
	{
		// Programmatic bulk authoring: build the whole definition through the
		// game's builder function, save through the asset registry, queue hot
		// reload (which refreshes any registry-cached instance from the new
		// disk bytes + re-instantiates live slots).
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphBuilder xBuilder(xAsset.GetDefinition());
		if (xAction.m_pfnGraphBuild != nullptr)
		{
			xAction.m_pfnGraphBuild(xBuilder);
		}
		const bool bBuilt = xBuilder.Build();
		GraphActionChecked(bBuilt && xAction.m_pfnGraphBuild != nullptr, "GraphBuild", xAction.m_szArg1.c_str());
		if (bBuilt)
		{
			std::error_code xEC;
			std::filesystem::create_directories(
				std::filesystem::path(Zenith_AssetRegistry::ResolvePath(xAction.m_szArg1)).parent_path(), xEC);
			GraphActionChecked(Zenith_AssetRegistry::Save(&xAsset, xAction.m_szArg1), "GraphBuildSave", xAction.m_szArg1.c_str());
			Zenith_GraphReload::NotifyAssetChanged(xAction.m_szArg1.c_str());
		}
		break;
	}
	case Zenith_EditorActionType::GRAPH_CLOSE:                 Zenith_GraphEditorPanel::Close(); break;

	default:
		Zenith_Assert(false, "Non-graph action routed to ExecuteGraphAuthoringAction");
		break;
	}
}

//=============================================================================
// Animation dope-sheet authoring actions (ANIM_OPEN_CLIP .. ANIM_EXPECT_
// SELECTED_COUNT). Every case ends in one of Zenith_EditorPanel_Animation's
// Action_* twins — the SAME call the panel's mouse handler makes — so a recipe
// and a human's gesture run one code path, and nothing here reaches past the
// panel into the document except to READ.
//=============================================================================
namespace
{
	// The (bone, track) an ANIM step names. An empty bone means ROOT MOTION:
	// there is no bone to type there, and dropping the case would leave the two
	// root-motion rows unreachable from a recipe.
	Zenith_AnimTrackId AnimTrackFromAction(const Zenith_EditorAction& xAction)
	{
		const Flux_AnimTrack eTrack = static_cast<Flux_AnimTrack>(xAction.m_aiArgs[0]);
		if (xAction.m_szArg1.empty())
		{
			return Zenith_AnimTrackId::RootMotion(eTrack);
		}
		return Zenith_AnimTrackId::Bone(xAction.m_szArg1, eTrack);
	}

	// ★ INDEX -> STABLE ID, AT EXECUTION TIME. See the header: a recipe can only
	// name "the second key on this track", and the panel only accepts an id.
	// Resolving here — against the track as it stands at THIS step, not as it
	// stood when the queue was built — is what makes a retime earlier in the
	// recipe leave the later steps still naming what a reader would expect.
	u_int AnimResolveKeyId(const Zenith_AnimationDocument& xDocument,
		const Zenith_AnimTrackId& xTrack, int iKeyIndex)
	{
		if (iKeyIndex < 0)
		{
			return uINVALID_ANIM_KEY_ID;
		}
		return xDocument.GetKeyIdAtIndex(xTrack, static_cast<u_int>(iKeyIndex));
	}
}

static void ExecuteAnimationAction(const Zenith_EditorAction& xAction)
{
	Zenith_EditorPanel_Animation& xPanel = Zenith_EditorPanel_Animation::Instance();
	const Zenith_AnimSelectMode eMode = static_cast<Zenith_AnimSelectMode>(xAction.m_aiArgs[2]);

	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::ANIM_OPEN_CLIP:
		// The window first, then the clip: OpenClip's own refusals (a GENERATED
		// clip, a path that does not resolve) are visible on the sheet, and a
		// hidden panel would report them to nobody.
		xPanel.ShowFlag() = true;
		AnimActionChecked(xPanel.OpenClip(xAction.m_szArg1), "AnimOpenClip", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_CLOSE_CLIP:
		xPanel.CloseClip();
		break;

	case Zenith_EditorActionType::ANIM_SELECT_KEY:
	{
		const Zenith_AnimTrackId xTrack = AnimTrackFromAction(xAction);
		const u_int uKeyId = AnimResolveKeyId(xPanel.Document(), xTrack, xAction.m_aiArgs[1]);
		AnimActionChecked(uKeyId != uINVALID_ANIM_KEY_ID && xPanel.Action_SelectKey(xTrack, uKeyId, eMode),
			"AnimSelectKey", xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::ANIM_BOX_SELECT:
		AnimActionChecked(xPanel.Action_BoxSelect(xAction.m_afArgs[0], xAction.m_afArgs[1],
			xAction.m_afArgs[2], xAction.m_afArgs[3], eMode), "AnimBoxSelect", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_MOVE_SELECTION:
		AnimActionChecked(xPanel.Action_MoveSelection(xAction.m_afArgs[0], xAction.m_bArg),
			"AnimMoveSelection", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_DELETE_SELECTION:
		AnimActionChecked(xPanel.Action_DeleteSelection(), "AnimDeleteSelection", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_DUPLICATE_SELECTION:
		AnimActionChecked(xPanel.Action_DuplicateSelection(), "AnimDuplicateSelection", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_COPY_SELECTION:
		AnimActionChecked(xPanel.Action_CopySelection(), "AnimCopySelection", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_PASTE_TO_BONE:
		AnimActionChecked(xPanel.Action_PasteToBone(xAction.m_szArg1, xAction.m_afArgs[0]),
			"AnimPasteToBone", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_RIPPLE_RETIME:
		AnimActionChecked(xPanel.Action_RippleRetime(xAction.m_afArgs[0], xAction.m_afArgs[1]),
			"AnimRippleRetime", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_SCRUB:
		AnimActionChecked(xPanel.Action_Scrub(xAction.m_afArgs[0]), "AnimScrub", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_SET_DURATION:
		AnimActionChecked(xPanel.Action_SetDuration(xAction.m_afArgs[0]), "AnimSetDuration", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_UNDO:
		AnimActionChecked(xPanel.Action_Undo(), "AnimUndo", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_REDO:
		AnimActionChecked(xPanel.Action_Redo(), "AnimRedo", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_EXPECT_KEY_TIME:
	{
		const Zenith_AnimTrackId xTrack = AnimTrackFromAction(xAction);
		const u_int uKeyId = AnimResolveKeyId(xPanel.Document(), xTrack, xAction.m_aiArgs[1]);
		float fTime = 0.0f;
		const bool bResolved = uKeyId != uINVALID_ANIM_KEY_ID
			&& xPanel.Document().GetKeyTime(xTrack, uKeyId, fTime);
		// Two asserts rather than one: "there is no such key" and "the key is at
		// the wrong time" are different mistakes, and a single combined message
		// would print a meaningless 0.0 for the first.
		AnimActionChecked(bResolved, "AnimExpectKeyTime (no such key)", xAction.m_szArg1.c_str());
		if (bResolved)
		{
			Zenith_Assert(std::fabs(fTime - xAction.m_afArgs[0]) <= xAction.m_afArgs[1],
				"EditorAutomation AnimExpectKeyTime('%s' track %d key %d): expected %.6f s, found %.6f s (tolerance %.6f)",
				xAction.m_szArg1.c_str(), xAction.m_aiArgs[0], xAction.m_aiArgs[1],
				xAction.m_afArgs[0], fTime, xAction.m_afArgs[1]);
		}
		break;
	}

	case Zenith_EditorActionType::ANIM_EXPECT_SELECTED_COUNT:
	{
		const u_int uActual = xPanel.GetSelectedKeyCount();
		Zenith_Assert(uActual == static_cast<u_int>(xAction.m_aiArgs[0]),
			"EditorAutomation AnimExpectSelectedCount: expected %d selected keys, found %u",
			xAction.m_aiArgs[0], uActual);
		(void)uActual;
		break;
	}

	default:
		Zenith_Assert(false, "Non-animation action routed to ExecuteAnimationAction");
		break;
	}
}

//-----------------------------------------------------------------------------
// Animation POSE authoring (WU-4.3): ANIM_POSE_SELECT_BONE ..
// ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION.
//-----------------------------------------------------------------------------
static void ExecuteAnimationPoseAction(const Zenith_EditorAction& xAction)
{
	Zenith_EditorPanel_Animation& xPanel = Zenith_EditorPanel_Animation::Instance();

	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::ANIM_POSE_SELECT_BONE:
		AnimActionChecked(xPanel.Action_SelectBone(static_cast<u_int>(xAction.m_aiArgs[0])),
			"AnimSelectBone", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_POSE_ROTATE_SELECTED_BONE_WORLD:
	{
		// ★ NO glm ON THIS PATH. A pose authored at boot is SERIALIZED, and
		// glm::angleAxis is a header inline whose floating-point model comes from
		// its own definition point — the Debug and Release tools builds then
		// disagree in the last bit or two and the tracked .zanim ping-pongs in
		// `git status` forever, invisibly to every tolerance-based guard (see
		// Editor/CLAUDE.md and ZM-D-183). Zenith_Maths::AuthoringRotation{X,Y,Z}
		// are single non-inline definitions under the pinned model, and they
		// cover exactly the three cardinal axes — so a non-cardinal axis is
		// refused here rather than quietly computed the unpinned way.
		const float fRadians = Zenith_Maths::AuthoringRadians(xAction.m_afArgs[3]);
		const bool bAxisX = (xAction.m_afArgs[0] == 1.0f) && (xAction.m_afArgs[1] == 0.0f) && (xAction.m_afArgs[2] == 0.0f);
		const bool bAxisY = (xAction.m_afArgs[0] == 0.0f) && (xAction.m_afArgs[1] == 1.0f) && (xAction.m_afArgs[2] == 0.0f);
		const bool bAxisZ = (xAction.m_afArgs[0] == 0.0f) && (xAction.m_afArgs[1] == 0.0f) && (xAction.m_afArgs[2] == 1.0f);
		Zenith_Assert(bAxisX || bAxisY || bAxisZ,
			"EditorAutomation AnimRotateSelectedBoneWorld: the axis must be cardinal (1,0,0)/(0,1,0)/(0,0,1), got (%.3f, %.3f, %.3f) - see the header",
			xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]);

		Zenith_Maths::Quat xDelta(1.0f, 0.0f, 0.0f, 0.0f);
		if (bAxisX)      { xDelta = Zenith_Maths::AuthoringRotationX(fRadians); }
		else if (bAxisY) { xDelta = Zenith_Maths::AuthoringRotationY(fRadians); }
		else if (bAxisZ) { xDelta = Zenith_Maths::AuthoringRotationZ(fRadians); }

		AnimActionChecked((bAxisX || bAxisY || bAxisZ) && xPanel.Action_RotateSelectedBoneWorld(xDelta),
			"AnimRotateSelectedBoneWorld", nullptr);
		break;
	}

	case Zenith_EditorActionType::ANIM_POSE_SET_KEY_FOR_SELECTED_BONE:
		AnimActionChecked(xPanel.Action_SetKeyForSelectedBone(), "AnimSetKeyForSelectedBone", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_POSE_SET_AUTO_KEY:
		// NOT AnimActionChecked: the action reports whether the value CHANGED, and
		// setting auto-key to what it already is asks for the state the recipe
		// wanted and gets it. Asserting on `false` here would fail a recipe that
		// merely stated its assumption twice.
		xPanel.Action_SetAutoKey(xAction.m_bArg);
		Zenith_Assert(xPanel.Action_GetAutoKey() == xAction.m_bArg,
			"EditorAutomation AnimSetAutoKey: auto-key did not take");
		break;

	case Zenith_EditorActionType::ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION:
	{
		const u_int uBone = static_cast<u_int>(xAction.m_aiArgs[0]);
		// Two asserts rather than one, for the same reason AnimExpectKeyTime has
		// two: "the rig has no such bone" and "the bone is at the wrong rotation"
		// are different mistakes and a combined message would print an identity
		// quaternion for the first.
		const bool bResolved = xPanel.Session().IsOpen() && uBone < xPanel.Session().GetBoneCount();
		AnimActionChecked(bResolved, "AnimExpectBoneLocalRotation (no such bone)", nullptr);
		if (bResolved)
		{
			const Zenith_Maths::Quat xActual = xPanel.Session().GetBoneLocalRotation(uBone);
			const float fTolerance = xAction.m_afArgs[4];
			// ★ COMPARED AS (x, y, z, w) AND AS ITS NEGATION. q and -q are the SAME
			// rotation, and every quaternion product is free to return either — a
			// component-wise compare that did not allow the sign flip would fail on
			// a pose that is exactly right.
			const float fDx = xActual.x - xAction.m_afArgs[0];
			const float fDy = xActual.y - xAction.m_afArgs[1];
			const float fDz = xActual.z - xAction.m_afArgs[2];
			const float fDw = xActual.w - xAction.m_afArgs[3];
			const float fSx = xActual.x + xAction.m_afArgs[0];
			const float fSy = xActual.y + xAction.m_afArgs[1];
			const float fSz = xActual.z + xAction.m_afArgs[2];
			const float fSw = xActual.w + xAction.m_afArgs[3];
			const bool bSame = std::fabs(fDx) <= fTolerance && std::fabs(fDy) <= fTolerance
				&& std::fabs(fDz) <= fTolerance && std::fabs(fDw) <= fTolerance;
			const bool bNegated = std::fabs(fSx) <= fTolerance && std::fabs(fSy) <= fTolerance
				&& std::fabs(fSz) <= fTolerance && std::fabs(fSw) <= fTolerance;
			Zenith_Assert(bSame || bNegated,
				"EditorAutomation AnimExpectBoneLocalRotation(bone %u): expected (%.6f, %.6f, %.6f, %.6f), found (%.6f, %.6f, %.6f, %.6f) (tolerance %.6f)",
				uBone, xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2], xAction.m_afArgs[3],
				xActual.x, xActual.y, xActual.z, xActual.w, fTolerance);
		}
		break;
	}

	default:
		Zenith_Assert(false, "Non-pose action routed to ExecuteAnimationPoseAction");
		break;
	}
}

//-----------------------------------------------------------------------------
// Animator-controller STATE MACHINE authoring (WU-6.5): ANIM_SM_OPEN ..
// ANIM_SM_EXPECT_DEFAULT_STATE. Every case ends in one of
// Zenith_EditorPanel_AnimStateMachine's Action_* twins — the SAME call the
// panel's mouse handler makes — so a recipe and a human's gesture run one code
// path, and nothing here reaches past the panel into the document except to
// READ.
//-----------------------------------------------------------------------------
namespace
{
	// A machine selector out of the step's int: negative means the def's
	// TOP-LEVEL machine, which has no layer id to type.
	u_int AnimSmMachineIdFromAction(const Zenith_EditorAction& xAction)
	{
		return xAction.m_aiArgs[0] < 0
			? uANIMCTRL_TOP_LEVEL_MACHINE
			: static_cast<u_int>(xAction.m_aiArgs[0]);
	}

	// The same contract as AnimActionChecked, with its own message so a failing
	// recipe names the family it came from.
	void AnimSmActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation animator-controller step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}
}

static void ExecuteAnimStateMachineAction(const Zenith_EditorAction& xAction)
{
	Zenith_EditorPanel_AnimStateMachine& xPanel = Zenith_EditorPanel_AnimStateMachine::Instance();
	const u_int uIndex = static_cast<u_int>(xAction.m_aiArgs[0] < 0 ? 0 : xAction.m_aiArgs[0]);

	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::ANIM_SM_OPEN:
		// The window first, then the asset: OpenAsset's refusals are visible on
		// the panel, and a hidden panel would report them to nobody.
		xPanel.ShowFlag() = true;
		AnimSmActionChecked(xPanel.OpenAsset(xAction.m_szArg1), "AnimSmOpen", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_OPEN_FRESH:
		xPanel.ShowFlag() = true;
		AnimSmActionChecked(xPanel.OpenAssetFresh(xAction.m_szArg1), "AnimSmOpenFresh", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_CLOSE:
		xPanel.CloseAsset();
		break;

	case Zenith_EditorActionType::ANIM_SM_SELECT_LAYER:
		AnimSmActionChecked(xPanel.Action_SelectLayerMachine(AnimSmMachineIdFromAction(xAction)),
			"AnimSmSelectLayer", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_SM_ADD_CLIP_PATH:
		AnimSmActionChecked(xPanel.Action_AddClipPath(xAction.m_szArg1), "AnimSmAddClipPath", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_ADD_STATE:
		AnimSmActionChecked(xPanel.Action_AddState(xAction.m_szArg1), "AnimSmAddState", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_REMOVE_STATE:
		AnimSmActionChecked(xPanel.Action_RemoveState(xAction.m_szArg1), "AnimSmRemoveState", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_RENAME_STATE:
		AnimSmActionChecked(xPanel.Action_RenameState(xAction.m_szArg1, xAction.m_szArg2),
			"AnimSmRenameState", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_SET_DEFAULT_STATE:
		AnimSmActionChecked(xPanel.Action_SetDefaultState(xAction.m_szArg1),
			"AnimSmSetDefaultState", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_SET_STATE_CLIP:
		AnimSmActionChecked(xPanel.Action_SetStateClip(xAction.m_szArg1, xAction.m_szArg2),
			"AnimSmSetStateClip", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_ADD_TRANSITION:
		AnimSmActionChecked(xPanel.Action_AddTransition(xAction.m_szArg1, xAction.m_szArg2),
			"AnimSmAddTransition", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_REMOVE_TRANSITION:
		AnimSmActionChecked(xPanel.Action_RemoveTransition(xAction.m_szArg1, uIndex),
			"AnimSmRemoveTransition", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_SET_TRANSITION_DURATION:
		AnimSmActionChecked(xPanel.Action_SetTransitionDuration(xAction.m_szArg1, uIndex, xAction.m_afArgs[0]),
			"AnimSmSetTransitionDuration", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_SET_TRANSITION_EXIT_TIME:
		AnimSmActionChecked(xPanel.Action_SetTransitionExitTime(xAction.m_szArg1, uIndex,
			xAction.m_bArg, xAction.m_afArgs[0]), "AnimSmSetTransitionExitTime", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_SET_TRANSITION_INTERRUPTIBLE:
		AnimSmActionChecked(xPanel.Action_SetTransitionInterruptible(xAction.m_szArg1, uIndex, xAction.m_bArg),
			"AnimSmSetTransitionInterruptible", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_ADD_CONDITION:
		AnimSmActionChecked(xPanel.Action_AddCondition(xAction.m_szArg1, uIndex, xAction.m_szArg2,
			static_cast<Flux_TransitionCondition::CompareOp>(xAction.m_aiArgs[1]), xAction.m_afArgs[0]),
			"AnimSmAddCondition", xAction.m_szArg2.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_REMOVE_CONDITION:
		AnimSmActionChecked(xPanel.Action_RemoveCondition(xAction.m_szArg1, uIndex,
			static_cast<u_int>(xAction.m_aiArgs[1] < 0 ? 0 : xAction.m_aiArgs[1])),
			"AnimSmRemoveCondition", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_ADD_PARAMETER:
		AnimSmActionChecked(xPanel.Action_AddParameter(xAction.m_szArg1,
			static_cast<Flux_AnimationParameters::ParamType>(xAction.m_aiArgs[0]), xAction.m_afArgs[0]),
			"AnimSmAddParameter", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_REMOVE_PARAMETER:
		AnimSmActionChecked(xPanel.Action_RemoveParameter(xAction.m_szArg1),
			"AnimSmRemoveParameter", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_UNDO:
		AnimSmActionChecked(xPanel.Action_Undo(), "AnimSmUndo", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_SM_REDO:
		AnimSmActionChecked(xPanel.Action_Redo(), "AnimSmRedo", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_SM_SAVE:
		AnimSmActionChecked(xPanel.Action_Save(), "AnimSmSave", xPanel.Document().GetAssetPath().c_str());
		break;

	case Zenith_EditorActionType::ANIM_SM_APPLY:
		AnimSmActionChecked(xPanel.Action_Apply(), "AnimSmApply", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_SM_EXPECT_STATE_COUNT:
	{
		const u_int uActual = xPanel.Document().GetStateCount();
		Zenith_Assert(uActual == static_cast<u_int>(xAction.m_aiArgs[0]),
			"EditorAutomation AnimSmExpectStateCount: expected %d states, found %u",
			xAction.m_aiArgs[0], uActual);
		(void)uActual;
		break;
	}

	case Zenith_EditorActionType::ANIM_SM_EXPECT_DEFAULT_STATE:
	{
		const std::string& strActual = xPanel.Document().GetDefaultStateName();
		Zenith_Assert(strActual == xAction.m_szArg1,
			"EditorAutomation AnimSmExpectDefaultState: expected '%s', found '%s'",
			xAction.m_szArg1.c_str(), strActual.c_str());
		(void)strActual;
		break;
	}

	default:
		Zenith_Assert(false, "Non-state-machine action routed to ExecuteAnimStateMachineAction");
		break;
	}
}

//-----------------------------------------------------------------------------
// BONE MASK authoring (WU-7.1): ANIM_MASK_OPEN .. ANIM_MASK_EXPECT_WEIGHT.
// Every case ends in one of Zenith_EditorPanel_Animation's Action_Mask* twins —
// the SAME call the "Bone Masks" section's own slider handler makes — so a
// recipe and a human's gesture run one code path, and nothing here reaches past
// the panel into the document except to READ.
//-----------------------------------------------------------------------------
namespace
{
	// The same contract as AnimActionChecked and AnimSmActionChecked, with its
	// own message so a failing recipe names the family it came from.
	void AnimMaskActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation bone-mask step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}
}

static void ExecuteAnimMaskAction(const Zenith_EditorAction& xAction)
{
	Zenith_EditorPanel_Animation& xPanel = Zenith_EditorPanel_Animation::Instance();

	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::ANIM_MASK_OPEN:
		AnimMaskActionChecked(xPanel.Action_MaskOpen(xAction.m_szArg1), "AnimMaskOpen", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_MASK_OPEN_FRESH:
		AnimMaskActionChecked(xPanel.Action_MaskOpenFresh(xAction.m_szArg1),
			"AnimMaskOpenFresh", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_MASK_CLOSE:
		// ★ NOT CHECKED. Closing when nothing is open is a legitimate thing for a
		// recipe to do defensively at the top of a block, and Action_MaskClose
		// reports false for it — asserting there would fail a boot over a
		// no-op. Every other verb in this family has a real failure to report.
		xPanel.Action_MaskClose();
		break;

	case Zenith_EditorActionType::ANIM_MASK_SET_WEIGHT:
		AnimMaskActionChecked(xPanel.Action_MaskSetWeight(xAction.m_szArg1, xAction.m_afArgs[0]),
			"AnimMaskSetWeight", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_MASK_SET_SUBTREE:
		// The likely failure here is "no rig previewed", which is why the message
		// carries the bone name: the recipe's own AnimOpenClip is the fix.
		AnimMaskActionChecked(xPanel.Action_MaskSetSubtree(xAction.m_szArg1, xAction.m_afArgs[0]),
			"AnimMaskSetSubtree", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_MASK_SET_HAS_AVATAR:
		AnimMaskActionChecked(xPanel.Action_MaskSetHasAvatar(xAction.m_bArg), "AnimMaskSetHasAvatar", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_MASK_UNDO:
		AnimMaskActionChecked(xPanel.Action_MaskUndo(), "AnimMaskUndo", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_MASK_REDO:
		AnimMaskActionChecked(xPanel.Action_MaskRedo(), "AnimMaskRedo", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_MASK_SAVE:
		AnimMaskActionChecked(xPanel.Action_MaskSave(), "AnimMaskSave",
			xPanel.MaskDocument().GetAssetPath().c_str());
		break;

	case Zenith_EditorActionType::ANIM_MASK_EXPECT_WEIGHT:
	{
		const float fActual = xPanel.MaskDocument().GetBoneWeight(xAction.m_szArg1);
		const float fTolerance = xAction.m_afArgs[1] > 0.0f ? xAction.m_afArgs[1] : 1.0e-4f;
		Zenith_Assert(std::fabs(fActual - xAction.m_afArgs[0]) <= fTolerance,
			"EditorAutomation AnimMaskExpectWeight: bone '%s' expected %f, found %f",
			xAction.m_szArg1.c_str(), xAction.m_afArgs[0], fActual);
		(void)fActual; (void)fTolerance;
		break;
	}

	default:
		Zenith_Assert(false, "Non-bone-mask action routed to ExecuteAnimMaskAction");
		break;
	}
}

//-----------------------------------------------------------------------------
// ANIMATOR LAYER authoring (WU-7.2): ANIM_LAYER_ADD .. ANIM_LAYER_EXPECT_ORDER.
// Every case ends in one of Zenith_EditorPanel_AnimStateMachine's layer
// Action_* twins — the SAME call the "Layers" strip's own handler makes — so a
// recipe and a human's gesture run one code path, and nothing here reaches past
// the panel into the document except to READ.
//-----------------------------------------------------------------------------
namespace
{
	// The same contract as AnimSmActionChecked, with its own message so a failing
	// recipe names the family it came from.
	void AnimLayerActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation animator-layer step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}

	// A LAYER ID out of a step's int. Negative is not a top-level selector here —
	// every verb in this family addresses a layer — so it is folded to the
	// never-minted sentinel and the checked wrapper reports it.
	u_int AnimLayerIdFromAction(const Zenith_EditorAction& xAction)
	{
		return xAction.m_aiArgs[0] < 0 ? uFLUX_INVALID_LAYER_ID : static_cast<u_int>(xAction.m_aiArgs[0]);
	}
}

static void ExecuteAnimLayerAction(const Zenith_EditorAction& xAction)
{
	Zenith_EditorPanel_AnimStateMachine& xPanel = Zenith_EditorPanel_AnimStateMachine::Instance();
	const u_int uLayerId = AnimLayerIdFromAction(xAction);

	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::ANIM_LAYER_ADD:
		AnimLayerActionChecked(xPanel.Action_AddLayer(xAction.m_szArg1), "AnimLayerAdd", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_LAYER_REMOVE:
		AnimLayerActionChecked(xPanel.Action_RemoveLayer(uLayerId), "AnimLayerRemove", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_LAYER_RENAME:
		AnimLayerActionChecked(xPanel.Action_RenameLayer(uLayerId, xAction.m_szArg1),
			"AnimLayerRename", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_LAYER_SET_WEIGHT:
		AnimLayerActionChecked(xPanel.Action_SetLayerWeight(uLayerId, xAction.m_afArgs[0]),
			"AnimLayerSetWeight", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_LAYER_SET_BLEND_MODE:
		AnimLayerActionChecked(xPanel.Action_SetLayerBlendMode(uLayerId,
			xAction.m_aiArgs[1] == 1 ? LAYER_BLEND_ADDITIVE : LAYER_BLEND_OVERRIDE),
			"AnimLayerSetBlendMode", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_LAYER_SET_EMIT_EVENTS:
		AnimLayerActionChecked(xPanel.Action_SetLayerEmitEvents(uLayerId, xAction.m_bArg),
			"AnimLayerSetEmitEvents", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_LAYER_SET_MASK_PATH:
		// The likely failure here is "that layer is additive", which is why the
		// message carries the PATH: the fix is either the blend mode or the step.
		AnimLayerActionChecked(xPanel.Action_SetLayerMaskAssetPath(uLayerId, xAction.m_szArg1),
			"AnimLayerSetMaskPath", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_LAYER_MOVE:
		AnimLayerActionChecked(xPanel.Action_MoveLayer(uLayerId,
			static_cast<u_int>(xAction.m_aiArgs[1] < 0 ? 0 : xAction.m_aiArgs[1])),
			"AnimLayerMove", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_LAYER_SELECT:
		AnimLayerActionChecked(xPanel.Action_SelectLayer(uLayerId), "AnimLayerSelect", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_LAYER_EXPECT_ORDER:
	{
		// aiArgs[0] is an INDEX here, not an id — this verb addresses a POSITION
		// in the blend order, which is the whole thing it exists to assert.
		const u_int uIndex = static_cast<u_int>(xAction.m_aiArgs[0] < 0 ? 0 : xAction.m_aiArgs[0]);
		u_int uIdAt = uFLUX_INVALID_LAYER_ID;
		std::string strActual;
		const bool bResolved = xPanel.Document().GetLayerIdAt(uIndex, uIdAt)
			&& xPanel.Document().GetLayerName(uIdAt, strActual);
		Zenith_Assert(bResolved && strActual == xAction.m_szArg1,
			"EditorAutomation AnimLayerExpectOrder: blend-order index %u expected '%s', found '%s'",
			uIndex, xAction.m_szArg1.c_str(), bResolved ? strActual.c_str() : "<no layer at that index>");
		(void)bResolved;
		break;
	}

	default:
		Zenith_Assert(false, "Non-layer action routed to ExecuteAnimLayerAction");
		break;
	}
}

//-----------------------------------------------------------------------------
// BLEND-TREE authoring (WU-7.3): ANIM_BLEND_SET_TREE_KIND ..
// ANIM_BLEND_EXPECT_POINT_POSITION. Every case ends in one of
// Zenith_EditorPanel_AnimStateMachine's blend Action_* twins — the SAME call the
// "Blend Tree" strip's own handler makes — so a recipe and a human's gesture run
// one code path, and nothing here reaches past the panel into the document
// except to READ.
//-----------------------------------------------------------------------------
namespace
{
	void AnimBlendActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation blend-tree step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}

	// A point INDEX out of a step's int. Negative folds to the never-valid
	// sentinel so the checked wrapper reports it, rather than wrapping to a huge
	// unsigned that a bounds test would refuse for the wrong stated reason.
	u_int AnimBlendIndexFromAction(const Zenith_EditorAction& xAction)
	{
		return xAction.m_aiArgs[0] < 0 ? uINVALID_ANIMSM_BLEND_POINT : static_cast<u_int>(xAction.m_aiArgs[0]);
	}
}

static void ExecuteAnimBlendAction(const Zenith_EditorAction& xAction)
{
	Zenith_EditorPanel_AnimStateMachine& xPanel = Zenith_EditorPanel_AnimStateMachine::Instance();
	const u_int uIndex = AnimBlendIndexFromAction(xAction);

	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::ANIM_BLEND_SET_TREE_KIND:
	{
		// ★ THE INT IS AN OFFSET FROM SINGLE_CLIP, matching the strip's combo, so
		// there is one mapping rather than two that can drift. Anything outside
		// 0..2 is left as it arrives and the document refuses it — an out-of-range
		// kind is an authoring typo and has to fail at the step, not be rounded
		// into a conversion nobody asked for.
		const Zenith_AnimCtrlStateTreeKind eKind = static_cast<Zenith_AnimCtrlStateTreeKind>(
			static_cast<u_int>(ZENITH_ANIMCTRL_TREE_SINGLE_CLIP) + static_cast<u_int>(xAction.m_aiArgs[0] < 0 ? 99 : xAction.m_aiArgs[0]));
		AnimBlendActionChecked(xPanel.Action_SetStateTreeKind(xAction.m_szArg1, eKind),
			"AnimBlendSetTreeKind", xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::ANIM_BLEND_SET_PARAMETER:
		AnimBlendActionChecked(xPanel.Action_SetBlendSpaceParameter(xAction.m_szArg1,
			xAction.m_aiArgs[0] == 1 ? ZENITH_ANIMCTRL_BLEND_AXIS_Y : ZENITH_ANIMCTRL_BLEND_AXIS_X,
			xAction.m_szArg2),
			"AnimBlendSetParameter", xAction.m_szArg2.c_str());
		break;

	case Zenith_EditorActionType::ANIM_BLEND_ADD_POINT:
		AnimBlendActionChecked(xPanel.Action_AddBlendPoint(xAction.m_szArg1, xAction.m_szArg2,
			xAction.m_afArgs[0], xAction.m_afArgs[1]),
			"AnimBlendAddPoint", xAction.m_szArg2.c_str());
		break;

	case Zenith_EditorActionType::ANIM_BLEND_REMOVE_POINT:
		AnimBlendActionChecked(xPanel.Action_RemoveBlendPoint(xAction.m_szArg1, uIndex),
			"AnimBlendRemovePoint", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_BLEND_SET_POINT_CLIP:
		AnimBlendActionChecked(xPanel.Action_SetBlendPointClip(xAction.m_szArg1, uIndex, xAction.m_szArg2),
			"AnimBlendSetPointClip", xAction.m_szArg2.c_str());
		break;

	case Zenith_EditorActionType::ANIM_BLEND_SET_POINT_POSITION:
		AnimBlendActionChecked(xPanel.Action_SetBlendPointPosition(xAction.m_szArg1, uIndex,
			xAction.m_afArgs[0], xAction.m_afArgs[1]),
			"AnimBlendSetPointPosition", xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ANIM_BLEND_SELECT_POINT:
		// -1 is the CLEAR, and AnimBlendIndexFromAction has already folded it onto
		// the sentinel the panel reads as one.
		AnimBlendActionChecked(xPanel.Action_SelectBlendPoint(uIndex), "AnimBlendSelectPoint", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_BLEND_EXPECT_POINT_COUNT:
	{
		const u_int uActual = xPanel.Document().GetBlendPointCount(xAction.m_szArg1);
		Zenith_Assert(uActual == static_cast<u_int>(xAction.m_aiArgs[0] < 0 ? 0 : xAction.m_aiArgs[0]),
			"EditorAutomation AnimBlendExpectPointCount: '%s' expected %d points, found %u",
			xAction.m_szArg1.c_str(), xAction.m_aiArgs[0], uActual);
		(void)uActual;
		break;
	}

	case Zenith_EditorActionType::ANIM_BLEND_EXPECT_POINT_POSITION:
	{
		std::string strClip;
		Zenith_Maths::Vector2 xPosition(0.0f);
		const bool bResolved = xPanel.Document().GetBlendPoint(xAction.m_szArg1, uIndex, strClip, xPosition);
		const float fTolerance = xAction.m_afArgs[2] > 0.0f ? xAction.m_afArgs[2] : 1.0e-4f;
		Zenith_Assert(bResolved
			&& std::fabs(xPosition.x - xAction.m_afArgs[0]) <= fTolerance
			&& std::fabs(xPosition.y - xAction.m_afArgs[1]) <= fTolerance,
			"EditorAutomation AnimBlendExpectPointPosition: '%s' point %u expected (%f, %f), found (%f, %f)",
			xAction.m_szArg1.c_str(), uIndex, xAction.m_afArgs[0], xAction.m_afArgs[1],
			xPosition.x, xPosition.y);
		(void)bResolved; (void)fTolerance;
		break;
	}

	default:
		Zenith_Assert(false, "Non-blend-tree action routed to ExecuteAnimBlendAction");
		break;
	}
}

//-----------------------------------------------------------------------------
// CURVE-EDITOR authoring (WU-8.2): ANIM_CURVE_SET_VIEW ..
// ANIM_CURVE_EXPECT_KEY_TANGENT. Every case ends in one of
// Zenith_EditorPanel_Animation's curve Action_* twins — the SAME call the curve
// view's own pointer handler makes — so a recipe and a human's gesture run one
// code path, and nothing here reaches past the panel into the document except to
// READ.
//-----------------------------------------------------------------------------
namespace
{
	void AnimCurveActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation curve step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}

	// ★ A BONE NAME IS REQUIRED, unlike the ANIM_* family where an empty one means
	// root motion. Root motion carries no tangent array (D17), so an empty name
	// here can only be a typo — and building a root-motion track id out of it
	// would push the failure one layer down, where the message says "the action
	// refused" instead of "you named no bone".
	Zenith_AnimTrackId AnimCurveTrackFromAction(const Zenith_EditorAction& xAction)
	{
		return Zenith_AnimTrackId::Bone(xAction.m_szArg1,
			static_cast<Flux_AnimTrack>(xAction.m_aiArgs[0]));
	}
}

static void ExecuteAnimCurveAction(const Zenith_EditorAction& xAction)
{
	Zenith_EditorPanel_Animation& xPanel = Zenith_EditorPanel_Animation::Instance();

	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::ANIM_CURVE_SET_VIEW:
		AnimCurveActionChecked(xPanel.Action_SetCurveView(xAction.m_bArg), "AnimCurveSetView", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_CURVE_SET_UNIFIED:
		AnimCurveActionChecked(xPanel.Action_SetTangentsUnified(xAction.m_bArg), "AnimCurveSetUnified", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_CURVE_SET_KEY_TANGENTS:
	{
		const Zenith_AnimTrackId xTrack = AnimCurveTrackFromAction(xAction);
		const u_int uKeyId = AnimResolveKeyId(xPanel.Document(), xTrack, xAction.m_aiArgs[1]);
		AnimCurveActionChecked(uKeyId != uINVALID_ANIM_KEY_ID && xPanel.Action_SetKeyTangents(xTrack, uKeyId,
			Zenith_Maths::Vector3(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]),
			Zenith_Maths::Vector3(xAction.m_afArgs[3], xAction.m_afArgs[4], xAction.m_afArgs[5])),
			"AnimCurveSetKeyTangents", xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::ANIM_CURVE_SET_SELECTION_AUTO:
		AnimCurveActionChecked(xPanel.Action_SetSelectionTangentsAuto(), "AnimCurveSetSelectionAuto", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_CURVE_SET_SELECTION_LINEAR:
		AnimCurveActionChecked(xPanel.Action_SetSelectionTangentsLinear(), "AnimCurveSetSelectionLinear", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_CURVE_DRAG_HANDLE_TO_PIXEL:
	{
		const Zenith_AnimTrackId xTrack = AnimCurveTrackFromAction(xAction);
		const u_int uKeyId = AnimResolveKeyId(xPanel.Document(), xTrack, xAction.m_aiArgs[1]);
		const u_int uComponent = xAction.m_aiArgs[2] < 0
			? uANIM_CURVE_COMPONENT_COUNT   // never valid, so the wrapper reports it
			: static_cast<u_int>(xAction.m_aiArgs[2]);
		AnimCurveActionChecked(uKeyId != uINVALID_ANIM_KEY_ID
			&& xPanel.Action_DragTangentHandleToPixel(xTrack, uKeyId, uComponent, xAction.m_bArg,
				xAction.m_afArgs[0], xAction.m_afArgs[1]),
			"AnimCurveDragHandleToPixel", xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::ANIM_CURVE_FIT_TO_SELECTION:
		AnimCurveActionChecked(xPanel.Action_FitCurveViewToSelection(), "AnimCurveFitToSelection", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_CURVE_EXPECT_KEY_TANGENT:
	{
		const Zenith_AnimTrackId xTrack = AnimCurveTrackFromAction(xAction);
		const u_int uKeyId = AnimResolveKeyId(xPanel.Document(), xTrack, xAction.m_aiArgs[1]);
		Flux_KeyTangents xTangents;
		const bool bResolved = uKeyId != uINVALID_ANIM_KEY_ID
			&& xPanel.Document().GetKeyTangents(xTrack, uKeyId, xTangents);
		const Zenith_Maths::Vector3& xActual = xAction.m_bArg ? xTangents.m_xInTangent : xTangents.m_xOutTangent;
		const float fTolerance = xAction.m_afArgs[6] > 0.0f ? xAction.m_afArgs[6] : 1.0e-4f;
		Zenith_Assert(bResolved
			&& std::fabs(xActual.x - xAction.m_afArgs[0]) <= fTolerance
			&& std::fabs(xActual.y - xAction.m_afArgs[1]) <= fTolerance
			&& std::fabs(xActual.z - xAction.m_afArgs[2]) <= fTolerance,
			"EditorAutomation AnimCurveExpectKeyTangent: '%s' key %d %s expected (%f, %f, %f), found (%f, %f, %f)",
			xAction.m_szArg1.c_str(), xAction.m_aiArgs[1], xAction.m_bArg ? "in" : "out",
			xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2],
			xActual.x, xActual.y, xActual.z);
		(void)bResolved; (void)fTolerance;
		break;
	}

	default:
		Zenith_Assert(false, "Non-curve action routed to ExecuteAnimCurveAction");
		break;
	}
}

//-----------------------------------------------------------------------------
// TANGENT-MODE authoring (B3): ANIM_TANGENT_SET_KEY_MODE ..
// ANIM_TANGENT_EXPECT_KEY_MODE. The two mutating cases end in
// Zenith_EditorPanel_Animation::Action_SetKeyTangentMode /
// Action_SetSelectionTangentMode — the SAME calls the curve toolbar's mode boxes
// make — so a recipe and a human's gesture run one code path.
//
// ★ NEITHER OF THEM GOES NEAR Action_SetKeyTangents OR SetKeyTangentsAuto. Those
// write BOTH ends, and a per-end verb that used one would silently rewrite the end
// the recipe did not name.
//-----------------------------------------------------------------------------
namespace
{
	void AnimTangentActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation tangent step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}

	// ★ VALIDATED, NOT CAST. An int outside the enum's range would otherwise reach
	// a switch as a value no case handles, and the failure would be a mode that was
	// never set with every gate green — so a bad number fails HERE, on the step
	// that carries it, naming what it was.
	bool AnimTangentEndFromInt(int iEnd, Zenith_AnimTangentEnd& eOut)
	{
		switch (iEnd)
		{
		case 0: eOut = ZENITH_ANIM_TANGENT_END_IN;   return true;
		case 1: eOut = ZENITH_ANIM_TANGENT_END_OUT;  return true;
		case 2: eOut = ZENITH_ANIM_TANGENT_END_BOTH; return true;
		default: break;
		}
		return false;
	}

	bool AnimTangentModeFromInt(int iMode, Flux_TangentMode& eOut)
	{
		if (iMode < 0 || iMode > static_cast<int>(uFLUX_TANGENT_MODE_MAX))
		{
			return false;
		}
		eOut = static_cast<Flux_TangentMode>(static_cast<uint8_t>(iMode));
		return true;
	}
}

static void ExecuteAnimTangentAction(const Zenith_EditorAction& xAction)
{
	Zenith_EditorPanel_Animation& xPanel = Zenith_EditorPanel_Animation::Instance();

	Zenith_AnimTangentEnd eEnd = ZENITH_ANIM_TANGENT_END_BOTH;
	Flux_TangentMode eMode = Flux_TangentMode::LINEAR;
	const bool bArgsValid = AnimTangentEndFromInt(xAction.m_aiArgs[2], eEnd)
		&& AnimTangentModeFromInt(xAction.m_aiArgs[3], eMode);
	if (!bArgsValid)
	{
		Zenith_Assert(false, "EditorAutomation tangent step: end %d / mode %d is not a legal pair "
			"(end 0 In, 1 Out, 2 Both; mode 0 Linear, 1 Flat, 2 Auto, 3 Custom)",
			xAction.m_aiArgs[2], xAction.m_aiArgs[3]);
		return;
	}

	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::ANIM_TANGENT_SET_KEY_MODE:
	{
		const Zenith_AnimTrackId xTrack = AnimCurveTrackFromAction(xAction);
		const u_int uKeyId = AnimResolveKeyId(xPanel.Document(), xTrack, xAction.m_aiArgs[1]);
		AnimTangentActionChecked(uKeyId != uINVALID_ANIM_KEY_ID
			&& xPanel.Action_SetKeyTangentMode(xTrack, uKeyId, eEnd, eMode),
			"AnimTangentSetKeyMode", xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::ANIM_TANGENT_SET_SELECTION_MODE:
		AnimTangentActionChecked(xPanel.Action_SetSelectionTangentMode(eEnd, eMode),
			"AnimTangentSetSelectionMode", nullptr);
		break;

	case Zenith_EditorActionType::ANIM_TANGENT_EXPECT_KEY_MODE:
	{
		const Zenith_AnimTrackId xTrack = AnimCurveTrackFromAction(xAction);
		const u_int uKeyId = AnimResolveKeyId(xPanel.Document(), xTrack, xAction.m_aiArgs[1]);
		Flux_TangentMode eActual = Flux_TangentMode::LINEAR;
		// ★ THROUGH THE PANEL'S OWN READ-BACK, which answers false for a key that
		// does not resolve, for a root-motion track, AND — with Both — for a key
		// whose two ends DISAGREE. All three are "there is no single mode here",
		// which is exactly what this expectation is denying.
		const bool bResolved = uKeyId != uINVALID_ANIM_KEY_ID
			&& xPanel.GetKeyTangentMode(xTrack, uKeyId, eEnd, eActual);
		Zenith_Assert(bResolved && eActual == eMode,
			"EditorAutomation AnimTangentExpectKeyMode: '%s' key %d end %d expected mode %d, found %d "
			"(resolved %d)",
			xAction.m_szArg1.c_str(), xAction.m_aiArgs[1], xAction.m_aiArgs[2], xAction.m_aiArgs[3],
			bResolved ? static_cast<int>(eActual) : -1, bResolved ? 1 : 0);
		(void)bResolved; (void)eActual;
		break;
	}

	default:
		Zenith_Assert(false, "Non-tangent action routed to ExecuteAnimTangentAction");
		break;
	}
}

//-----------------------------------------------------------------------------
// IK POSING (E1): ANIM_IK_BAKE_TO_TARGET, and nothing else yet. Its own
// sub-executor rather than a case in ExecuteAction's switch, for the reason the
// enum block is a block: the first second verb would otherwise have to promote
// one into the other, moving a boundary two units and a static_assert pin.
//
// The case ends in the panel's own Action_* twin — the SAME call the preview
// pane's IK target drag makes on release — so a recipe and a human's gesture run
// one code path, and nothing here reaches past the panel.
//-----------------------------------------------------------------------------
namespace
{
	void AnimIkActionChecked(bool bOk, const char* szAction, const char* szArg)
	{
		Zenith_Assert(bOk, "EditorAutomation IK step %s('%s') failed", szAction, szArg ? szArg : "");
		(void)bOk; (void)szAction; (void)szArg;
	}
}

static void ExecuteAnimIkAction(const Zenith_EditorAction& xAction)
{
	Zenith_EditorPanel_Animation& xPanel = Zenith_EditorPanel_Animation::Instance();

	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::ANIM_IK_BAKE_TO_TARGET:
		// ★ THE THREE FLOATS GO STRAIGHT THROUGH — no scale, no axis test, no
		// conversion. Unlike ANIM_POSE_ROTATE_SELECTED_BONE_WORLD, which has to
		// build a quaternion and therefore has to do it through the pinned
		// Zenith_Maths::Authoring* helpers, there is no arithmetic to pin here:
		// the recipe states a position and the solver is handed that position.
		AnimIkActionChecked(xPanel.Action_BakeIKForSelectedChain(
			Zenith_Maths::Vector3(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2])),
			"AnimBakeIK", nullptr);
		break;

	default:
		Zenith_Assert(false, "Non-IK action routed to ExecuteAnimIkAction");
		break;
	}
}

// Particle field edits (SET_PARTICLE_CONFIG .. SET_PARTICLE_EMITTING).
static void ExecuteParticleAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::SET_PARTICLE_CONFIG:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_PARTICLE_CONFIG");
		Zenith_Assert(xEntity.HasComponent<Zenith_ParticleEmitterComponent>(), "Selected entity has no ParticleEmitterComponent");
		xEntity.GetComponent<Zenith_ParticleEmitterComponent>().SetConfig(
			static_cast<Flux_ParticleEmitterConfig*>(xAction.m_pArg));
		break;
	}

	case Zenith_EditorActionType::SET_PARTICLE_CONFIG_BY_NAME:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_PARTICLE_CONFIG_BY_NAME");
		Zenith_Assert(xEntity.HasComponent<Zenith_ParticleEmitterComponent>(), "Selected entity has no ParticleEmitterComponent");
		Flux_ParticleEmitterConfig* pxConfig = Flux_ParticleEmitterConfig::Find(xAction.m_szArg1.c_str());
		Zenith_Assert(pxConfig, "Particle config not found: %s", xAction.m_szArg1.c_str());
		xEntity.GetComponent<Zenith_ParticleEmitterComponent>().SetConfig(pxConfig);
		break;
	}

	case Zenith_EditorActionType::SET_PARTICLE_EMITTING:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_PARTICLE_EMITTING");
		Zenith_Assert(xEntity.HasComponent<Zenith_ParticleEmitterComponent>(), "Selected entity has no ParticleEmitterComponent");
		xEntity.GetComponent<Zenith_ParticleEmitterComponent>().SetEmitting(xAction.m_bArg);
		break;
	}

	default:
		Zenith_Assert(false, "Non-particle action routed to ExecuteParticleAction");
		break;
	}
}

// Collider + model actions (ADD_COLLIDER_SHAPE .. SET_MODEL_MATERIAL).
static void ExecuteColliderModelAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::ADD_COLLIDER_SHAPE:
	case Zenith_EditorActionType::ADD_CAPSULE_COLLIDER:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("collider-shape action");
		Zenith_Assert(xEntity.HasComponent<Zenith_ColliderComponent>(), "Selected entity has no ColliderComponent");
		Zenith_ColliderComponent& xCollider = xEntity.GetComponent<Zenith_ColliderComponent>();
		if (xAction.m_eType == Zenith_EditorActionType::ADD_CAPSULE_COLLIDER)
		{
			// Explicit capsule dimensions (radius, cylinder half-height).
			xCollider.AddCapsuleCollider(xAction.m_afArgs[0], xAction.m_afArgs[1],
				static_cast<RigidBodyType>(xAction.m_aiArgs[0]));
		}
		else
		{
			xCollider.AddCollider(static_cast<CollisionVolumeType>(xAction.m_aiArgs[0]),
				static_cast<RigidBodyType>(xAction.m_aiArgs[1]));
		}
		break;
	}

	case Zenith_EditorActionType::ADD_MESH_ENTRY:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("ADD_MESH_ENTRY");
		Zenith_Assert(xEntity.HasComponent<Zenith_ModelComponent>(), "Selected entity has no ModelComponent");
		Flux_MeshGeometry* pxGeometry = static_cast<Flux_MeshGeometry*>(xAction.m_pArg);
		Zenith_MaterialAsset* pxMaterial = static_cast<Zenith_MaterialAsset*>(xAction.m_pArg2);
		Zenith_Assert(pxGeometry, "Null geometry for ADD_MESH_ENTRY");
		Zenith_Assert(pxMaterial, "Null material for ADD_MESH_ENTRY");
		xEntity.GetComponent<Zenith_ModelComponent>().AddMeshEntry(*pxGeometry, *pxMaterial);
		break;
	}

	case Zenith_EditorActionType::LOAD_MODEL:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("LOAD_MODEL");
		Zenith_Assert(xEntity.HasComponent<Zenith_ModelComponent>(), "Selected entity has no ModelComponent");
		Zenith_Assert(!xAction.m_szArg1.empty(), "Null path for LOAD_MODEL");
		xEntity.GetComponent<Zenith_ModelComponent>().LoadModel(xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::SET_MODEL_MATERIAL:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_MODEL_MATERIAL");
		Zenith_Assert(xEntity.HasComponent<Zenith_ModelComponent>(), "Selected entity has no ModelComponent");
		Zenith_ModelComponent& xModel = xEntity.GetComponent<Zenith_ModelComponent>();
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
				static_cast<u_int>(xEntity.GetEntityID().m_uIndex));
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

	default:
		Zenith_Assert(false, "Non-collider/model action routed to ExecuteColliderModelAction");
		break;
	}
}

// Terrain material field edits (SET_TERRAIN_MATERIAL, SET_TERRAIN_SPLATMAP_PATH).
// Distinct from the TERRAIN_EDITOR_* range above, which edits the terrain editor
// session rather than a Zenith_TerrainComponent's material slots.
static void ExecuteTerrainMaterialAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::SET_TERRAIN_MATERIAL:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_TERRAIN_MATERIAL");
		Zenith_Assert(xEntity.HasComponent<Zenith_TerrainComponent>(), "Selected entity has no TerrainComponent");
		const int iSlot = xAction.m_aiArgs[0];
		Zenith_MaterialAsset* pxMaterial = static_cast<Zenith_MaterialAsset*>(xAction.m_pArg);
		Zenith_Assert(iSlot >= 0 && iSlot < static_cast<int>(Zenith_TerrainComponent::TERRAIN_MATERIAL_COUNT),
			"SET_TERRAIN_MATERIAL slot %d out of range [0, %u)", iSlot, Zenith_TerrainComponent::TERRAIN_MATERIAL_COUNT);
		Zenith_Assert(pxMaterial, "Null material for SET_TERRAIN_MATERIAL");
		xEntity.GetComponent<Zenith_TerrainComponent>().GetMaterialHandle(static_cast<u_int>(iSlot)).Set(pxMaterial);
		break;
	}

	case Zenith_EditorActionType::SET_TERRAIN_SPLATMAP_PATH:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_TERRAIN_SPLATMAP_PATH");
		Zenith_Assert(xEntity.HasComponent<Zenith_TerrainComponent>(), "Selected entity has no TerrainComponent");
		Zenith_Assert(!xAction.m_szArg1.empty(), "Null path for SET_TERRAIN_SPLATMAP_PATH");
		xEntity.GetComponent<Zenith_TerrainComponent>().GetSplatmapHandle().SetPath(xAction.m_szArg1.c_str());
		break;
	}

	default:
		Zenith_Assert(false, "Non-terrain-material action routed to ExecuteTerrainMaterialAction");
		break;
	}
}

// Prefab variant authoring (CREATE_PREFAB_FROM_SELECTED .. INSTANTIATE_PREFAB).
static void ExecutePrefabAction(const Zenith_EditorAction& xAction)
{
	switch (xAction.m_eType)
	{
	case Zenith_EditorActionType::CREATE_PREFAB_FROM_SELECTED:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("CREATE_PREFAB_FROM_SELECTED");
		Zenith_Assert(!xAction.m_szArg1.empty(), "Null prefab name for CREATE_PREFAB_FROM_SELECTED");
		Zenith_Assert(!xAction.m_szArg2.empty(), "Null save path for CREATE_PREFAB_FROM_SELECTED");

		Zenith_Prefab xPrefab;
		const bool bCreated = xPrefab.CreateFromEntity(xEntity, xAction.m_szArg1.c_str());
		Zenith_Assert(bCreated, "CreateFromEntity failed for '%s'", xAction.m_szArg1.c_str());
		const bool bSaved = xPrefab.SaveToFile(xAction.m_szArg2.c_str());
		Zenith_Assert(bSaved, "SaveToFile failed for '%s'", xAction.m_szArg2.c_str());

		// Force-cache through the registry so subsequent steps that look the
		// path up via PrefabHandle resolve cheaply (no disk re-read on every
		// CreateAsVariant cycle check).
		Zenith_AssetRegistry::GetView<Zenith_Prefab>(xAction.m_szArg2.c_str());
		break;
	}

	case Zenith_EditorActionType::CREATE_PREFAB_VARIANT:
	{
		Zenith_Assert(!xAction.m_szArg1.empty(), "Null variant name for CREATE_PREFAB_VARIANT");
		Zenith_Assert(!xAction.m_szArg2.empty(), "Null base path for CREATE_PREFAB_VARIANT");
		const char* szSavePath = xAction.m_szArg3.c_str();
		Zenith_Assert(!xAction.m_szArg3.empty(), "Null save path for CREATE_PREFAB_VARIANT");

		// Make sure the base prefab is loaded so PrefabHandle's cycle check
		// can resolve it. The cycle detector deliberately does NOT trigger a
		// disk load (see Zenith_Prefab::WouldFormVariantCycle) — we have to
		// prime the registry here.
		Zenith_AssetRegistry::GetView<Zenith_Prefab>(xAction.m_szArg2.c_str());

		PrefabHandle xBaseHandle(xAction.m_szArg2.c_str());
		Zenith_Prefab xVariant;
		const bool bCreated = xVariant.CreateAsVariant(xBaseHandle, xAction.m_szArg1.c_str());
		Zenith_Assert(bCreated, "CreateAsVariant failed for '%s' (base '%s')",
			xAction.m_szArg1.c_str(), xAction.m_szArg2.c_str());
		const bool bSaved = xVariant.SaveToFile(szSavePath);
		Zenith_Assert(bSaved, "SaveToFile failed for variant '%s' at '%s'",
			xAction.m_szArg1.c_str(), szSavePath);

		Zenith_AssetRegistry::GetView<Zenith_Prefab>(szSavePath);
		break;
	}

	case Zenith_EditorActionType::ADD_PREFAB_VARIANT_OVERRIDE_VEC3:
	{
		Zenith_Assert(!xAction.m_szArg1.empty(), "Null prefab path for ADD_PREFAB_VARIANT_OVERRIDE_VEC3");
		Zenith_Assert(!xAction.m_szArg2.empty(), "Null component name for ADD_PREFAB_VARIANT_OVERRIDE_VEC3");
		const char* szPropertyName = xAction.m_szArg3.c_str();
		Zenith_Assert(!xAction.m_szArg3.empty(), "Null property name for ADD_PREFAB_VARIANT_OVERRIDE_VEC3");

		// Modifying a prefab held by the registry is safe because Zenith_Prefab*
		// is the same pointer the registry caches — adding an override mutates
		// in-memory state, then SaveToFile rewrites the .zpfb. Callers that load
		// the file again get the updated overrides.
		Zenith_Prefab* pxPrefab = Zenith_AssetRegistry::GetView<Zenith_Prefab>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxPrefab, "Could not load prefab '%s' for override", xAction.m_szArg1.c_str());

		Zenith_PropertyOverride xOv;
		xOv.m_strComponentName = xAction.m_szArg2.c_str();
		xOv.m_strPropertyPath  = szPropertyName;
		xOv.m_xValue << Zenith_Maths::Vector3(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]);
		pxPrefab->AddOverride(std::move(xOv));

		const bool bSaved = pxPrefab->SaveToFile(xAction.m_szArg1.c_str());
		Zenith_Assert(bSaved, "SaveToFile failed after AddOverride for '%s'", xAction.m_szArg1.c_str());
		break;
	}

	case Zenith_EditorActionType::INSTANTIATE_PREFAB:
	{
		Zenith_Assert(!xAction.m_szArg1.empty(), "Null prefab path for INSTANTIATE_PREFAB");

		Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_Assert(xActiveScene.IsValid(), "INSTANTIATE_PREFAB requires an active scene");
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
		Zenith_Assert(pxSceneData, "Active scene data was null in INSTANTIATE_PREFAB");

		Zenith_Prefab* pxPrefab = Zenith_AssetRegistry::GetView<Zenith_Prefab>(xAction.m_szArg1.c_str());
		Zenith_Assert(pxPrefab, "Could not load prefab '%s' for instantiation", xAction.m_szArg1.c_str());

		const char* szEntityName = xAction.m_szArg2.c_str();
		// Transform payload: pos[0..2], quat[3..6] (wxyz), scale[7..9].
		const Zenith_Maths::Vector3 xPos(xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2]);
		const Zenith_Maths::Quat    xRot(xAction.m_afArgs[3], xAction.m_afArgs[4], xAction.m_afArgs[5], xAction.m_afArgs[6]);
		const Zenith_Maths::Vector3 xScale(xAction.m_afArgs[7], xAction.m_afArgs[8], xAction.m_afArgs[9]);
		Zenith_Entity xEntity = pxPrefab->Instantiate(pxSceneData, szEntityName, xPos, xRot, xScale);
		Zenith_Assert(xEntity.IsValid(), "Instantiate returned invalid entity for '%s'", xAction.m_szArg1.c_str());

		// Mirror the editor's normal selection behaviour after entity creation
		// so subsequent transform/component steps target the new entity.
		g_xEngine.Editor().SelectEntity(xEntity.GetEntityID());
		break;
	}

	default:
		Zenith_Assert(false, "Non-prefab action routed to ExecutePrefabAction");
		break;
	}
}

void Zenith_EditorAutomation::ExecuteAction(const Zenith_EditorAction& xAction)
{
	Zenith_Editor& xEditor = g_xEngine.Editor();

	// Terrain-editor authoring actions have their own executor (see above).
	bool bTerrainActionSucceeded = false;
	if (TryRouteTerrainEditorAction(xAction, g_xEngine.TerrainEditor(),
		TerrainRectExecutionMode::Production, bTerrainActionSucceeded))
	{
		(void)bTerrainActionSucceeded;
		return;
	}

	// UI authoring actions likewise have their own executor (see below).
	if (xAction.m_eType >= Zenith_EditorActionType::CREATE_UI_TEXT &&
		xAction.m_eType <= Zenith_EditorActionType::SET_UI_VIRTUAL_BUTTON_HIT_SLOP)
	{
		ExecuteUIAction(xAction);
		return;
	}

	// Material editor authoring actions have their own executor too.
	if (xAction.m_eType >= Zenith_EditorActionType::MATERIAL_CREATE &&
		xAction.m_eType <= Zenith_EditorActionType::MATERIAL_SAVE)
	{
		ExecuteMaterialAction(xAction);
		return;
	}

	// Grass-type authoring actions edit the terrain editor's working table.
	if (xAction.m_eType >= Zenith_EditorActionType::GRASS_TYPES_CREATE &&
		xAction.m_eType <= Zenith_EditorActionType::GRASS_TYPES_SAVE)
	{
		ExecuteGrassTypeAction(xAction, g_xEngine.TerrainEditor());
		return;
	}

	// Remaining field-edit ranges, same contiguous-range-router pattern.
	if (xAction.m_eType >= Zenith_EditorActionType::SET_CAMERA_POSITION &&
		xAction.m_eType <= Zenith_EditorActionType::SET_MAIN_CAMERA)
	{
		ExecuteCameraAction(xAction);
		return;
	}

	if (xAction.m_eType >= Zenith_EditorActionType::SET_TRANSFORM_POSITION &&
		xAction.m_eType <= Zenith_EditorActionType::SET_TRANSFORM_ROTATION_QUAT)
	{
		ExecuteTransformAction(xAction);
		return;
	}

	if (xAction.m_eType >= Zenith_EditorActionType::SET_LIGHT_INTENSITY &&
		xAction.m_eType <= Zenith_EditorActionType::SET_SUN_TIME_OF_DAY)
	{
		ExecuteLightAction(xAction);
		return;
	}

	// Enum order is OPEN_FRESH..CLOSE then BUILD tacked on after (see header) --
	// the range end is GRAPH_BUILD, not GRAPH_CLOSE, so the whole contiguous
	// block routes here.
	if (xAction.m_eType >= Zenith_EditorActionType::GRAPH_OPEN_FRESH &&
		xAction.m_eType <= Zenith_EditorActionType::GRAPH_BUILD)
	{
		ExecuteGraphAuthoringAction(xAction);
		return;
	}

	if (xAction.m_eType >= Zenith_EditorActionType::SET_PARTICLE_CONFIG &&
		xAction.m_eType <= Zenith_EditorActionType::SET_PARTICLE_EMITTING)
	{
		ExecuteParticleAction(xAction);
		return;
	}

	if (xAction.m_eType >= Zenith_EditorActionType::ADD_COLLIDER_SHAPE &&
		xAction.m_eType <= Zenith_EditorActionType::SET_MODEL_MATERIAL)
	{
		ExecuteColliderModelAction(xAction);
		return;
	}

	if (xAction.m_eType >= Zenith_EditorActionType::SET_TERRAIN_MATERIAL &&
		xAction.m_eType <= Zenith_EditorActionType::SET_TERRAIN_SPLATMAP_PATH)
	{
		ExecuteTerrainMaterialAction(xAction);
		return;
	}

	if (xAction.m_eType >= Zenith_EditorActionType::CREATE_PREFAB_FROM_SELECTED &&
		xAction.m_eType <= Zenith_EditorActionType::INSTANTIATE_PREFAB)
	{
		ExecutePrefabAction(xAction);
		return;
	}

	// Animation dope-sheet authoring (WU-3.4). The upper bound is the block's
	// LAST member and the header static_asserts the block's width against it, so
	// a verb appended without moving this line fails the build rather than
	// falling through to the generic switch's assert at boot.
	if (xAction.m_eType >= Zenith_EditorActionType::ANIM_OPEN_CLIP &&
		xAction.m_eType <= Zenith_EditorActionType::ANIM_EXPECT_SELECTED_COUNT)
	{
		ExecuteAnimationAction(xAction);
		return;
	}

	// Animation POSE authoring (WU-4.3). A SECOND range immediately after the one
	// above rather than four more members of it: appending into that block would
	// move ANIM_EXPECT_SELECTED_COUNT, which is the bound the line above and the
	// header's static_assert both compare against.
	if (xAction.m_eType >= Zenith_EditorActionType::ANIM_POSE_SELECT_BONE &&
		xAction.m_eType <= Zenith_EditorActionType::ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION)
	{
		ExecuteAnimationPoseAction(xAction);
		return;
	}

	// Animator-controller STATE MACHINE authoring (WU-6.5). A THIRD animation
	// range, for the reason the second one exists: appending into the block above
	// would move the bound both that line and the header's static_assert compare
	// against.
	if (xAction.m_eType >= Zenith_EditorActionType::ANIM_SM_OPEN &&
		xAction.m_eType <= Zenith_EditorActionType::ANIM_SM_EXPECT_DEFAULT_STATE)
	{
		ExecuteAnimStateMachineAction(xAction);
		return;
	}

	// Bone-mask authoring (WU-7.1). A FOURTH animation range, for the reason the
	// third exists: appending into the block above would move the bound both that
	// line and the header's static_assert compare against.
	if (xAction.m_eType >= Zenith_EditorActionType::ANIM_MASK_OPEN &&
		xAction.m_eType <= Zenith_EditorActionType::ANIM_MASK_EXPECT_WEIGHT)
	{
		ExecuteAnimMaskAction(xAction);
		return;
	}

	// Animator-LAYER authoring (WU-7.2). A FIFTH animation range, for the reason
	// the fourth exists: appending into the block above would move the bound both
	// that line and the header's static_assert compare against.
	if (xAction.m_eType >= Zenith_EditorActionType::ANIM_LAYER_ADD &&
		xAction.m_eType <= Zenith_EditorActionType::ANIM_LAYER_EXPECT_ORDER)
	{
		ExecuteAnimLayerAction(xAction);
		return;
	}

	// Blend-tree authoring (WU-7.3). A SIXTH animation range, for the reason the
	// fifth exists: appending into the block above would move the bound both that
	// line and the header's static_assert compare against.
	if (xAction.m_eType >= Zenith_EditorActionType::ANIM_BLEND_SET_TREE_KIND &&
		xAction.m_eType <= Zenith_EditorActionType::ANIM_BLEND_EXPECT_POINT_POSITION)
	{
		ExecuteAnimBlendAction(xAction);
		return;
	}

	// Curve-editor authoring (WU-8.2). A SEVENTH animation range, for the reason
	// the sixth exists: appending into the block above would move the bound both
	// that line and the header's static_assert compare against.
	if (xAction.m_eType >= Zenith_EditorActionType::ANIM_CURVE_SET_VIEW &&
		xAction.m_eType <= Zenith_EditorActionType::ANIM_CURVE_EXPECT_KEY_TANGENT)
	{
		ExecuteAnimCurveAction(xAction);
		return;
	}

	// Tangent-MODE authoring (B3). An EIGHTH animation range, for the reason the
	// seventh exists: appending into the block above would move the bound both that
	// line and the header's static_assert compare against.
	if (xAction.m_eType >= Zenith_EditorActionType::ANIM_TANGENT_SET_KEY_MODE &&
		xAction.m_eType <= Zenith_EditorActionType::ANIM_TANGENT_EXPECT_KEY_MODE)
	{
		ExecuteAnimTangentAction(xAction);
		return;
	}

	// IK posing (E1). An EIGHTH animation range, for the reason the seventh
	// exists. Written as a RANGE although it is one member wide today: a second
	// IK verb then appends for free, where a `case` in the switch below would
	// have to be promoted to a range and would move a boundary two units pin.
	if (xAction.m_eType >= Zenith_EditorActionType::ANIM_IK_BAKE_TO_TARGET &&
		xAction.m_eType <= Zenith_EditorActionType::ANIM_IK_BAKE_TO_TARGET)
	{
		ExecuteAnimIkAction(xAction);
		return;
	}

	switch (xAction.m_eType)
	{
	//--------------------------------------------------------------------------
	// Scene operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_SCENE:
		xEditor.CreateNewScene(xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::SAVE_SCENE:
		xEditor.SaveActiveScene(xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::UNLOAD_SCENE:
		g_xEngine.Editor().UnloadActiveScene();
		break;

	//--------------------------------------------------------------------------
	// Entity operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::CREATE_ENTITY:
		g_xEngine.Editor().CreateEntity(xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::SELECT_ENTITY:
		g_xEngine.Editor().SelectEntityByName(xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::SET_ENTITY_TRANSIENT:
		g_xEngine.Editor().SetSelectedEntityTransient(xAction.m_bArg);
		break;

	//--------------------------------------------------------------------------
	// Component operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::ADD_COMPONENT:
		g_xEngine.Editor().AddComponentToSelected(xAction.m_szArg1.c_str());
		break;

	case Zenith_EditorActionType::ATTACH_TO_BONE:
	{
		Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxEntity, "No entity selected for ATTACH_TO_BONE");
		Zenith_SceneData* pxSceneData = pxEntity->GetSceneData();
		Zenith_Assert(pxSceneData, "ATTACH_TO_BONE: selected entity has no scene");
		// Resolve the skeleton target by name within the same scene (authored earlier
		// in the step list).
		Zenith_Entity xTarget = pxSceneData->FindEntityByName(xAction.m_szArg1.c_str());
		Zenith_Assert(xTarget.IsValid(), "ATTACH_TO_BONE: target entity not found by name");
		if (!pxEntity->HasComponent<Zenith_AttachmentComponent>())
		{
			pxEntity->AddComponent<Zenith_AttachmentComponent>();
		}
		const Zenith_Maths::Matrix4 xOffset = BuildEulerOffsetMatrix(
			xAction.m_afArgs[0], xAction.m_afArgs[1], xAction.m_afArgs[2],
			xAction.m_afArgs[3], xAction.m_afArgs[4], xAction.m_afArgs[5]);
		pxEntity->GetComponent<Zenith_AttachmentComponent>().AttachToBone(
			xTarget, xAction.m_szArg2.c_str(), xOffset);
		break;
	}

	//--------------------------------------------------------------------------
	// Script operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::ATTACH_GRAPH:  g_xEngine.Editor().AttachGraphToSelected(xAction.m_szArg1.c_str()); break;

	//--------------------------------------------------------------------------
	// NavMesh
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::SET_NAVMESH_ASSET:
	{
		Zenith_Entity& xEntity = GetSelectedEntityChecked("SET_NAVMESH_ASSET");
		Zenith_Assert(xEntity.HasComponent<Zenith_NavMeshComponent>(),
			"SET_NAVMESH_ASSET: selected entity has no NavMeshComponent "
			"(AddStep_AddComponent(\"NavMesh\") first)");
		Zenith_Assert(!xAction.m_szArg1.empty(), "SET_NAVMESH_ASSET: empty asset ref");
		// SetAssetRef LOADS as well as storing, and returns whether a mesh is
		// live afterwards. A failure is NOT asserted here: the component records
		// it as NAVMESH_LOAD_STATE_FAILED plus a human-readable reason its panel
		// shows, and Zenith_NavMesh::LoadFromFile has already asserted on the
		// real cause -- a second assert here would only obscure it.
		xEntity.GetComponent<Zenith_NavMeshComponent>().SetAssetRef(xAction.m_szArg1);
		break;
	}

	//--------------------------------------------------------------------------
	// Scene loading operations
	//--------------------------------------------------------------------------
	case Zenith_EditorActionType::LOAD_INITIAL_SCENE:
	{
		Zenith_Assert(xAction.m_pfnFunc, "Null function pointer for LOAD_INITIAL_SCENE");
		// Invoke the load callback under a lifecycle deferral guard so
		// DispatchFullLifecycleInit owns Awake/OnEnable order.
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
