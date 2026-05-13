#include "Zenith.h"

#include "Core/Zenith_AudioBus.h"

namespace
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Recording buffer + frame counter live in the anonymous namespace so
	// they aren't visible outside this TU. Single-threaded by contract --
	// EmitSound is main-thread-only (game OnUpdate / OnInteract). If a
	// future audio thread ever calls EmitSound, the buffer needs a mutex.
	Zenith_Vector<Zenith_AudioBus::EmittedSound> s_xEmittedSounds;
	u_int                                        s_uCurrentFrame = 0;
#endif
}

namespace Zenith_AudioBus
{
	void EmitSound(const char* szName,
	               const Zenith_Maths::Vector3& xPosition,
	               float fLoudness,
	               float fRadius)
	{
#ifdef ZENITH_INPUT_SIMULATOR
		EmittedSound xEntry;
		xEntry.m_szName    = szName;
		xEntry.m_xPosition = xPosition;
		xEntry.m_fLoudness = fLoudness;
		xEntry.m_fRadius   = fRadius;
		xEntry.m_uFrame    = s_uCurrentFrame;
		s_xEmittedSounds.PushBack(xEntry);
#else
		// Shipping builds: stub until the post-MVP audio playback layer
		// lands. Args silenced to avoid unused-parameter warnings.
		(void)szName;
		(void)xPosition;
		(void)fLoudness;
		(void)fRadius;
#endif
	}

#ifdef ZENITH_INPUT_SIMULATOR
	const Zenith_Vector<EmittedSound>& GetEmittedSoundsForTest()
	{
		return s_xEmittedSounds;
	}

	void ClearEmittedSoundsForTest()
	{
		s_xEmittedSounds.Clear();
	}

	void AdvanceFrameForTest()
	{
		++s_uCurrentFrame;
	}
#endif
}
