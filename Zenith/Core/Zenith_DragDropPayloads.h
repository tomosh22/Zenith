#pragma once

// Drag-drop payload type identifiers (max 32 chars per ImGui) and the file-path
// payload POD. Lifted out of Editor/Zenith_Editor.h into this dependency-free L0
// header so non-editor TUs (e.g. ScriptComponent's drag-drop target) can consume
// them without pulling the full editor header. Editor.h includes this header so
// every existing editor-side user keeps getting these names transitively.

// Drag-drop payload type identifiers (max 32 chars per ImGui)
#define DRAGDROP_PAYLOAD_TEXTURE  "ZENITH_TEXTURE"
#define DRAGDROP_PAYLOAD_MESH    "ZENITH_MESH"
#define DRAGDROP_PAYLOAD_MATERIAL "ZENITH_MATERIAL"
#define DRAGDROP_PAYLOAD_PREFAB   "ZENITH_PREFAB"
#define DRAGDROP_PAYLOAD_MODEL    "ZENITH_MODEL"
#define DRAGDROP_PAYLOAD_ANIMATION "ZENITH_ANIMATION"
#define DRAGDROP_PAYLOAD_SCRIPT_ASSET "ZSCRIPT_ASSET"
#define DRAGDROP_PAYLOAD_FILE_GENERIC "ZENITH_FILE_GENERIC"

// Drag-drop payload data structure
struct DragDropFilePayload
{
	char m_szFilePath[512];          // Absolute path to file
};
