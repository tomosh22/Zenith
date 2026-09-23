#pragma once

// Scenario 10: exists ONLY at Zenith/Shared/LibB_Sibling.h, and
// "Zenith/Shared" is in no project's include dirs -- resolvable only via
// the parent-include stack (LibB_Api.h -> detail/LibB_Inner.h ->
// LibB_Sibling.h, each includer's own directory).
struct LibB_SiblingThing
{
	int m_iValue = 0;
};
