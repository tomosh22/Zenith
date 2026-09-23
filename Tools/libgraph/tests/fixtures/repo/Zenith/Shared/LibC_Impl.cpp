#include "FxPch.h"

// Scenario 11: pure used-undeclared. LibC's .cs does NOT declare LibA (see
// Sharpmake_LibC.cs), and no allowlist line matches these includes.
#include "LibA/LibA.h"
#include "LibA/LibA_Solo.h"

// Scenario 16: ambiguous include -- "Zenith" and "Zenith/LibA" both carry a
// FxConfig.h; two include dirs could satisfy this, so it resolves to the
// first (Zenith/FxConfig.h) and should be reported ambiguous.
#include "FxConfig.h"

#include "Shared/LibC_Api.h"

void LibC_Api_Touch()
{
	// Defined here, out of line, so LibA.Tests.inl's test-only reach has
	// something real to call.
}

// Scenario 11: a mention -- sizeof() only, neither embed nor ref.
static const std::size_t s_ulLibADataSize = sizeof(LibA_Data);

// Scenario 11: a mention -- a by-value function parameter.
void LibC_Take(LibA_Data xData)
{
	(void)xData;
	(void)s_ulLibADataSize;
}

// Scenario 7: LibC_Impl.cpp is LibA_Solo.h's one-and-only outside consumer.
static LibA_SoloThing s_xSolo;

// Scenario 18: link-only. Zenith/EngineComposition/Composition.cpp declares
// `extern void LibC_ExportAll();` and calls it without including any LibC
// header. Defined here, out of line, with no LibC header declaring it.
void LibC_ExportAll()
{
	(void)s_xSolo;
}
