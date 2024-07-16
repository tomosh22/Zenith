#include "Zenith.h"

#include "Input/Zenith_Input.h"
#include "Zenith_OS_Include.h"

void Zenith_Input::KeyPressedCallback(uint32_t uKeyCode)
{

}

void Zenith_Input::MouseButtonPressedCallback(uint32_t uKeyCode)
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
