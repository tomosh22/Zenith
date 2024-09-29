#pragma once
//#TO just for MAX_FRAMES_IN_FLIGHT
#include "Flux/Flux_Enums.h"
#include "Flux/Flux.h"
class Zenith_State
{
public:
	Zenith_State() = default;

	virtual void OnEnter() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnExit() = 0;
};

class Zenith_StateMachine
{
public:
	static void Update()
	{
		if (s_pxRequestedState)
		{
			//#TO_TODO: extremely lazy solution to destroying gpu resources that are still in flight
			for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
			{
				if (!Flux_Swapchain::BeginFrame())
				{
					Flux_MemoryManager::EndFrame(false);
					continue;
				}
				Flux_PlatformAPI::BeginFrame();
				Flux_Swapchain::CopyToFramebuffer();
				Flux_PlatformAPI::EndFrame();
				Flux_Swapchain::EndFrame();
			}
			s_pxCurrentState->OnExit();
			delete s_pxCurrentState;
			s_pxCurrentState = s_pxRequestedState;
			s_pxCurrentState->OnEnter();
			s_pxRequestedState = nullptr;
		}
		s_pxCurrentState->OnUpdate();
	}
	static void RequestState(Zenith_State* const pxNewState)
	{
		s_pxRequestedState = pxNewState;
	}
	static Zenith_State* s_pxCurrentState;
	static inline Zenith_State* s_pxRequestedState = nullptr;
};