#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_ComponentRegistry.h"

//==============================================================================
// Component Registry Auto-Registration
//==============================================================================
// Components automatically register with the editor via ZENITH_REGISTER_COMPONENT.
// The macro registers with ComponentMetaRegistry, which also registers with
// ComponentRegistry in ZENITH_TOOLS builds.
//
// This ensures:
// 1. Single registration point - ZENITH_REGISTER_COMPONENT handles everything
// 2. No manual registration list to maintain
// 3. All components with the macro appear in "Add Component" menu
//==============================================================================

#endif // ZENITH_TOOLS
