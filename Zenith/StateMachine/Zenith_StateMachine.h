#pragma once
#include "EntityComponent/Zenith_Scene.h"
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
	static void Project_Initialise();
	static void Update()
	{
		if (s_pxRequestedState)
		{
			Zenith_Scene::GetCurrentScene().Reset();
			s_pxCurrentState->OnExit();
			delete s_pxCurrentState;
			Zenith_Vulkan::GetDevice().waitIdle();
			s_pxCurrentState = s_pxRequestedState;
			Flux_MemoryManager::BeginFrame();
			s_pxCurrentState->OnEnter();
			Flux_MemoryManager::EndFrame(false);
			s_pxRequestedState = nullptr;
			return;
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