#pragma once

#include "StateMachine/Zenith_StateMachine.h"

class SuperSecret_State_InGame ZENITH_FINAL : public Zenith_State
{
	void OnEnter() override ZENITH_FINAL;
	void OnUpdate() override ZENITH_FINAL;
	void OnExit() override ZENITH_FINAL;
};