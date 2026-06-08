#pragma once

#include "Maths/Zenith_Maths.h"

// ============================================================================
// Low-layer editor-query boundary (the g_xGizmoTransformAccess seam pattern).
//
// EntityComponent (L2) and UI (L3) tools-gated code needs a handful of read-only
// editor queries plus two commands, but must NOT include Editor/Zenith_Editor.h
// (L4) — that is a layer-up violation. The editor publishes this neutral
// function-pointer table; the lower layers call through it instead of reaching
// g_xEngine.Editor() directly.
//
// Returns are bool / Maths types only, so the EditorMode enum (defined in
// Editor.h) never has to move down a layer. The table is defined + filled on the
// editor side (Editor/Zenith_EditorQuery.cpp, #ifdef ZENITH_TOOLS). Every caller
// is itself #ifdef ZENITH_TOOLS, so in non-tools builds g_xEditorQuery is simply
// never referenced (the extern needs no definition there).
// ============================================================================

class Zenith_MaterialAsset;

struct Zenith_EditorQueryAccessor
{
	bool                  (*m_pfnIsEditorStopped)()         = nullptr; // EditorMode == Stopped
	bool                  (*m_pfnIsEditorPlaying)()         = nullptr; // EditorMode == Playing
	Zenith_Maths::Vector2 (*m_pfnGetViewportPos)()          = nullptr;
	Zenith_Maths::Vector2 (*m_pfnGetViewportSize)()         = nullptr;
	void                  (*m_pfnSetSelectedAsMainCamera)() = nullptr;
	void                  (*m_pfnSelectMaterial)(Zenith_MaterialAsset* pxMaterial) = nullptr;
};

extern Zenith_EditorQueryAccessor g_xEditorQuery;
