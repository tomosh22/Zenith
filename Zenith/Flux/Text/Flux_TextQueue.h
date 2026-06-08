#pragma once

#include <string>
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

// Flux_TextQueue — the per-frame text-render queue, owned by the Flux Text
// subsystem (L2). Producers (UI canvas widgets at L3, Flux_HDR histogram labels
// at L2) push Flux_TextEntry records here; the Flux_Text drainer (L2) iterates
// and clears them each frame.
//
// This header is deliberately lightweight — it includes ONLY L0 headers
// (<string>, Maths, Collections) so that UI can include it downward (L3->L2)
// without pulling any render-backend headers. Do NOT include Flux_TextImpl.h or
// any other heavy Flux header here.

namespace Flux
{
	// One queued text-draw request. Plane bounds / glyph layout are resolved by
	// the drainer (ProcessTextEntry in Flux_Text.cpp).
	struct Flux_TextEntry
	{
		std::string m_strText;
		Zenith_Maths::Vector2 m_xPosition;
		float m_fSize;
		Zenith_Maths::Vector4 m_xColor;
		int m_iSortOrder = 0; // Owning element's sort order (for overlay clip partitioning)
	};

	// Static-class queue facade. The backing storage lives in Flux_TextQueue.cpp.
	struct Flux_TextQueue
	{
		// Mutable access to the pending entries (producers PushBack, the drainer iterates).
		static Zenith_Vector<Flux_TextEntry>& GetPending();

		// Drop all pending entries (called by the drainer after upload + on Reset).
		static void ClearPending();

		// Convenience producer helper — equivalent to GetPending().PushBack(xEntry).
		static void Submit(const Flux_TextEntry& xEntry);
	};
}
