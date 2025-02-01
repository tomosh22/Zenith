#include "Zenith.h"

#include "Input/Zenith_Input.h"
#include "Zenith_OS_Include.h"

static std::unordered_set<Zenith_KeyCode> s_xFrameKeyPresses;

void Zenith_Input::BeginFrame()
{
	s_xFrameKeyPresses.clear();
}

void Zenith_Input::KeyPressedCallback(Zenith_KeyCode iKey)
{
	s_xFrameKeyPresses.insert(iKey);
}

void Zenith_Input::MouseButtonPressedCallback(Zenith_KeyCode iKey)
{
}

void Zenith_Input::GetMousePosition(Zenith_Maths::Vector2_64& xOut)
{
	Zenith_Window::GetInstance()->GetMousePosition(xOut);
}

bool Zenith_Input::IsKeyDown(Zenith_KeyCode iKey)
{
	return Zenith_Window::GetInstance()->IsKeyDown(iKey);
}

bool Zenith_Input::WasKeyPressedThisFrame(Zenith_KeyCode iKey)
{
	return s_xFrameKeyPresses.find(iKey) != s_xFrameKeyPresses.end();
}
