#pragma once

class Flux_Text
{
public:
	static void Initialise();
	static void Shutdown();

	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();

	static uint32_t UploadChars();
};