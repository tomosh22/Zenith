#pragma once

#include "Core/Zenith_Result.h"
#include "DataStream/Zenith_DataStream.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"

#include <cstdint>

// ============================================================================
// ZM_SaveSchema -- the pure Zenithmon game-payload codec.
//
// This is deliberately below Zenith_SaveData: it owns only the inner ZMSV
// payload and never names slots, files, ECS components, or runtime scenes.
// Writes append one complete payload transactionally. Reads consume one caller-
// bounded payload transactionally and publish a GameState only after every
// framed module validates.
//
// ★ TWO SCHEMA VERSIONS ARE READABLE; ONE IS WRITTEN. Write always emits v2. Read
// accepts v2 and MIGRATES v1 forward (ZM-D-201); every other value is
// VERSION_MISMATCH, and there is still no forward-compatible read of a newer one.
// v1 is v2 minus save module 12, so the migration synthesizes an EMPTY collected
// ground-item set and changes not one byte of modules 1..11 -- which is why the
// literal 824-byte v1 golden in Tests/ZM_Tests_SaveMigration.cpp still describes
// every module a v2 payload carries in common with it.
// ============================================================================

namespace ZM_SaveSchema
{
	static constexpr uint32_t uMAGIC = 0x56534D5Au; // "ZMSV" little-endian
	static constexpr uint32_t uSCHEMA_VERSION_CURRENT = 2u;
	static constexpr uint32_t uMODULE_VERSION_CURRENT = 1u;
	static constexpr uint32_t uMODULE_COUNT = 12u;

	// The one RETIRED schema version this codec still reads. It is spelled here, not
	// buried in Read, because it is the whole of the migration's public surface: a
	// caller (and a unit) needs to name "the version a save on disk might still be".
	static constexpr uint32_t uSCHEMA_VERSION_V1 = 1u;
	// ...and the module count THAT version's payloads legitimately carry. The header
	// check is version-aware rather than relaxed: a v1 blob claiming 12 modules and a
	// v2 blob claiming 11 are both still CORRUPT_DATA.
	static constexpr uint32_t uMODULE_COUNT_V1 = 11u;

	// TOTAL: the module count a payload of this schema version must declare, or 0 for
	// a version this codec cannot read. Exposed so a unit can drive the version /
	// count pairing without re-spelling either number.
	constexpr uint32_t ModuleCountForSchemaVersion(uint32_t uSchemaVersion)
	{
		if (uSchemaVersion == uSCHEMA_VERSION_CURRENT) { return uMODULE_COUNT; }
		if (uSchemaVersion == uSCHEMA_VERSION_V1)      { return uMODULE_COUNT_V1; }
		return 0u;
	}

	Zenith_Status Write(const ZM_GameState& xState, Zenith_DataStream& xOutStream);
	Zenith_Status Read(Zenith_DataStream& xInStream, uint64_t ulByteLength,
		ZM_GameState& xOutState);
}
