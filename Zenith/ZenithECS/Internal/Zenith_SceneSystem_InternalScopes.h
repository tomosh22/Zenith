#pragma once

//==============================================================================
// Zenith_SceneSystem_InternalScopes.h -- RAII scope types used ONLY by the
// Zenith_SceneSystem Internal/ TUs.
//
// Zenith_SceneCreationTargetScope was relocated out of the public
// Zenith_SceneSystem.h: no file outside Zenith/ZenithECS/ names it
// (grep-verified), so it does not belong on the public scene-API surface. It
// reaches the private Zenith_SceneSystem state through Zenith_SceneSystem::Get()
// in its body (defined in Internal/Zenith_SceneSystem_Lifecycle.cpp); it remains
// a friend of Zenith_SceneSystem (the friend declaration lives in
// Zenith_SceneSystem.h and doubles as a forward declaration).
//
// Zenith_SceneSystem.h provides Zenith_Scene (the only type this scope names)
// plus the matching friend declaration, so it is included here.
//==============================================================================

#include "ZenithECS/Zenith_SceneSystem.h"

struct Zenith_SceneCreationTargetScope
{
	explicit Zenith_SceneCreationTargetScope(Zenith_Scene xScene);
	~Zenith_SceneCreationTargetScope();
	Zenith_SceneCreationTargetScope(const Zenith_SceneCreationTargetScope&) = delete;
	Zenith_SceneCreationTargetScope& operator=(const Zenith_SceneCreationTargetScope&) = delete;
};
