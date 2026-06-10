#pragma once

#ifdef ZENITH_TOOLS

class Zenith_TerrainEditor;

//=============================================================================
// Terrain Editor panel — tool palette, brush settings, layers, procedural
// generation, erosion, auto-splat rules, and save/bake. All edits route
// through Zenith_TerrainEditor's scriptable API (the same entry points the
// editor automation uses).
//=============================================================================
namespace Zenith_EditorPanelTerrainEditor
{
	void Render(Zenith_TerrainEditor& xEditor, bool& bShowPanel);
}

#endif // ZENITH_TOOLS
