#include "Zenith.h"

#include "Zenith_OS_Include.h"
#include "Flux/Zenith_Flux.h"

int main()
{
	Zenith_Window::Inititalise("Zenith", 1280, 720);
	Zenith_Flux::EarlyInitialise();
	Zenith_Flux::LateInitialise();
	__debugbreak();
}