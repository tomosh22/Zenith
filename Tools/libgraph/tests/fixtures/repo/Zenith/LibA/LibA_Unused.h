#pragma once

// Scenario 4: an unused public header. Listed in LibA's vcxproj (a real
// member), but included by nobody -- outside LibA or in. A trim-hint
// candidate for later waves ("move to Internal/").
struct LibA_UnusedThing
{
	int m_iValue = 0;
};
