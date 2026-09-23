#pragma once

// Scenario 8 (leaking header): LibB_Api.h is owned by LibB, but includes
// LibA's entry header and uses LibA's types three ways -- embed by value,
// embed via a template argument, and a bare pointer (ref). It also derives
// from a LibA type (embed, base class). A leaf-header consumer (LibA) is
// leaked onward to anyone who reaches LibB through this header.
#include "LibA/LibA.h"

// Scenario 10: the parent-include-stack-only case.
#include "detail/LibB_Inner.h"

#include <vector>

class LibB_Widget : public LibA_Thing
{
public:
	LibA_Data m_xData;                 // embed: by-value member
	std::vector<LibA_Data> m_axItems;  // embed: template argument by value
	LibA_Thing* m_pxThing = nullptr;   // ref: pointer only
};
