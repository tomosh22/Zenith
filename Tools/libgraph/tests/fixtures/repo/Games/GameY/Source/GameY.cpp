// Scenario 17: GameY reaches LibA ONLY via the leaking header, from a .cpp
// only (no GameY header includes it).
#include "FxPch.h"

#include "Shared/LibB_Api.h"

int main()
{
	LibB_Widget xWidget;
	(void)xWidget;
	return 0;
}
