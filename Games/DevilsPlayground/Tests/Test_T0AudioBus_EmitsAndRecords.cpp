#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_AudioBus.h"

#include <cmath>
#include <cstring>

// ============================================================================
// Test_T0AudioBus_EmitsAndRecords (MVP-0.4.1)
//
// Tier-0 harness smoke test for Zenith_AudioBus. Verifies the recording API:
//
//   1. ClearEmittedSoundsForTest() empties the buffer.
//   2. EmitSound() appends an entry with name / position / loudness / radius
//      faithfully recorded.
//   3. Multiple EmitSound calls accumulate.
//   4. AdvanceFrameForTest() increments the m_uFrame stamp on the next emit.
//
// Pattern: pure cache exercise -- no scene load, no entity spawn. Runs in
// headless CI without graphics.
// ============================================================================

namespace
{
	int g_iFailures = 0;
}

static void Setup_T0AudioBus_EmitsAndRecords()
{
	g_iFailures = 0;
	Zenith_AudioBus::ClearEmittedSoundsForTest();
}

static bool Step_T0AudioBus_EmitsAndRecords(int iFrame)
{
	(void)iFrame;
	return false;
}

static bool FloatEq(float a, float b)
{
	return std::fabs(a - b) < 0.001f;
}

static bool Verify_T0AudioBus_EmitsAndRecords()
{
	g_iFailures = 0;

	// 1) Buffer starts empty after Clear.
	if (Zenith_AudioBus::GetEmittedSoundsForTest().GetSize() != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0AudioBus: buffer not empty after Clear (size=%u)",
			Zenith_AudioBus::GetEmittedSoundsForTest().GetSize());
		++g_iFailures;
	}

	// 2) Single emission round-trips faithfully.
	Zenith_AudioBus::EmitSound("Test.Forge.Hammer",
		Zenith_Maths::Vector3(10.0f, 0.0f, -5.0f),
		1.0f,  // loudness
		30.0f  // radius (matches Test_P2Forge_AudibleAt30m contract)
	);
	const auto& xSounds = Zenith_AudioBus::GetEmittedSoundsForTest();
	if (xSounds.GetSize() != 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0AudioBus: expected 1 emission, got %u", xSounds.GetSize());
		++g_iFailures;
	}
	else
	{
		const Zenith_AudioBus::EmittedSound& xS = xSounds.Get(0);
		if (xS.m_szName == nullptr || std::strcmp(xS.m_szName, "Test.Forge.Hammer") != 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_T0AudioBus: name mismatch ('%s')",
				xS.m_szName ? xS.m_szName : "(null)");
			++g_iFailures;
		}
		if (!FloatEq(xS.m_xPosition.x, 10.0f) ||
		    !FloatEq(xS.m_xPosition.y,  0.0f) ||
		    !FloatEq(xS.m_xPosition.z, -5.0f))
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_T0AudioBus: position mismatch (got %f,%f,%f)",
				xS.m_xPosition.x, xS.m_xPosition.y, xS.m_xPosition.z);
			++g_iFailures;
		}
		if (!FloatEq(xS.m_fLoudness, 1.0f))
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_T0AudioBus: loudness mismatch (got %f)", xS.m_fLoudness);
			++g_iFailures;
		}
		if (!FloatEq(xS.m_fRadius, 30.0f))
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_T0AudioBus: radius mismatch (got %f)", xS.m_fRadius);
			++g_iFailures;
		}
	}

	// 3) Multiple emissions accumulate.
	const u_int uFrameStamp = xSounds.GetSize() > 0 ? xSounds.Get(0).m_uFrame : 0;
	Zenith_AudioBus::EmitSound("Test.Chest.Open",
		Zenith_Maths::Vector3(0.0f), 0.5f, 20.0f);
	Zenith_AudioBus::EmitSound("Test.NoiseMachine",
		Zenith_Maths::Vector3(0.0f), 1.0f, 25.0f);
	const auto& xSoundsAfter = Zenith_AudioBus::GetEmittedSoundsForTest();
	if (xSoundsAfter.GetSize() != 3)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0AudioBus: expected 3 emissions, got %u",
			xSoundsAfter.GetSize());
		++g_iFailures;
	}

	// 4) AdvanceFrameForTest() bumps the frame stamp on the next emission.
	Zenith_AudioBus::AdvanceFrameForTest();
	Zenith_AudioBus::EmitSound("Test.NextFrame",
		Zenith_Maths::Vector3(0.0f), 0.1f, 5.0f);
	const auto& xSoundsFinal = Zenith_AudioBus::GetEmittedSoundsForTest();
	if (xSoundsFinal.GetSize() < 4)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0AudioBus: expected at least 4 emissions, got %u",
			xSoundsFinal.GetSize());
		++g_iFailures;
	}
	else
	{
		const Zenith_AudioBus::EmittedSound& xLatest = xSoundsFinal.Get(xSoundsFinal.GetSize() - 1);
		if (xLatest.m_uFrame <= uFrameStamp)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_T0AudioBus: frame stamp didn't advance (was %u, now %u)",
				uFrameStamp, xLatest.m_uFrame);
			++g_iFailures;
		}
	}

	// Tidy up so we don't pollute subsequent batch tests.
	Zenith_AudioBus::ClearEmittedSoundsForTest();

	return g_iFailures == 0;
}

static const Zenith_AutomatedTest g_xAudioBusEmitsTest = {
	"Test_T0AudioBus_EmitsAndRecords",
	&Setup_T0AudioBus_EmitsAndRecords,
	&Step_T0AudioBus_EmitsAndRecords,
	&Verify_T0AudioBus_EmitsAndRecords,
	10,
	// m_bRequiresGraphics: false -- pure engine-API exercise, no scene/entity.
	false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAudioBusEmitsTest);

#endif // ZENITH_INPUT_SIMULATOR
