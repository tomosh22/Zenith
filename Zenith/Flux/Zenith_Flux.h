#pragma once

#include "Zenith_PlatformGraphics_Include.h"

class Zenith_Flux
{
public:
	static void EarlyInitialise();
	static void LateInitialise();

	static const uint32_t GetFrameIndex() { return s_uFrameIndex; }
	static const uint32_t GetFrameCounter() { return s_uFrameCounter; }
	static void SubmitCommandBuffer(const Flux_CommandBuffer* pxCmd, RenderOrder eOrder)
	{
		for (const Flux_CommandBuffer* pxExistingCmd : s_xPendingCommandBuffers[eOrder])
		{
			Zenith_Assert(pxExistingCmd != pxCmd, "Command buffer has already been submitted");
		}
		s_xPendingCommandBuffers[eOrder].push_back(pxCmd);
	}
private:
	static uint32_t s_uFrameIndex;
	static uint32_t s_uFrameCounter;
	static std::vector<const Flux_CommandBuffer*> s_xPendingCommandBuffers[RENDER_ORDER_MAX];
};