#pragma once

#ifdef ZENITH_TOOLS

#include "EntityComponent/Zenith_Entity.h"

//=============================================================================
// Properties Panel
//
// Displays and allows editing of the selected entity's properties:
// - Entity name
// - Component properties (via registry)
// - Add component functionality
//=============================================================================

namespace Zenith_EditorPanelProperties
{
	/**
	 * Render the properties panel
	 *
	 * @param pxSelectedEntity Pointer to the selected entity (may be null)
	 * @param uPrimarySelectedEntityID The ID of the primary selected entity
	 */
	void Render(Zenith_Entity* pxSelectedEntity, Zenith_EntityID uPrimarySelectedEntityID);
}

#endif // ZENITH_TOOLS
