#pragma once

#ifdef ZENITH_TOOLS
#if ZENITH_MEMORY_TRACKING_FULL

#include "Collections/Zenith_Vector.h"

struct Zenith_AllocationRecord;

//=============================================================================
// Memory Debug Panel
//
// Displays memory statistics, allocation list, and budget configuration.
//=============================================================================

namespace Zenith_EditorPanelMemory
{
	// Render the memory debug panel
	void Render();

	// Show/hide the panel
	void SetVisible(bool bVisible);
	bool IsVisible();

	//-------------------------------------------------------------------------
	// Private helpers for the Allocation tab. Declared here so the dispatcher
	// (RenderAllocationTab) can delegate to focused per-section functions
	// instead of carrying the entire tab's logic inline.
	//-------------------------------------------------------------------------
	namespace Private
	{
		// Draws the filter text input and Clear button.
		void RenderAllocationFilters();

		// Collects live allocation records from the tracker, applies the
		// current filter text, and sorts them according to the active sort
		// column / direction. Output is written into axRecords.
		void CollectAndSortAllocations(Zenith_Vector<const Zenith_AllocationRecord*>& axRecords);

		// Renders the scrollable allocation table, handling sort-spec changes
		// and row selection.
		void RenderAllocationTable(const Zenith_Vector<const Zenith_AllocationRecord*>& axRecords);

		// If a row is selected, renders the callstack for that allocation.
		void RenderAllocationCallstack(const Zenith_Vector<const Zenith_AllocationRecord*>& axRecords);
	}
}

#endif // ZENITH_MEMORY_TRACKING_FULL
#endif // ZENITH_TOOLS
