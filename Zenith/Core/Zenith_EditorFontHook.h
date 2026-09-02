#pragma once

// ============================================================================
// Zenith_EditorFontHook
//
// The editor owns the ImGui font set (Editor/Zenith_EditorUI.cpp), but fonts
// must be registered with the atlas BEFORE a renderer backend builds it:
//   - the Vulkan backend builds lazily (ImGuiBackendFlags_RendererHasTextures),
//     so any time before the first NewFrame is fine;
//   - the Null backend has NO renderer backend and rasterises the atlas itself
//     during InitialiseImGui (GetTexDataAsRGBA32), after which the legacy path
//     locks the atlas — a font added later would never be baked.
// Both backends therefore call this right after ImGui::CreateContext, from a
// TOOLS-only block. It is a plain declaration here (Core, layer 0) with its
// definition in the Editor module — the same shape as Zenith_EditorAddLogMessage
// in Zenith.h — so neither backend TU includes an editor header.
// ============================================================================

#ifdef ZENITH_TOOLS
void Zenith_EditorFonts_Load();
#endif
