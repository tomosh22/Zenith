#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Core/Zenith_EditorQuery.h"
#include "Editor/Zenith_Editor.h"

// Editor-side definition + install of the g_xEditorQuery facade (Decision 1A).
// This is the ONE legitimate editor-side seam that reaches g_xEngine.Editor() on
// behalf of the lower (EC/UI) layers; it carries a single, cached g_xEngine reach
// (Ed()), allow-listed in engine_singleton_allowlist.txt. The six EC/UI files that
// used to reach g_xEngine.Editor() directly (a layer-up include of Editor.h) now
// call through g_xEditorQuery, dropping both the Editor.h include and their reaches.
namespace
{
	// Single cached reach for the whole table; the forwarders route through it.
	static Zenith_Editor& Ed() { return g_xEngine.Editor(); }

	static bool                  EQ_IsStopped()               { return Ed().GetEditorMode() == EditorMode::Stopped; }
	static bool                  EQ_IsPlaying()               { return Ed().GetEditorMode() == EditorMode::Playing; }
	static Zenith_Maths::Vector2 EQ_ViewportPos()             { return Ed().GetViewportPos(); }
	static Zenith_Maths::Vector2 EQ_ViewportSize()            { return Ed().GetViewportSize(); }
	static void                  EQ_SetSelectedAsMainCamera() { Ed().SetSelectedAsMainCamera(); }
	static void                  EQ_SelectMaterial(Zenith_MaterialAsset* pxMaterial) { Ed().SelectMaterial(pxMaterial); }
}

Zenith_EditorQueryAccessor g_xEditorQuery =
{
	&EQ_IsStopped,
	&EQ_IsPlaying,
	&EQ_ViewportPos,
	&EQ_ViewportSize,
	&EQ_SetSelectedAsMainCamera,
	&EQ_SelectMaterial,
};

#endif // ZENITH_TOOLS
