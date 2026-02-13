#pragma once

class Flux_SSAO
{
public:
	static bool s_bEnabled;

	static void Initialise();

	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();
};