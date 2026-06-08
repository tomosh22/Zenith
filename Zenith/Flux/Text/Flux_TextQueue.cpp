#include "Zenith.h"

#include "Flux/Text/Flux_TextQueue.h"

namespace Flux
{
	// Per-frame pending text-render queue. Relocated from Zenith_UICanvas.cpp so
	// the Flux Text subsystem owns its own queue (no UI layer-up edge).
	static Zenith_Vector<Flux_TextEntry> s_xPending;

	Zenith_Vector<Flux_TextEntry>& Flux_TextQueue::GetPending()
	{
		return s_xPending;
	}

	void Flux_TextQueue::ClearPending()
	{
		s_xPending.Clear();
	}

	void Flux_TextQueue::Submit(const Flux_TextEntry& xEntry)
	{
		s_xPending.PushBack(xEntry);
	}
}
