#pragma once

// Scenario 5: LibA's entry header. Declares the public surface consumers
// reach LibA through.
//
// Scenario 6: LibA_Math.h is an "exposed-only" header -- included by this
// entry header and by nothing outside LibA directly.
#include "LibA_Math.h"

// Scenario 21: an unlisted third-party header under Middleware/glmx/, which
// nobody's vcxproj lists -- synthesizes the plain "glmx" middleware node.
#include "glmx/glmx.hpp"

class LibA_Thing
{
public:
	void Tick();

private:
	LibA_Math_Vec3 m_xPosition{};
};

struct LibA_Data
{
	int m_iA = 0;
	int m_iB = 0;
};

// Scenario 5: LibA_Mode and LibA_Count must stay unused by every consumer
// outside LibA -- the "types nobody outside uses" case.
enum class LibA_Mode
{
	FX_MODE_A,
	FX_MODE_B,
};

using LibA_Handle = unsigned int;
typedef int LibA_Count;

void LibA_Init();
