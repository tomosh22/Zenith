#pragma once

// Scenario 2: a pure internal header, included only by LibA's own files.
struct LibA_PrivateState
{
	int m_iCounter = 0;
};
