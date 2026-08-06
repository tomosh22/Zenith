#include "UnitTests/Zenith_UnitTests.h"
#include <cstring>   // memset (canary fills)

// ============================================================================
// Flux_MemoryManager::DownloadBufferData — the headless zero-fill contract.
//
// The backend readback primitive is the ONLY way GPU buffer contents reach the
// CPU, and on a GPU-less backend there is nothing to read: the Null (and D3D12)
// managers fill the destination with zeroes BY CONSTRUCTION. These pin that
// contract in both directions — every requested byte becomes 0, and not one
// byte past the requested size is touched — so a caller can distinguish "the
// GPU reported zero" from "the readback never ran" only by the config it is in,
// never by leftover stack garbage.
//
// Consequence for anything built on top: a test that asserts on REAL buffer
// contents is windowed-only. Headless it would be asserting on this zero-fill.
//
// The GPU-real path cannot be exercised here — it idles the device and needs a
// live allocator — so non-Null configs return (a pass, not a skip: there is no
// zero-fill contract on those backends to violate). The Null branch constructs
// its own manager rather than reaching for the engine's: the stub is stateless,
// so a local instance runs the identical code with no boot-order coupling.
// ============================================================================

ZENITH_TEST(FluxBufferReadback, DownloadZeroFillsDestinationHeadless)
{
	if (!Zenith_IsNullRenderer())
	{
		return;
	}
#ifdef ZENITH_NULL_RENDERER
	constexpr size_t uBYTES = 64;

	Flux_MemoryManager xMem;
	const Flux_VRAMHandle xHandle = xMem.CreateBufferVRAM(
		static_cast<u_int>(uBYTES),
		static_cast<MemoryFlags>(1 << MEMORY_FLAGS__UNORDERED_ACCESS),
		MEMORY_RESIDENCY_GPU);
	ZENITH_ASSERT_TRUE(xHandle.IsValid(), "null backend hands back a valid-looking buffer handle");

	// Canary: a destination that is already all-zero could not tell a real
	// zero-fill apart from a no-op.
	u_int8 auDst[uBYTES];
	memset(auDst, 0xAB, uBYTES);

	xMem.DownloadBufferData(xHandle, auDst, uBYTES);

	size_t uNonZero = 0;
	for (size_t u = 0; u < uBYTES; ++u)
	{
		if (auDst[u] != 0)
		{
			++uNonZero;
		}
	}
	ZENITH_ASSERT_EQ(uNonZero, size_t(0), "every requested byte is zeroed");
#endif
}

ZENITH_TEST(FluxBufferReadback, DownloadWritesExactlyRequestedSizeHeadless)
{
	if (!Zenith_IsNullRenderer())
	{
		return;
	}
#ifdef ZENITH_NULL_RENDERER
	constexpr size_t uREQUESTED = 48;
	constexpr size_t uTAIL      = 48;

	Flux_MemoryManager xMem;
	const Flux_VRAMHandle xHandle = xMem.CreateBufferVRAM(
		static_cast<u_int>(uREQUESTED),
		static_cast<MemoryFlags>(1 << MEMORY_FLAGS__UNORDERED_ACCESS),
		MEMORY_RESIDENCY_GPU);

	// The tail beyond the requested size is the overrun tripwire: a stub that
	// zeroed a padded/rounded length instead of exactly uREQUESTED would clear it.
	u_int8 auDst[uREQUESTED + uTAIL];
	memset(auDst, 0xCD, sizeof(auDst));

	xMem.DownloadBufferData(xHandle, auDst, uREQUESTED);

	size_t uNonZeroInRange = 0;
	for (size_t u = 0; u < uREQUESTED; ++u)
	{
		if (auDst[u] != 0)
		{
			++uNonZeroInRange;
		}
	}
	ZENITH_ASSERT_EQ(uNonZeroInRange, size_t(0), "requested range fully zeroed");

	size_t uClobberedTailBytes = 0;
	for (size_t u = uREQUESTED; u < uREQUESTED + uTAIL; ++u)
	{
		if (auDst[u] != 0xCD)
		{
			++uClobberedTailBytes;
		}
	}
	ZENITH_ASSERT_EQ(uClobberedTailBytes, size_t(0), "nothing written past the requested size");
#endif
}
