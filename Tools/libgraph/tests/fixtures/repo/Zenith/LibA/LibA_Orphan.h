#pragma once

// Scenario 19: an orphan. This file exists on disk under LibA's own
// directory, but no vcxproj lists it -- deliberately left out of
// Build/liba_win64.vcxproj's ItemGroup.
struct LibA_OrphanThing
{
	int m_iValue = 0;
};
