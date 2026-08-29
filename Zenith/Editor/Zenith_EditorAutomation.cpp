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
