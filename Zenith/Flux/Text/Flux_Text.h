#pragma once

// Character width as fraction of height (typical monospace ratio is ~0.5-0.6)
// Must also match CHAR_ASPECT_RATIO in Flux_Text.vert
static constexpr float fCHAR_ASPECT_RATIO = 0.5f;

// Character spacing includes a small gap (10% of char width) for natural appearance
static constexpr float fCHAR_SPACING = fCHAR_ASPECT_RATIO * 1.1f;

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