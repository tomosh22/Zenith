#pragma once

// Scenario 15: only ever reached through a conditional (#ifdef FX_TOOLS)
// include, from Zenith/Shared/LibB_Impl.cpp.
struct LibA_ExtraThing
{
	int m_iValue = 0;
};
