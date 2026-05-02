#pragma once

#ifdef ZENITH_TOOLS

#include <string>

class Zenith_Prefab;

//=============================================================================
// Variant Editor Panel
//
// Inspects and authors prefab variants. A variant is a derived prefab that
// inherits from a base + applies a list of property overrides at instantiation
// time (see Prefab/CLAUDE.md and EntityComponent/Zenith_ComponentMeta.h).
//
// The panel is intentionally compact: it covers the core authoring loop
// (pick a base prefab, name a variant, add Transform Position/Scale overrides,
// save to disk) without trying to be a full property editor for every type.
// Editing complex overrides (Light, Animator) is out of scope for this first
// pass; the underlying SetComponentProperty API supports them and a richer UI
// can layer on top.
//
// Workflow:
//   1. Drop a .zpfb file from Content Browser onto the "Base prefab" target,
//      or paste its path manually.
//   2. Type a name for the new variant in "Variant name".
//   3. Click "Create variant". The variant lives in memory at this point.
//   4. Use the override editor to add Position / Scale overrides with
//      Vector3 inputs.
//   5. Click "Save" to write the variant to disk as a .zpfb.
//
// Existing variants on disk can be loaded for inspection by dropping their
// path onto the "Inspect" target — read-only display of the override list.
//=============================================================================

namespace Zenith_EditorPanelVariantEditor
{
	void Render();

	void SetVisible(bool bVisible);
	bool IsVisible();
}

#endif // ZENITH_TOOLS
