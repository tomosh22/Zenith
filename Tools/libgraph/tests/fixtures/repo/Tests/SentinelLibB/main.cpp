// Scenario 22: SentinelLibB -- the fixture's leaf-proof executable, mirroring
// Tests/Sentinel*/main.cpp. Its .cs declares LibB, LibA, MidLib in that
// order (subject = LibB), plus a RenderBackend.Vulkan-conditional edge to
// LibC that stays unresolved for the fixture's backend-prefix-less "fx_*"
// configs.
#include "LibA/LibA.h"
#include "Shared/LibB_Api.h"

int main()
{
	LibB_Widget xWidget;
	(void)xWidget;
	LibA_Init();
	return 0;
}
