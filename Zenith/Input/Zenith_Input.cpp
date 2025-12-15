#include "Zenith.h"

#include "Input/Zenith_Input.h"
#include "Zenith_OS_Include.h"

static std::unordered_set<Zenith_KeyCode> s_xFrameKeyPresses;
static Zenith_Maths::Vector2_64 s_xLastMousePosition = { 0.0, 0.0 };
static Zenith_Maths::Vector2_64 s_xMouseDelta = { 0.0, 0.0 };
static bool s_bFirstFrame = true;

void Zenith_Input::BeginFrame()
{
	s_xFrameKeyPresses.clear();

	// Calculate mouse delta
	Zenith_Maths::Vector2_64 xCurrentMousePos;
	Zenith_Window::GetInstance()->GetMousePosition(xCurrentMousePos);

	if (s_bFirstFrame)
	{
		s_xMouseDelta = { 0.0, 0.0 };
		s_bFirstFrame = false;
	}
	else
	{
		s_xMouseDelta.x = xCurrentMousePos.x - s_xLastMousePosition.x;
		s_xMouseDelta.y = xCurrentMousePos.y - s_xLastMousePosition.y;
	}

	s_xLastMousePosition = xCurrentMousePos;
}

void Zenith_Input::KeyPressedCallback(Zenith_KeyCode iKey)
{
	s_xFrameKeyPresses.insert(iKey);
}

void Zenith_Input::MouseButtonPressedCallback(Zenith_KeyCode iKey)
{
	s_xFrameKeyPresses.insert(iKey);
}

void Zenith_Input::GetMousePosition(Zenith_Maths::Vector2_64& xOut)
{
	Zenith_Window::GetInstance()->GetMousePosition(xOut);
}

void Zenith_Input::GetMouseDelta(Zenith_Maths::Vector2_64& xOut)
{
	xOut = s_xMouseDelta;
}

bool Zenith_Input::IsKeyDown(Zenith_KeyCode iKey)
{
	return Zenith_Window::GetInstance()->IsKeyDown(iKey);
}

bool Zenith_Input::WasKeyPressedThisFrame(Zenith_KeyCode iKey)
{
	return s_xFrameKeyPresses.find(iKey) != s_xFrameKeyPresses.end();
}
