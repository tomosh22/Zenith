#include "Zenith.h"
#include "Flux/Flux_Screenshot.h"

// Pending-request state lives in function-local statics (the sanctioned
// cross-TU singleton shape — see Core/CLAUDE.md's carve-out list; this is
// process-level test/diagnostic I/O, like the CLI screenshot path it extends).
// A request is set on the main thread (a test step) and consumed on the main
// thread (swapchain EndFrame, before present), so no synchronisation is needed.
namespace
{
	bool& PendingFlag()
	{
		static bool s_bPending = false;
		return s_bPending;
	}

	std::string& PendingPath()
	{
		static std::string s_strPath;
		return s_strPath;
	}
}

namespace Flux_Screenshot
{
	void RequestDump(const char* szPath)
	{
		if (szPath == nullptr || szPath[0] == '\0')
		{
			return;
		}
		PendingPath() = szPath;
		PendingFlag() = true;
	}

	bool ConsumePendingDump(std::string& strPathOut)
	{
		if (!PendingFlag())
		{
			return false;
		}
		strPathOut = PendingPath();
		PendingFlag() = false;
		return true;
	}
}
