#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"

// Forward declarations
class Flux_MaterialAsset;

//=============================================================================
// Material Editor Panel
//
// Material property editing with texture drag-drop support.
//=============================================================================

// Material editor state structure
struct MaterialEditorState
{
	Flux_MaterialAsset*& m_pxSelectedMaterial;
	bool& m_bShowMaterialEditor;
};

namespace Zenith_EditorPanelMaterialEditor
{
	/**
	 * Render the material editor panel
	 *
	 * @param xState Reference to material editor state
	 */
	void Render(MaterialEditorState& xState);

	/**
	 * Render a texture slot with drag-drop support
	 *
	 * @param szLabel Label for the texture slot
	 * @param pMaterial Material to modify
	 * @param strCurrentPath Current texture path
	 * @param SetPathFunc Function to set the new texture path
	 */
	void RenderMaterialTextureSlot(const char* szLabel, Flux_MaterialAsset* pMaterial,
		const std::string& strCurrentPath,
		void (*SetPathFunc)(Flux_MaterialAsset*, const std::string&));
}

#endif // ZENITH_TOOLS
