#pragma once

// Scenario 6: exposed-only header -- reachable from outside LibA only
// through LibA.h (which includes this), never included directly by anyone
// outside LibA.
struct LibA_Math_Vec3
{
	float m_fX = 0.0f;
	float m_fY = 0.0f;
	float m_fZ = 0.0f;
};
