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
// ★ THREE SCHEMA VERSIONS ARE READABLE; ONE IS WRITTEN. Write always emits v3. Read
// accepts v3 and MIGRATES v2 and v1 forward (ZM-D-201, ZM-D-223); every other value
// is VERSION_MISMATCH, and there is still no forward-compatible read of a newer one.
//
// ★★ v3 CHANGES NO BYTE OF THE WIRE FORMAT -- IT CHANGES WHAT A NUMBER MEANS. v2 and
// v3 payloads are structurally identical, same 12 modules, same field order, same
// lengths. What moved is the COORDINATE SPACE of the stored world position: v1/v2
// recorded the player's body CENTRE (the physics body position, which then WAS the
// centre), v3 records its FEET (ZM-D-223 moved the human entity origin). A v2 save
// read without migration therefore lands the player exactly one body half-height in
// the air, on a schema check that passes and a payload that validates. That is
// precisely why the version had to move: nothing else on the wire could tell the two
// apart.
// v1 is v2 minus save module 12, so the migration synthesizes an EMPTY collected
// ground-item set and changes not one byte of modules 1..11 -- which is why the
// literal 824-byte v1 golden in Tests/ZM_Tests_SaveMigration.cpp still describes
// every module a v2 payload carries in common with it.
// ============================================================================

namespace ZM_SaveSchema
{
	static constexpr uint32_t uMAGIC = 0x56534D5Au; // "ZMSV" little-endian
	static constexpr uint32_t uSCHEMA_VERSION_CURRENT = 3u;
	static constexpr uint32_t uMODULE_VERSION_CURRENT = 1u;
	static constexpr uint32_t uMODULE_COUNT = 12u;

	// The RETIRED schema versions this codec still reads. They are spelled here, not
	// buried in Read, because they are the whole of the migration's public surface: a
	// caller (and a unit) needs to name "the version a save on disk might still be".
	static constexpr uint32_t uSCHEMA_VERSION_V1 = 1u;
	static constexpr uint32_t uSCHEMA_VERSION_V2 = 2u;
	// ...and the module count THOSE versions' payloads legitimately carry. The header
	// check is version-aware rather than relaxed: a v1 blob claiming 12 modules and a
	// v2 blob claiming 11 are both still CORRUPT_DATA.
	//
	// ★ v2 AND v3 SHARE A COUNT, AND THAT IS NOT A BUG. v2->v3 is a semantic
	// migration, not a structural one -- no module was added, so there is no count to
	// distinguish them by. The version word is the ONLY thing that separates the two,
	// which is exactly why the migration below cannot be inferred from the payload.
	static constexpr uint32_t uMODULE_COUNT_V1 = 11u;
	static constexpr uint32_t uMODULE_COUNT_V2 = 12u;

	// True iff this codec can read uSchemaVersion at all (current or migratable).
	// Spelled once so Read's reject and a unit's expectation cannot disagree.
	constexpr bool IsReadableSchemaVersion(uint32_t uSchemaVersion)
	{
		return uSchemaVersion == uSCHEMA_VERSION_CURRENT
			|| uSchemaVersion == uSCHEMA_VERSION_V2
			|| uSchemaVersion == uSCHEMA_VERSION_V1;
	}

	// True iff a payload of this version stores the player's world position as a body
	// CENTRE rather than as its FEET, and so needs the ZM-D-223 coordinate migration.
	constexpr bool SchemaVersionStoresBodyCentre(uint32_t uSchemaVersion)
	{
		return uSchemaVersion == uSCHEMA_VERSION_V1
			|| uSchemaVersion == uSCHEMA_VERSION_V2;
	}

	// ★★ THE OFFSET THOSE PAYLOADS WERE WRITTEN WITH -- FROZEN, AND DELIBERATELY NOT
	// fZM_HUMAN_BODY_HALF_HEIGHT.
	//
	// The two are the same number TODAY, and that is exactly why the distinction is
	// easy to miss. They are not the same FACT. fZM_HUMAN_BODY_HALF_HEIGHT is a live
	// gameplay tuning value: how tall a Zenithmon human is right now, and a thing a
	// designer may retune. This is a statement about BYTES THAT ALREADY EXIST -- the
	// distance between the centre a v1/v2 save recorded and the feet it meant, fixed
	// forever at the moment those saves were written.
	//
	// Deriving the migration from the live constant would make a historical save
	// decode DIFFERENTLY after a character-height change: every save on every disk
	// would silently shift by the retune delta, on a code path nobody touched, with
	// the schema version still reading 2 and every field still validating. A
	// migration must be a pure function of the bytes and their version, and nothing
	// else.
	//
	// It has no static_assert tying it to the body contract on purpose: such an
	// assert would fire on a legitimate retune and invite someone to "fix" it by
	// re-linking the two, which is the defect.
	inline constexpr float fHISTORICAL_BODY_HALF_HEIGHT = 0.9f;

	// TOTAL: the module count a payload of this schema version must declare, or 0 for
	// a version this codec cannot read. Exposed so a unit can drive the version /
	// count pairing without re-spelling either number.
	constexpr uint32_t ModuleCountForSchemaVersion(uint32_t uSchemaVersion)
	{
		if (uSchemaVersion == uSCHEMA_VERSION_CURRENT) { return uMODULE_COUNT; }
		if (uSchemaVersion == uSCHEMA_VERSION_V2)      { return uMODULE_COUNT_V2; }
		if (uSchemaVersion == uSCHEMA_VERSION_V1)      { return uMODULE_COUNT_V1; }
		return 0u;
	}

	Zenith_Status Write(const ZM_GameState& xState, Zenith_DataStream& xOutStream);
	Zenith_Status Read(Zenith_DataStream& xInStream, uint64_t ulByteLength,
		ZM_GameState& xOutState);
}
