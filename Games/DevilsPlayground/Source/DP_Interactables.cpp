#include "Zenith.h"

#include "DP_Interactables.h"

namespace DP_Interactables
{
	void MarkAsInteractable(Zenith_EntityID /*xId*/, Kind /*eKind*/, void* /*pUserData*/)
	{
		// W0 stub. B3 wires this into DPInteractable_Behaviour during script
		// attach; this entry point is reserved for non-behaviour entities (props
		// the editor can flag as interactable).
	}
}
