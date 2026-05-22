#pragma once

#ifdef ZENITH_TOOLS

#include "Collections/Zenith_Vector.h"
#include "Editor/Zenith_EditorAutomation.h"

// Phase 5.5d: per-Engine editor automation queue + run state.
class Zenith_EditorAutomationImpl
{
public:
	Zenith_EditorAutomationImpl() = default;
	~Zenith_EditorAutomationImpl() = default;

	Zenith_EditorAutomationImpl(const Zenith_EditorAutomationImpl&) = delete;
	Zenith_EditorAutomationImpl& operator=(const Zenith_EditorAutomationImpl&) = delete;

	Zenith_Vector<Zenith_EditorAction> m_axActions;
	uint32_t                           m_uCurrentAction = 0;
	bool                               m_bRunning       = false;
	bool                               m_bComplete      = false;
};

#endif // ZENITH_TOOLS
