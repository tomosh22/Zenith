#pragma once

// Scenario 7: single-consumer header. Included from outside LibA only by
// Zenith/Shared/LibC_Impl.cpp (see scenario 11).
struct LibA_SoloThing
{
	int m_iValue = 0;
};
