#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "UnitTests/Zenith_MockInput.h"

bool Zenith_MockInput::s_bMockingEnabled = false;
std::unordered_set<Zenith_KeyCode> Zenith_MockInput::s_xMockedHeldKeys;
std::unordered_set<Zenith_KeyCode> Zenith_MockInput::s_xMockedFrameKeyPresses;
Zenith_Maths::Vector2_64 Zenith_MockInput::s_xMockedMousePosition = { 0.0, 0.0 };

void Zenith_MockInput::EnableMocking(bool bEnable)
{
	s_bMockingEnabled = bEnable;
	if (bEnable)
	{
		Reset();
	}
}

bool Zenith_MockInput::IsMockingEnabled()
{
	return s_bMockingEnabled;
}

void Zenith_MockInput::SimulateKeyPress(Zenith_KeyCode eKey)
{
	s_xMockedFrameKeyPresses.insert(eKey);
	s_xMockedHeldKeys.insert(eKey);
}

void Zenith_MockInput::SimulateMousePress(Zenith_KeyCode eMouseButton)
{
	s_xMockedFrameKeyPresses.insert(eMouseButton);
	s_xMockedHeldKeys.insert(eMouseButton);
}

void Zenith_MockInput::SetMousePosition(const Zenith_Maths::Vector2_64& xPos)
{
	s_xMockedMousePosition = xPos;
}

void Zenith_MockInput::SetKeyHeld(Zenith_KeyCode eKey, bool bHeld)
{
	if (bHeld)
	{
		s_xMockedHeldKeys.insert(eKey);
	}
	else
	{
		s_xMockedHeldKeys.erase(eKey);
	}
}

void Zenith_MockInput::SetKeysHeld(const std::unordered_set<Zenith_KeyCode>& xKeys)
{
	s_xMockedHeldKeys = xKeys;
}

void Zenith_MockInput::ClearHeldKeys()
{
	s_xMockedHeldKeys.clear();
}

bool Zenith_MockInput::IsKeyHeldMocked(Zenith_KeyCode eKey)
{
	return s_xMockedHeldKeys.find(eKey) != s_xMockedHeldKeys.end();
}

bool Zenith_MockInput::WasKeyPressedThisFrameMocked(Zenith_KeyCode eKey)
{
	return s_xMockedFrameKeyPresses.find(eKey) != s_xMockedFrameKeyPresses.end();
}

void Zenith_MockInput::GetMousePositionMocked(Zenith_Maths::Vector2_64& xOut)
{
	xOut = s_xMockedMousePosition;
}

void Zenith_MockInput::BeginTestFrame()
{
	s_xMockedFrameKeyPresses.clear();
}

void Zenith_MockInput::EndTestFrame()
{
	s_xMockedFrameKeyPresses.clear();
}

void Zenith_MockInput::Reset()
{
	s_xMockedHeldKeys.clear();
	s_xMockedFrameKeyPresses.clear();
	s_xMockedMousePosition = { 0.0, 0.0 };
}

#endif // ZENITH_TOOLS
