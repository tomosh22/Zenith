#pragma once

#ifdef ZENITH_TOOLS

//------------------------------------------------------------------------------
// Open-Graph-Editor hook. Lets engine-side panels (Zenith_GraphComponent's
// properties section, the content browser glue) open the editor's Graph Editor
// panel without an EntityComponent -> Editor layer-up include. Constant-
// initialised to the panel's thunk in Zenith_EditorPanel_GraphEditor.cpp -
// referencing it from a consumer also pulls that TU in (no dead-strip hazard),
// the same pattern as g_pfnZenithLightGather.
//------------------------------------------------------------------------------
typedef void (*Zenith_OpenGraphEditorFn)(const char* szAssetPath);
extern Zenith_OpenGraphEditorFn g_pfnZenithOpenGraphEditor;

#endif // ZENITH_TOOLS
