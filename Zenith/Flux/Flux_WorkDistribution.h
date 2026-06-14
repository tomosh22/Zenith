#pragma once

// Per-worker-thread command range distribution for parallel Vulkan command
// buffer recording. Split out of Flux.h.
#include "Core/ZenithConfig.h"   // FLUX_NUM_WORKER_THREADS

// Work distribution indices for parallel Vulkan command buffer recording
struct Flux_WorkDistribution
{
	u_int auStartIndex[FLUX_NUM_WORKER_THREADS];
	u_int auEndIndex[FLUX_NUM_WORKER_THREADS];
	u_int uTotalCommandCount;

	void Clear()
	{
		for (u_int i = 0; i < FLUX_NUM_WORKER_THREADS; i++)
		{
			auStartIndex[i] = 0;
			auEndIndex[i] = 0;
		}
		uTotalCommandCount = 0;
	}
};
