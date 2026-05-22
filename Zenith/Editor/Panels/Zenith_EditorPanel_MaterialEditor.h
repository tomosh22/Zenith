#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"

// Forward declarations
class Zenith_MaterialAsset;

//=============================================================================
// Material Editor Panel
//
// Material property editing with texture drag-drop support.
//=============================================================================

// Material editor state structure.
// m_pxSelectedMaterial is a non-owning view pointer -- the editor's owning
// MaterialHandle lives on Zenith_EditorImpl. The panel never mutates this
// field; it calls Zenith_Editor::SelectMaterial / ClearMaterialSelection
// to change the selection.
struct MaterialEditorState
{
	Zenith_MaterialAsset*  m_pxSelectedMaterial;
	bool&                  m_bShowMaterialEditor;
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
	void RenderMaterialTextureSlot(const char* szLabel, Zenith_MaterialAsset* pMaterial,
		const std::string& strCurrentPath,
		void (*SetPathFunc)(Zenith_MaterialAsset*, const std::string&));

	//-------------------------------------------------------------------------
	// Private rendering sections
	//-------------------------------------------------------------------------

	/**
	 * Render the top toolbar (Create New / Load Material buttons).
	 */
	void RenderToolbarSection();

	/**
	 * Render the Loaded Materials collapsing list.
	 */
	void RenderLoadedMaterialsSection();

	/**
	 * Render the selected material's header (name field + path / unsaved state).
	 *
	 * @param pMat The currently selected material (non-null).
	 */
	void RenderMaterialHeaderSection(Zenith_MaterialAsset* pMat);

	/**
	 * Render the material's editable properties and texture slots.
	 *
	 * @param pMat The currently selected material (non-null).
	 */
	void RenderPropertiesAndTexturesSection(Zenith_MaterialAsset* pMat);

	/**
	 * Render the Save / Save As / Reload buttons for the selected material.
	 *
	 * @param pMat The currently selected material (non-null).
	 */
	void RenderSaveLoadControlsSection(Zenith_MaterialAsset* pMat);

	/**
	 * Render the "no selection" placeholder text.
	 */
	void RenderNoSelectionSection();
}

#endif // ZENITH_TOOLS
