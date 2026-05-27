#include "Zenith.h"

#include "EntityComponent/Zenith_SceneSystemGuards.h"
#include "EntityComponent/Internal/Zenith_SceneLifecycleScheduler.h"
#include "Core/Zenith_Engine.h"

//=============================================================================
// Zenith_SceneSystemGuards — RAII guard implementations.
//
// All guards mutate state on Zenith_SceneLifecycleScheduler (reached via
// g_xEngine.SceneLifecycle()).
//=============================================================================

Zenith_PrefabInstantiationGuard::Zenith_PrefabInstantiationGuard()
	: m_bPrevValue(g_xEngine.SceneLifecycle().m_bIsPrefabInstantiating)
{
	g_xEngine.SceneLifecycle().m_bIsPrefabInstantiating = true;
}

Zenith_PrefabInstantiationGuard::~Zenith_PrefabInstantiationGuard()
{
	g_xEngine.SceneLifecycle().m_bIsPrefabInstantiating = m_bPrevValue;
}

Zenith_SceneUpdateDeferralGuard::Zenith_SceneUpdateDeferralGuard()
	: m_bPrevValue(g_xEngine.SceneLifecycle().m_bIsUpdating)
{
	g_xEngine.SceneLifecycle().m_bIsUpdating = true;
}

Zenith_SceneUpdateDeferralGuard::~Zenith_SceneUpdateDeferralGuard()
{
	g_xEngine.SceneLifecycle().m_bIsUpdating = m_bPrevValue;
}

Zenith_SceneCreationTargetScope::Zenith_SceneCreationTargetScope(Zenith_Scene xScene)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Zenith_SceneCreationTargetScope must be constructed on the main thread");
	g_xEngine.SceneLifecycle().m_axCreationTargetStack.PushBack(xScene);
}

Zenith_SceneCreationTargetScope::~Zenith_SceneCreationTargetScope()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Zenith_SceneCreationTargetScope must be destroyed on the main thread");
	Zenith_Assert(g_xEngine.SceneLifecycle().m_axCreationTargetStack.GetSize() > 0,
		"Zenith_SceneCreationTargetScope: creation-target stack underflow on destruction");
	g_xEngine.SceneLifecycle().m_axCreationTargetStack.PopBack();
}
