#pragma once

class Flux_Text
{
public:
	static void Initialise();

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();

	static uint32_t UploadChars();
};