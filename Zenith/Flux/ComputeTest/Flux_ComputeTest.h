#pragma once

#include "Flux/Flux.h"

class Flux_ComputeTest
{
public:
	static void Initialise();
	static void Run();
	static Flux_RenderAttachment& GetComputeOutputTexture();

private:
	static void RunComputePass();
	static void RunDisplayPass();
};
