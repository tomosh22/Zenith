#include "Zenith.h"

#include "Core/Zenith_ErrorCode.h"
#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"
#include "Zenithmon/Source/Core/ZM_SaveSchema.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/World/ZM_GroundItem.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"     // ZM_GetWorldSpec -- a resume point that is genuinely SET
#include "Zenithmon/Source/Save/ZM_ResumePoint.h"    // ZM_MakeWorldPosition

#include <cstring>   // memcpy -- the mutable copies the framing units patch

// ============================================================================
// ZM_Tests_SaveMigration -- the independently authored schema wire goldens, one
// literal blob per shipped version, and the v1 -> v2 migration they gate.
//
// Both arrays are the compatibility artifacts: they are literal, complete, and do
// not use the codec, a generated asset, or a zero-fill/compression helper to
// describe any byte. auV1Golden was authored and observed RED before
// ZM_SaveSchema landed at S7 item 1 SC2.
//
// ★★ auV1Golden IS NEVER REGENERATED. It is the ONLY independent record of a wire
// format the writer can no longer produce, and re-deriving it from the current
// codec would make the migration test self-referential -- it would then prove
// "the reader agrees with the writer", which is true of any pair of broken
// halves, instead of "the reader still understands bytes on someone's disk".
// ZM-D-201 bumped the schema and did NOT touch one byte of it.
//
// ★ WHAT CHANGED AT v2, AND HOW LITTLE. v2 is v1 plus save module 12 (the
// collected ground-item set), so exactly three things differ in a payload of the
// same state: the schemaVersion word, the moduleCount word, and an appended
// twelfth framed module. Modules 1..11 are byte-for-byte identical -- which is
// what GoldenV1_ModuleBytesSurviveVerbatimInsideTheV2Payload measures rather than
// asserts in prose, and what lets the retired v1 golden keep describing every
// module a v2 payload shares with it.
// ============================================================================

namespace
{
	// ---- Framing offsets, restated so a unit can PATCH a header word ------------
	// The three inner-header fields are at fixed offsets by construction (the header
	// precedes every variable-length module), so unlike a module payload these can be
	// addressed directly.
	static constexpr u_int uWIRE_MAGIC_OFFSET = 0u;
	static constexpr u_int uWIRE_SCHEMA_VERSION_OFFSET = 4u;
	static constexpr u_int uWIRE_MODULE_COUNT_OFFSET = 8u;
	static constexpr u_int uWIRE_INNER_HEADER_BYTES = 12u;
	static constexpr u_int uWIRE_MODULE_HEADER_BYTES = 12u;

	static constexpr uint8_t auV1Golden[824] =
	{
		0x5A, 0x4D, 0x53, 0x56, 0x01, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // 000
		0x01, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, // 016
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x00, 0x00, 0x02, 0x00, // 032
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 048
		0x00, 0x00, 0x00, 0x01, 0x7F, 0x02, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 064
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xE2, 0x01, // 080
		0x00, 0x00, 0x10, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 096
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 112
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 128
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 144
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 160
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 176
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 192
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 208
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 224
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 240
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 256
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 272
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 288
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 304
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 320
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 336
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 352
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 368
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 384
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 400
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 416
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 432
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 448
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 464
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 480
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 496
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 512
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 528
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 544
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 560
		0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, // 576
		0x98, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 592
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 608
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // 624
		0x04, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x02, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // 640
		0x01, 0x00, 0x00, 0x00, 0x81, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, // 656
		0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, // 672
		0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, // 688
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, // 704
		0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, 0x07, 0x06, // 720
		0x05, 0x04, 0x03, 0x02, 0x01, 0x0A, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, // 736
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 752
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 768
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 784
		0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, // 800
		0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x02                                                  // 816
	};

	static_assert(sizeof(auV1Golden) == 824u);

	// ---- The v2 golden ----------------------------------------------------------
	//
	// The SAME fixture state as the v1 golden, plus two collected ground-item props
	// (ids 0 and 2 -- a proper subset WITH A GAP, so a codec that marks everything and
	// a codec that marks nothing both fail).
	//
	// ★ THE IDS ARE WRITTEN HERE AS LITERAL WIRE BYTES (`0x00, 0x00` and `0x02, 0x00`)
	// AND NEVER AS (uint16_t)ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE. The enum VALUE *is*
	// the persisted id, so an assertion phrased against the enum would move
	// consistently with a renumbering and could never fail -- which is exactly the
	// class of drift a golden exists to catch. If a renumbering is ever deliberate,
	// these two bytes are what has to be re-derived, and that is the point.
	//
	// ★ THE FIRST 824 BYTES ARE auV1Golden WITH TWO WORDS PATCHED -- schemaVersion
	// 1 -> 2 at offset 4, moduleCount 0x0B -> 0x0C at offset 8 -- AND NOTHING ELSE,
	// which is the whole v2 claim stated as data. It is spelled out in full rather
	// than built from the other array at runtime, because a golden assembled by code
	// is a golden that agrees with whatever that code does.
	//
	// Appended at 824: module 12's frame -- id 12, version 1, byteLength 6 -- then the
	// payload `entryCount = 2`, `id = 0`, `id = 2`.
	static constexpr uint8_t auV2Golden[842] =
	{
		0x5A, 0x4D, 0x53, 0x56, 0x02, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // 000
		0x01, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, // 016
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x00, 0x00, 0x02, 0x00, // 032
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 048
		0x00, 0x00, 0x00, 0x01, 0x7F, 0x02, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 064
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xE2, 0x01, // 080
		0x00, 0x00, 0x10, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 096
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 112
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 128
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 144
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 160
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 176
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 192
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 208
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 224
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 240
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 256
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 272
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 288
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 304
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 320
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 336
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 352
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 368
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 384
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 400
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 416
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 432
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 448
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 464
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 480
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 496
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 512
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 528
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 544
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 560
		0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, // 576
		0x98, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 592
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 608
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // 624
		0x04, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x02, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // 640
		0x01, 0x00, 0x00, 0x00, 0x81, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, // 656
		0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, // 672
		0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, // 688
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, // 704
		0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, 0x07, 0x06, // 720
		0x05, 0x04, 0x03, 0x02, 0x01, 0x0A, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, // 736
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 752
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 768
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 784
		0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, // 800
		0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x02, 0x0C, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // 816
		0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00                                      // 832
	};

	// v2 == v1 plus one module header plus a { entryCount, id, id } payload. Spelled
	// as arithmetic over the OTHER array's size so the two can never drift apart
	// silently: change one and this stops compiling.
	static constexpr u_int uV2_GROUND_ITEM_MODULE_BYTES =
		uWIRE_MODULE_HEADER_BYTES + 2u + 2u * 2u;
	static_assert(sizeof(auV2Golden) == sizeof(auV1Golden) + uV2_GROUND_ITEM_MODULE_BYTES);

	// The state BOTH goldens describe below module 12. Untouched by ZM-D-201.
	ZM_GameState MakeGoldenV1State()
	{
		ZM_GameState xState;
		ZM_Monster xMonster;
		xMonster.m_eSpecies = ZM_SPECIES_FERNFAWN;
		xMonster.m_uLevel = 1u;
		xMonster.m_uCurrentExp = 0u;
		xMonster.m_auIV[0] = 0u;
		xMonster.m_auIV[1] = 1u;
		xMonster.m_auIV[2] = 2u;
		xMonster.m_auIV[3] = 3u;
		xMonster.m_auIV[4] = 4u;
		xMonster.m_auIV[5] = 5u;
		xMonster.m_auEV[0] = 6u;
		xMonster.m_auEV[1] = 7u;
		xMonster.m_auEV[2] = 8u;
		xMonster.m_auEV[3] = 9u;
		xMonster.m_auEV[4] = 10u;
		xMonster.m_auEV[5] = 11u;
		xMonster.m_eNature = ZM_NATURE_FERAL;
		xMonster.m_eAbility = ZM_ABILITY_GRAZER;
		xMonster.m_eStatus = ZM_MAJOR_STATUS_POISON;
		for (u_int uMove = 0u; uMove < uZM_MAX_MOVES; ++uMove)
		{
			xMonster.m_axMoves[uMove].m_eMove = ZM_MOVE_NONE;
			xMonster.m_axMoves[uMove].m_uCurPP = 0u;
			xMonster.m_axMoves[uMove].m_uMaxPP = 0u;
		}
		xMonster.m_uCurrentHp = 0u;
		xMonster.m_eGender = ZM_GENDER_FEMALE;
		xMonster.m_uFriendship = 0x7Fu;
		xMonster.m_uFlags = uZM_MONSTER_FLAG_IS_SHINY;
		xMonster.m_szNickname[0] = 'A';

		xState.m_xParty.Add(xMonster);
		xState.MarkCaught(ZM_SPECIES_FERNFAWN);
		xState.m_xStoryFlags.Set(9u, true);
		xState.m_uBadgeMask = 0x81u;
		xState.m_xBag.Add(ZM_ITEM_CATCHORB, 2u);
		xState.m_uMoney = 0x12345678u;
		xState.m_xTowerRun.m_uCurrentStreak = 1u;
		xState.m_xTowerRun.m_uBestStreak = 2u;
		xState.m_xTowerRun.m_ulSeed = 0x0102030405060708ull;
		xState.m_xOptions.m_eTextSpeed = ZM_TEXT_SPEED_FAST;
		xState.m_bPendingWhiteout = true;
		return xState;
	}

	// ...and the v2 fixture: the SAME state plus the two collected props the v2
	// golden's module 12 spells. The MARKS go through ZM_GroundItemSet::Mark rather
	// than through ZM_TryPickUpGroundItem, deliberately: this fixture must describe a
	// SAVE, not a playthrough, and routing it through the pickup path would also move
	// the bag and stop the two goldens sharing modules 1..11.
	ZM_GameState MakeGoldenV2State()
	{
		ZM_GameState xState = MakeGoldenV1State();
		xState.m_xCollectedGroundItems.Mark(ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE);
		xState.m_xCollectedGroundItems.Mark(ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE);
		return xState;
	}

	void AssertBytesMatch(const Zenith_DataStream& xStream, const uint8_t* puGolden,
		u_int uGoldenLength, const char* szContext)
	{
		ZENITH_ASSERT_EQ(xStream.GetCursor(), (uint64_t)uGoldenLength,
			"%s byte count", szContext);
		const uint8_t* pActual = static_cast<const uint8_t*>(xStream.GetData());
		ZENITH_ASSERT_NOT_NULL(pActual, "%s data", szContext);
		if (pActual == nullptr || xStream.GetCursor() < (uint64_t)uGoldenLength) { return; }

		for (u_int uByte = 0u; uByte < uGoldenLength; ++uByte)
		{
			ZENITH_ASSERT_EQ((u_int)pActual[uByte], (u_int)puGolden[uByte],
				"%s byte %u", szContext, uByte);
		}
	}

	u_int ReadWireU32(const uint8_t* puBytes, u_int uOffset)
	{
		return (u_int)puBytes[uOffset]
			| ((u_int)puBytes[uOffset + 1u] << 8u)
			| ((u_int)puBytes[uOffset + 2u] << 16u)
			| ((u_int)puBytes[uOffset + 3u] << 24u);
	}

	// The same, for a wire float. memcpy rather than a reinterpret_cast: the goldens
	// are byte arrays with no alignment guarantee, and type-punning through a pointer
	// cast is UB the optimiser is entitled to act on.
	float ReadWireFloat(const uint8_t* puBytes, u_int uOffset)
	{
		float fValue = 0.0f;
		memcpy(&fValue, puBytes + uOffset, sizeof(fValue));
		return fValue;
	}

	// ---- The v2 RESUME golden -----------------------------------------------------
	//
	// ★★ A SECOND v2 LITERAL, AND IT EXISTS BECAUSE THE FIRST ONE CANNOT TEST THIS.
	// auV2Golden's fixture never set a resume point, so its world-position module is
	// all zeroes -- it can prove the ZM-D-223 coordinate migration LEAVES AN UNSET
	// POSITION ALONE, and nothing else. A migration that converted the wrong axis, by
	// the wrong amount, or not at all would pass every clause that array can carry.
	//
	// This one is a v2 save taken while STANDING somewhere: Dawnmere, spawn tag
	// "TownCenter", body centre (120, 25.95666, 60), yaw 0. The Y is the real figure
	// such a save held -- TownCenter's marker feet 25.05666 plus one body half-height
	// -- so migrating it must yield exactly the marker back.
	//
	// ★ IT IS FROZEN, LIKE auV1Golden, AND FOR THE SAME REASON. SaveFormat.md's
	// migration policy is binding: canned blobs are compiled C byte arrays, never
	// regenerated from the codec. A fixture built by running the CURRENT writer and
	// patching its version word would only prove "the reader agrees with the writer",
	// which is true of any pair of broken halves -- and it would silently follow the
	// writer to v4. These bytes are what a v2 save on someone's disk actually says,
	// and they must never be re-derived.
	static constexpr uint8_t auV2ResumeGolden[842] =
	{
		0x5Au, 0x4Du, 0x53u, 0x56u, 0x02u, 0x00u, 0x00u, 0x00u, 0x0Cu, 0x00u, 0x00u, 0x00u,
		0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x3Eu, 0x00u, 0x00u, 0x00u,
		0x01u, 0x01u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x02u, 0x03u,
		0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x09u, 0x0Au, 0x0Bu, 0x00u, 0x00u, 0x02u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x7Fu, 0x02u, 0x41u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0xE2u, 0x01u,
		0x00u, 0x00u, 0x10u, 0x1Eu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
		0x28u, 0x00u, 0x00u, 0x00u, 0x98u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u,
		0x01u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x0Au, 0x00u, 0x00u, 0x02u,
		0x05u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
		0x81u, 0x06u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x06u, 0x00u, 0x00u,
		0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x01u,
		0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x78u, 0x56u, 0x34u, 0x12u, 0x08u,
		0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u,
		0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u,
		0x00u, 0x08u, 0x07u, 0x06u, 0x05u, 0x04u, 0x03u, 0x02u, 0x01u, 0x0Au, 0x00u, 0x00u,
		0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x34u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u,
		0x00u, 0x54u, 0x6Fu, 0x77u, 0x6Eu, 0x43u, 0x65u, 0x6Eu, 0x74u, 0x65u, 0x72u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xF0u,
		0x42u, 0x3Du, 0xA7u, 0xCFu, 0x41u, 0x00u, 0x00u, 0x70u, 0x42u, 0x00u, 0x00u, 0x00u,
		0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x07u, 0x00u, 0x00u,
		0x00u, 0x01u, 0x00u, 0x01u, 0x00u, 0x01u, 0x00u, 0x02u, 0x0Cu, 0x00u, 0x00u, 0x00u,
		0x01u, 0x00u, 0x00u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u,
		0x02u, 0x00u,
	};
	static_assert(sizeof(auV2ResumeGolden) == 842u);

	// Where the world position's three floats sit inside that literal, MEASURED from
	// it rather than computed from the module layout: the test asserts the bytes at
	// these offsets really are the centre triple, so a layout change reds here
	// instead of silently moving what the migration is applied to.
	static constexpr u_int uV2_RESUME_POS_X_OFFSET = 789u;
	static constexpr u_int uV2_RESUME_POS_Y_OFFSET = 793u;
	static constexpr u_int uV2_RESUME_POS_Z_OFFSET = 797u;

	// A resume point that is genuinely SET, so the coordinate migration has something
	// to convert. The Y is the one number under test; X and Z are carried through
	// unchanged and are asserted as such, because a migration that shifted all three
	// would be just as wrong and just as invisible to a Y-only check.
	constexpr float fMIGRATION_PROBE_X = 120.0f;
	constexpr float fMIGRATION_PROBE_CENTRE_Y = 25.95666f;
	constexpr float fMIGRATION_PROBE_Z = 60.0f;


	void WriteWireU32(uint8_t* puBytes, u_int uOffset, u_int uValue)
	{
		puBytes[uOffset] = (uint8_t)(uValue & 0xffu);
		puBytes[uOffset + 1u] = (uint8_t)((uValue >> 8u) & 0xffu);
		puBytes[uOffset + 2u] = (uint8_t)((uValue >> 16u) & 0xffu);
		puBytes[uOffset + 3u] = (uint8_t)((uValue >> 24u) & 0xffu);
	}

	// A destination whose fields are all UNLIKE either golden's, so "the read failed
	// and changed nothing" is a claim with teeth.
	ZM_GameState MakeUntouchableDestination()
	{
		ZM_GameState xState;
		xState.m_uMoney = 0xABCDEF01u;
		xState.m_uBadgeMask = 0x3Cu;
		xState.m_xCollectedGroundItems.Mark(ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB);
		return xState;
	}

	void AssertDestinationUntouched(const ZM_GameState& xState, const char* szCase)
	{
		ZENITH_ASSERT_EQ(xState.m_uMoney, 0xABCDEF01u, "%s changed the destination money", szCase);
		ZENITH_ASSERT_EQ((u_int)xState.m_uBadgeMask, 0x3Cu,
			"%s changed the destination badges", szCase);
		ZENITH_ASSERT_TRUE(
			xState.m_xCollectedGroundItems.IsSet(ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB),
			"%s cleared the destination's collected prop", szCase);
		ZENITH_ASSERT_EQ(xState.m_xCollectedGroundItems.Count(), 1u,
			"%s changed the destination's collected count", szCase);
		ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 0u, "%s populated the destination party", szCase);
	}

	// Drive one refusal case. The destination lives in THIS frame rather than in the
	// caller's, so however many cases a unit runs, only one ~80 KB ZM_GameState is
	// alive at a time -- the same budget the shipped codec units hold.
	void ExpectReadRefused(uint8_t* puBytes, u_int uLength, Zenith_ErrorCode eExpected,
		const char* szCase)
	{
		Zenith_DataStream xStream(puBytes, uLength);
		ZM_GameState xDestination = MakeUntouchableDestination();
		const Zenith_Status xStatus = ZM_SaveSchema::Read(xStream, uLength, xDestination);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "%s decoded", szCase);
		ZENITH_ASSERT_EQ((u_int)xStatus.Error(), (u_int)eExpected,
			"%s returned the wrong error", szCase);
		ZENITH_ASSERT_EQ(xStream.GetCursor(), 0ull, "%s advanced the input cursor", szCase);
		AssertDestinationUntouched(xDestination, szCase);
	}

	void AssertGoldenMonster(const ZM_Monster& xMonster)
	{
		ZENITH_ASSERT_EQ((u_int)xMonster.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN);
		ZENITH_ASSERT_EQ(xMonster.m_uLevel, 1u);
		ZENITH_ASSERT_EQ(xMonster.m_uCurrentExp, 0u);
		for (u_int uStat = 0u; uStat < ZM_STAT_COUNT; ++uStat)
		{
			ZENITH_ASSERT_EQ(xMonster.m_auIV[uStat], uStat, "IV %u", uStat);
			ZENITH_ASSERT_EQ(xMonster.m_auEV[uStat], uStat + 6u, "EV %u", uStat);
		}
		ZENITH_ASSERT_EQ((u_int)xMonster.m_eNature, (u_int)ZM_NATURE_FERAL);
		ZENITH_ASSERT_EQ((u_int)xMonster.m_eAbility, (u_int)ZM_ABILITY_GRAZER);
		ZENITH_ASSERT_EQ((u_int)xMonster.m_eStatus, (u_int)ZM_MAJOR_STATUS_POISON);
		for (u_int uMove = 0u; uMove < uZM_MAX_MOVES; ++uMove)
		{
			ZENITH_ASSERT_EQ((u_int)xMonster.m_axMoves[uMove].m_eMove, (u_int)ZM_MOVE_NONE,
				"move %u id", uMove);
			ZENITH_ASSERT_EQ(xMonster.m_axMoves[uMove].m_uCurPP, 0u, "move %u current PP", uMove);
			ZENITH_ASSERT_EQ(xMonster.m_axMoves[uMove].m_uMaxPP, 0u, "move %u maximum PP", uMove);
		}
		ZENITH_ASSERT_EQ(xMonster.m_uCurrentHp, 0u);
		ZENITH_ASSERT_EQ((u_int)xMonster.m_eGender, (u_int)ZM_GENDER_FEMALE);
		ZENITH_ASSERT_EQ(xMonster.m_uFriendship, 0x7Fu);
		ZENITH_ASSERT_EQ(xMonster.m_uFlags, uZM_MONSTER_FLAG_IS_SHINY);
		ZENITH_ASSERT_EQ((u_int)(unsigned char)xMonster.m_szNickname[0], (u_int)'A');
		for (u_int uByte = 1u; uByte < uZM_MONSTER_NICKNAME_CAPACITY; ++uByte)
		{
			ZENITH_ASSERT_EQ((u_int)(unsigned char)xMonster.m_szNickname[uByte], 0u,
				"nickname padding byte %u", uByte);
		}
	}

	// Everything BOTH goldens carry -- i.e. modules 1..11. Module 12 is asserted at
	// each call site, because that is the ONE thing the two disagree about.
	void AssertGoldenDecodedState(const ZM_GameState& xState)
	{
		ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 1u);
		if (xState.m_xParty.Count() == 1u)
		{
			AssertGoldenMonster(xState.m_xParty.Get(0u));
		}

		ZENITH_ASSERT_EQ(xState.m_xBoxes.Count(), 0u);
		for (u_int uBox = 0u; uBox < uZM_BOX_COUNT; ++uBox)
		{
			for (u_int uSlot = 0u; uSlot < uZM_BOX_SLOTS_PER_BOX; ++uSlot)
			{
				ZENITH_ASSERT_NULL(xState.m_xBoxes.TryGet(uBox, uSlot),
					"box %u slot %u", uBox, uSlot);
			}
		}

		ZENITH_ASSERT_EQ(xState.GetSeenCount(), 1u);
		ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 1u);
		for (u_int uSpecies = 0u; uSpecies < (u_int)ZM_SPECIES_COUNT; ++uSpecies)
		{
			const bool bFernfawn = uSpecies == (u_int)ZM_SPECIES_FERNFAWN;
			ZENITH_ASSERT_EQ(xState.IsSeen((ZM_SPECIES_ID)uSpecies), bFernfawn,
				"seen species %u", uSpecies);
			ZENITH_ASSERT_EQ(xState.IsCaught((ZM_SPECIES_ID)uSpecies), bFernfawn,
				"caught species %u", uSpecies);
		}

		ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), 1u);
		for (u_int uByte = 0u; uByte < uZM_STORY_FLAG_BYTE_COUNT; ++uByte)
		{
			const u_int uExpected = (uByte == 1u) ? 0x02u : 0u;
			ZENITH_ASSERT_EQ((u_int)xState.m_xStoryFlags.m_auFlags[uByte], uExpected,
				"story byte %u", uByte);
		}

		ZENITH_ASSERT_EQ((u_int)xState.m_uBadgeMask, 0x81u);
		ZENITH_ASSERT_EQ(xState.m_xBag.TotalStackCount(), 1u);
		for (u_int uItem = 0u; uItem < (u_int)ZM_ITEM_COUNT; ++uItem)
		{
			const u_int uExpected = uItem == (u_int)ZM_ITEM_CATCHORB ? 2u : 0u;
			ZENITH_ASSERT_EQ(xState.m_xBag.GetCount((ZM_ITEM_ID)uItem), uExpected,
				"item %u count", uItem);
		}
		ZENITH_ASSERT_EQ(xState.m_uMoney, 0x12345678u);

		ZENITH_ASSERT_EQ(xState.m_xDaycare.m_uParentCount, 0u);
		ZENITH_ASSERT_FALSE(xState.m_xDaycare.m_bEggPresent);
		ZENITH_ASSERT_EQ(xState.m_xDaycare.m_uEggStepsRemaining, 0u);
		ZENITH_ASSERT_EQ(xState.m_xTowerRun.m_uCurrentStreak, 1u);
		ZENITH_ASSERT_EQ(xState.m_xTowerRun.m_uBestStreak, 2u);
		ZENITH_ASSERT_EQ(xState.m_xTowerRun.m_ulSeed, 0x0102030405060708ull);

		ZENITH_ASSERT_EQ(xState.m_xWorldPosition.m_uSceneBuildIndex, uZM_WORLD_SCENE_UNSET);
		for (u_int uByte = 0u; uByte < uZM_WORLD_SPAWN_TAG_CAPACITY; ++uByte)
		{
			ZENITH_ASSERT_EQ((u_int)(unsigned char)xState.m_xWorldPosition.m_szSpawnTag[uByte], 0u,
				"spawn-tag byte %u", uByte);
		}
		for (u_int uAxis = 0u; uAxis < 3u; ++uAxis)
		{
			ZENITH_ASSERT_EQ_FLOAT(xState.m_xWorldPosition.m_afPosition[uAxis], 0.0f, 0.0f,
				"world-position axis %u", uAxis);
		}
		ZENITH_ASSERT_EQ_FLOAT(xState.m_xWorldPosition.m_fYaw, 0.0f, 0.0f);
		ZENITH_ASSERT_EQ((u_int)xState.m_xOptions.m_eTextSpeed, (u_int)ZM_TEXT_SPEED_FAST);
		ZENITH_ASSERT_FALSE(xState.m_bPendingWhiteout,
			"pending whiteout is transient and must not be decoded from the save");
	}
}

// ★ THE TRIPWIRE THAT BROUGHT YOU HERE. The assertions on the CURRENT constants
// below red the instant anyone bumps the schema, which is what forces whoever does
// it back into this file to add their version's literal blob and its migration.
// They moved 1 -> 2 and 11 -> 12 at ZM-D-201; the golden ARRAYS above did not move
// and must not.
//
// The name of this unit changed with its premise. It used to be
// GoldenV1_LiteralBytesMatchCanonicalWriter, and that claim died the moment the
// writer stopped emitting v1 -- the canonical writer can no longer produce
// auV1Golden and never will again. What it can still prove, and what actually
// matters, is that v2 DISTURBED NOTHING: every byte of modules 1..11 is where v1
// left it, and the entire delta is two header words plus an appended module.
ZENITH_TEST(ZM_Save, GoldenV1_ModuleBytesSurviveVerbatimInsideTheV2Payload)
{
	ZENITH_ASSERT_EQ(ZM_SaveSchema::uMAGIC, 0x56534D5Au);
	ZENITH_ASSERT_EQ(ZM_SaveSchema::uSCHEMA_VERSION_CURRENT, 3u);
	ZENITH_ASSERT_EQ(ZM_SaveSchema::uSCHEMA_VERSION_V2, 2u);
	ZENITH_ASSERT_EQ(ZM_SaveSchema::uSCHEMA_VERSION_V1, 1u);
	ZENITH_ASSERT_EQ(ZM_SaveSchema::uMODULE_COUNT_V2, 12u);
	ZENITH_ASSERT_EQ(ZM_SaveSchema::uMODULE_VERSION_CURRENT, 1u);
	ZENITH_ASSERT_EQ(ZM_SaveSchema::uMODULE_COUNT, 12u);
	ZENITH_ASSERT_EQ(ZM_SaveSchema::uMODULE_COUNT_V1, 11u);
	ZENITH_ASSERT_EQ((u_int)ZM_SPECIES_COUNT, 152u);
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesAbilities(ZM_SPECIES_FERNFAWN).m_eRegular,
		(u_int)ZM_ABILITY_GRAZER, "the fixture carries Fernfawn's regular ability");
	// GREATER-OR-EQUAL, not equal: the v2 golden spells prop ids 0 and 2, so the
	// registry must hold at least three rows -- but APPENDING rows changes no byte of
	// either golden (module 12 writes only the ids that are SET), so an equality here
	// would red on a perfectly legal append.
	ZENITH_ASSERT_GE((u_int)ZM_GROUND_ITEM_COUNT, 3u,
		"the v2 golden spells ground-item id 2, which the registry no longer carries");

	// The RETIRED header, read out of the literal bytes themselves rather than from
	// any constant: this is what a v1 save on someone's disk actually says.
	ZENITH_ASSERT_EQ(ReadWireU32(auV1Golden, uWIRE_MAGIC_OFFSET), 0x56534D5Au,
		"the v1 golden's magic");
	ZENITH_ASSERT_EQ(ReadWireU32(auV1Golden, uWIRE_SCHEMA_VERSION_OFFSET), 1u,
		"the v1 golden's schema version");
	ZENITH_ASSERT_EQ(ReadWireU32(auV1Golden, uWIRE_MODULE_COUNT_OFFSET), 11u,
		"the v1 golden's module count");

	const ZM_GameState xState = MakeGoldenV1State();
	ZENITH_ASSERT_TRUE(xState.m_bPendingWhiteout,
		"the source sets the transient latch so equality proves it is excluded");
	ZENITH_ASSERT_EQ(xState.m_xCollectedGroundItems.Count(), 0u,
		"the v1 fixture must carry NO collected prop, or the tail below is not empty");

	Zenith_DataStream xStream;
	const Zenith_Status xStatus = ZM_SaveSchema::Write(xState, xStream);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "the canonical writer rejected its valid fixture");
	if (!xStatus.IsOk()) { return; }

	// A v1-shaped state now encodes to v1's bytes plus an EMPTY module 12.
	const u_int uEmptyTailBytes = uWIRE_MODULE_HEADER_BYTES + 2u;
	ZENITH_ASSERT_EQ(xStream.GetCursor(),
		(uint64_t)(sizeof(auV1Golden) + uEmptyTailBytes),
		"the v2 payload is not the v1 payload plus one empty framed module");
	const uint8_t* pActual = static_cast<const uint8_t*>(xStream.GetData());
	ZENITH_ASSERT_NOT_NULL(pActual, "the canonical writer produced no data");
	if (pActual == nullptr
		|| xStream.GetCursor() != (uint64_t)(sizeof(auV1Golden) + uEmptyTailBytes))
	{
		return;
	}

	// (1) The two words that MUST have moved -- and the writer must genuinely differ
	// from the golden here, or the rest of this unit is comparing a blob to itself.
	ZENITH_ASSERT_EQ(ReadWireU32(pActual, uWIRE_SCHEMA_VERSION_OFFSET), 3u,
		"the writer did not emit schema version 3");
	ZENITH_ASSERT_EQ(ReadWireU32(pActual, uWIRE_MODULE_COUNT_OFFSET), 12u,
		"the writer did not emit twelve modules");

	// (2) EVERY OTHER BYTE OF THE v1 REGION IS UNCHANGED. This is the claim: the magic
	// ahead of the two patched words, and all eleven modules behind them.
	for (u_int uByte = 0u; uByte < uWIRE_SCHEMA_VERSION_OFFSET; ++uByte)
	{
		ZENITH_ASSERT_EQ((u_int)pActual[uByte], (u_int)auV1Golden[uByte],
			"the magic moved at byte %u", uByte);
	}
	for (u_int uByte = uWIRE_INNER_HEADER_BYTES; uByte < (u_int)sizeof(auV1Golden); ++uByte)
	{
		ZENITH_ASSERT_EQ((u_int)pActual[uByte], (u_int)auV1Golden[uByte],
			"module byte %u moved between v1 and v2", uByte);
	}

	// (3) ...and the whole of the delta is one empty, framed module 12.
	const u_int uTail = (u_int)sizeof(auV1Golden);
	ZENITH_ASSERT_EQ(ReadWireU32(pActual, uTail), 12u, "the appended module's id");
	ZENITH_ASSERT_EQ(ReadWireU32(pActual, uTail + 4u), 1u, "the appended module's version");
	ZENITH_ASSERT_EQ(ReadWireU32(pActual, uTail + 8u), 2u, "the appended module's byte length");
	ZENITH_ASSERT_EQ((u_int)pActual[uTail + 12u], 0u, "the appended module's entry count (low)");
	ZENITH_ASSERT_EQ((u_int)pActual[uTail + 13u], 0u, "the appended module's entry count (high)");
}

// THE CANNED-BLOB MIGRATION TEST the SaveFormat.md gate rule demands: a literal
// blob of the retired version, fed to the shipped reader, decoding to the state it
// always meant plus the new module's synthesized default.
ZENITH_TEST(ZM_Save, GoldenV1_LiteralBlobDecodesExpectedState)
{
	Zenith_DataStream xInput(const_cast<uint8_t*>(auV1Golden), sizeof(auV1Golden));
	ZM_GameState xDecoded = MakeUntouchableDestination();
	const Zenith_Status xStatus = ZM_SaveSchema::Read(xInput, sizeof(auV1Golden), xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "the literal v1 golden was not migrated forward");
	if (!xStatus.IsOk()) { return; }
	ZENITH_ASSERT_EQ(xInput.GetCursor(), (uint64_t)sizeof(auV1Golden),
		"the decoder must consume the complete length-bounded payload");

	AssertGoldenDecodedState(xDecoded);

	// ★ THE MIGRATION'S OWN CLAUSE. A v1 save predates ground items entirely, so the
	// only correct answer is that NOTHING has been collected -- and the destination
	// was pre-loaded with a marked prop, so "the migration left the caller's set
	// alone" cannot pass here.
	ZENITH_ASSERT_EQ(xDecoded.m_xCollectedGroundItems.Count(), 0u,
		"a migrated v1 save does not carry an empty collected ground-item set");
	for (u_int uProp = 0u; uProp < (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
	{
		ZENITH_ASSERT_FALSE(xDecoded.m_xCollectedGroundItems.IsSet((ZM_GROUND_ITEM_ID)uProp),
			"a migrated v1 save claims prop %u was already taken", uProp);
	}

	// ...and the migrated state is a fully-formed v2 state: re-encoding it and reading
	// it back must reproduce it. That is the round trip a player gets when an old save
	// is loaded and saved again, and it is deliberately NOT a byte comparison against
	// auV1Golden -- the writer cannot produce those bytes any more, and asserting it
	// could would be asking the migration to undo itself.
	Zenith_DataStream xReencoded;
	const Zenith_Status xReencodeStatus = ZM_SaveSchema::Write(xDecoded, xReencoded);
	ZENITH_ASSERT_TRUE(xReencodeStatus.IsOk(), "the migrated v1 state failed to re-encode");
	if (!xReencodeStatus.IsOk()) { return; }

	const uint64_t ulLength = xReencoded.GetCursor();
	Zenith_DataStream xRoundTrip(xReencoded.GetData(), ulLength);
	ZM_GameState xSecond = MakeUntouchableDestination();
	const Zenith_Status xSecondStatus = ZM_SaveSchema::Read(xRoundTrip, ulLength, xSecond);
	ZENITH_ASSERT_TRUE(xSecondStatus.IsOk(), "the re-encoded migration failed to decode");
	if (!xSecondStatus.IsOk()) { return; }
	AssertGoldenDecodedState(xSecond);
	ZENITH_ASSERT_EQ(xSecond.m_xCollectedGroundItems.Count(), 0u,
		"the re-encoded migration invented a collected prop");
}

// The version gate and the module-count check are ONE decision asked twice, and
// this unit drives both directions of the pairing. Every case is also an
// atomicity case: a refused read leaves the caller's state byte-identical.
ZENITH_TEST(ZM_Save, MigrationV1ToV2_VersionAndModuleCountPairingIsEnforcedAtomically)
{
	ZENITH_ASSERT_EQ(
		ZM_SaveSchema::ModuleCountForSchemaVersion(ZM_SaveSchema::uSCHEMA_VERSION_V1), 11u);
	ZENITH_ASSERT_EQ(
		ZM_SaveSchema::ModuleCountForSchemaVersion(ZM_SaveSchema::uSCHEMA_VERSION_CURRENT), 12u);
	ZENITH_ASSERT_EQ(ZM_SaveSchema::ModuleCountForSchemaVersion(0u), 0u,
		"version 0 must be unreadable -- there is no invented v0");
	ZENITH_ASSERT_EQ(
		ZM_SaveSchema::ModuleCountForSchemaVersion(ZM_SaveSchema::uSCHEMA_VERSION_V2), 12u,
		"v2 is migratable and carries twelve modules -- it shares its count with v3, "
		"because v2->v3 is a SEMANTIC migration and added no module");
	// 4 rather than 3: the probe has to name a version ABOVE current, and current
	// moved to 3. Spelled as CURRENT + 1 so the next bump cannot leave a probe
	// silently asserting that a supported version is unsupported.
	ZENITH_ASSERT_EQ(
		ZM_SaveSchema::ModuleCountForSchemaVersion(ZM_SaveSchema::uSCHEMA_VERSION_CURRENT + 1u), 0u,
		"a version above current must be unreadable -- there is no forward-compatible read");

	// ONE scratch buffer, sized to the LARGER golden and re-filled per case, so the
	// number of cases costs nothing in frame space.
	uint8_t auMutant[sizeof(auV2Golden)] = {};

	// (1) A v1 header claiming TWELVE modules is corruption, not a v2 payload.
	memcpy(auMutant, auV1Golden, sizeof(auV1Golden));
	WriteWireU32(auMutant, uWIRE_MODULE_COUNT_OFFSET, 12u);
	ExpectReadRefused(auMutant, (u_int)sizeof(auV1Golden), Zenith_ErrorCode::CORRUPT_DATA,
		"a v1 header claiming twelve modules");

	// (2) A v2 header claiming ELEVEN modules is corruption, not a v1 payload. This is
	// the direction a relaxed "count <= current" check would silently accept -- and it
	// is exactly the shape of a v2 save whose module 12 has been truncated away.
	memcpy(auMutant, auV2Golden, sizeof(auV2Golden));
	WriteWireU32(auMutant, uWIRE_MODULE_COUNT_OFFSET, 11u);
	ExpectReadRefused(auMutant, (u_int)sizeof(auV2Golden), Zenith_ErrorCode::CORRUPT_DATA,
		"a v2 header claiming eleven modules");

	// (3) A version this codec has never shipped is VERSION_MISMATCH, and the version
	// is decided before the count is even read.
	// CURRENT + 1 rather than a literal: 3 is a supported version now.
	for (const u_int uVersion :
		{ 0u, ZM_SaveSchema::uSCHEMA_VERSION_CURRENT + 1u, 0xffffffffu })
	{
		memcpy(auMutant, auV1Golden, sizeof(auV1Golden));
		WriteWireU32(auMutant, uWIRE_SCHEMA_VERSION_OFFSET, uVersion);
		ExpectReadRefused(auMutant, (u_int)sizeof(auV1Golden),
			Zenith_ErrorCode::VERSION_MISMATCH, "an unsupported schema version");
	}

	// (4) A v1-versioned blob carrying a STOWAWAY module 12 is still corruption: the
	// count says eleven, so the extra frame is a top-level trailing byte run and the
	// migration must not quietly consume it.
	memcpy(auMutant, auV2Golden, sizeof(auV2Golden));
	WriteWireU32(auMutant, uWIRE_SCHEMA_VERSION_OFFSET, 1u);
	WriteWireU32(auMutant, uWIRE_MODULE_COUNT_OFFSET, 11u);
	ExpectReadRefused(auMutant, (u_int)sizeof(auV2Golden), Zenith_ErrorCode::CORRUPT_DATA,
		"a v1 blob with a stowaway module 12");
}

ZENITH_TEST(ZM_Save, GoldenV2_LiteralBytesMatchCanonicalWriter)
{
	const ZM_GameState xState = MakeGoldenV2State();
	ZENITH_ASSERT_EQ(xState.m_xCollectedGroundItems.Count(), 2u,
		"the v2 fixture does not carry the two props its golden spells");
	ZENITH_ASSERT_FALSE(
		xState.m_xCollectedGroundItems.IsSet(ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB),
		"the v2 fixture must leave a GAP, or a decoder that marks everything passes");

	Zenith_DataStream xStream;
	const Zenith_Status xStatus = ZM_SaveSchema::Write(xState, xStream);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "the canonical writer rejected its valid fixture");
	if (!xStatus.IsOk()) { return; }

	// ★★ v3 IS THE v2 GOLDEN WITH EXACTLY ONE WORD CHANGED, AND THIS IS WHERE THAT IS
	// PROVED RATHER THAN ASSUMED. The v2->v3 migration exists precisely because the
	// two payloads are structurally identical -- there is nothing in the data to
	// infer the coordinate space from, so the version word had to carry it. If any
	// OTHER byte moved, that premise is false and the migration is unsound: a v2 save
	// would then be distinguishable, and this unit is the thing that would say so.
	ZENITH_ASSERT_EQ(xStream.GetCursor(), (uint64_t)sizeof(auV2Golden),
		"a v3 payload is not the same length as its v2 golden");
	const uint8_t* pActual = static_cast<const uint8_t*>(xStream.GetData());
	ZENITH_ASSERT_NOT_NULL(pActual, "the canonical writer produced no data");
	if (pActual == nullptr || xStream.GetCursor() != (uint64_t)sizeof(auV2Golden))
	{
		return;
	}
	ZENITH_ASSERT_EQ(ReadWireU32(pActual, uWIRE_SCHEMA_VERSION_OFFSET), 3u,
		"the writer did not emit schema version 3");
	ZENITH_ASSERT_EQ(ReadWireU32(auV2Golden, uWIRE_SCHEMA_VERSION_OFFSET), 2u,
		"the retired golden is no longer a v2 payload");
	for (u_int uByte = 0u; uByte < (u_int)sizeof(auV2Golden); ++uByte)
	{
		const bool bInVersionWord = uByte >= uWIRE_SCHEMA_VERSION_OFFSET
			&& uByte < uWIRE_SCHEMA_VERSION_OFFSET + 4u;
		if (bInVersionWord) { continue; }
		ZENITH_ASSERT_EQ((u_int)pActual[uByte], (u_int)auV2Golden[uByte],
			"v3 differs from its v2 golden outside the version word, at byte %u", uByte);
	}
}

ZENITH_TEST(ZM_Save, GoldenV2_LiteralBlobDecodesExpectedState)
{
	Zenith_DataStream xInput(const_cast<uint8_t*>(auV2Golden), sizeof(auV2Golden));
	ZM_GameState xDecoded = MakeUntouchableDestination();
	const Zenith_Status xStatus = ZM_SaveSchema::Read(xInput, sizeof(auV2Golden), xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "the literal v2 golden failed to decode");
	if (!xStatus.IsOk()) { return; }
	ZENITH_ASSERT_EQ(xInput.GetCursor(), (uint64_t)sizeof(auV2Golden),
		"the decoder must consume the complete length-bounded payload");

	AssertGoldenDecodedState(xDecoded);

	// Module 12, asserted PER PROP rather than by count alone: a count of two is also
	// what a decoder that marked the WRONG two props would report.
	ZENITH_ASSERT_EQ(xDecoded.m_xCollectedGroundItems.Count(), 2u,
		"the decoded collected-prop count is wrong");
	ZENITH_ASSERT_TRUE(
		xDecoded.m_xCollectedGroundItems.IsSet(ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE),
		"wire id 0 did not decode to the first registry row");
	ZENITH_ASSERT_FALSE(
		xDecoded.m_xCollectedGroundItems.IsSet(ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB),
		"the gap in the golden's id list was filled in");
	ZENITH_ASSERT_TRUE(
		xDecoded.m_xCollectedGroundItems.IsSet(ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE),
		"wire id 2 did not decode to the third registry row");

	// ★ THE COORDINATE MIGRATION IS A NO-OP FOR THIS GOLDEN, ON PURPOSE. Its fixture
	// never set a resume point, so its world position is inert padding and ZM-D-223
	// must leave it exactly as found -- shifting an UNSET position would invent a
	// resume point half a body below the origin for every save that never made one.
	ZENITH_ASSERT_EQ(xDecoded.m_xWorldPosition.m_uSceneBuildIndex,
		(uint32_t)uZM_WORLD_SCENE_UNSET,
		"the golden's resume point is set -- this clause no longer tests the no-op");
	for (u_int u = 0u; u < 3u; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(xDecoded.m_xWorldPosition.m_afPosition[u], 0.0f, 0.0f,
			"the v2->v3 migration moved an UNSET resume point (axis %u)", u);
	}

	// Re-encoding publishes v3, so the bytes come back as the golden with its version
	// word advanced -- the same one-word relationship the canonical-writer unit pins.
	Zenith_DataStream xReencoded;
	const Zenith_Status xReencodeStatus = ZM_SaveSchema::Write(xDecoded, xReencoded);
	ZENITH_ASSERT_TRUE(xReencodeStatus.IsOk(), "the decoded v2 state failed to re-encode");
	if (!xReencodeStatus.IsOk()) { return; }
	const uint8_t* pReencoded = static_cast<const uint8_t*>(xReencoded.GetData());
	ZENITH_ASSERT_NOT_NULL(pReencoded, "the re-encode produced no data");
	if (pReencoded == nullptr) { return; }
	ZENITH_ASSERT_EQ(ReadWireU32(pReencoded, uWIRE_SCHEMA_VERSION_OFFSET), 3u,
		"a migrated v2 save must be re-encoded at the CURRENT version, not its own");
	for (u_int uByte = uWIRE_INNER_HEADER_BYTES;
		uByte < (u_int)sizeof(auV2Golden); ++uByte)
	{
		ZENITH_ASSERT_EQ((u_int)pReencoded[uByte], (u_int)auV2Golden[uByte],
			"a migrated v2 save re-encoded with a changed module byte at %u", uByte);
	}
}

// ============================================================================
// ZM-D-223 -- a v2 save stores a body CENTRE; v3 stores FEET
// ============================================================================

// ★★ THE DEFECT THIS PINS, AND WHY NOTHING ELSE COULD SEE IT.
// `ZM_GameStateManager::CaptureWorldPosition` stores `GetBodyPosition(player)`
// verbatim -- and ZM-D-223 changed what that call MEANS. The human entity origin
// moved from the body centre to the FEET (the capsule SHAPE carries the offset
// now), so the same line that used to record a centre records feet, and every save
// written before that change holds a coordinate half a body height too high.
//
// Nothing on the wire distinguishes the two. A v2 and a v3 payload have the same
// magic, the same twelve modules, the same field order and the same lengths; a
// stored Y of 25.95666 is a legal centre AND a legal pair of feet. Read
// unmigrated, an old save loads cleanly, validates completely, and drops the player
// into the air on every continue -- which is the shape of failure a schema version
// exists to prevent, and the reason one had to move for a format that did not.
//
// ★ DRIVEN BY auV2ResumeGolden, A FROZEN LITERAL. SaveFormat.md's migration policy
// is binding: a canned blob is a compiled byte array, never regenerated from the
// codec. Running the current writer and patching its version word would prove only
// that the reader agrees with the writer -- true of any pair of broken halves --
// and would follow the writer to v4 without anyone noticing.
ZENITH_TEST(ZM_Save, MigrationV2ToV3_BodyCentreWorldPositionBecomesFeet)
{
	// (0) THE LITERAL IS WHAT IT CLAIMS TO BE, read out of the bytes themselves.
	ZENITH_ASSERT_EQ(ReadWireU32(auV2ResumeGolden, uWIRE_MAGIC_OFFSET), 0x56534D5Au,
		"the resume golden's magic");
	ZENITH_ASSERT_EQ(ReadWireU32(auV2ResumeGolden, uWIRE_SCHEMA_VERSION_OFFSET), 2u,
		"the resume golden is no longer a v2 payload -- it must never be regenerated");
	ZENITH_ASSERT_EQ(ReadWireU32(auV2ResumeGolden, uWIRE_MODULE_COUNT_OFFSET), 12u,
		"the resume golden's module count");

	// ...and its world position really is the body-centre triple this test converts,
	// read from the frozen bytes rather than assumed from the module layout.
	ZENITH_ASSERT_EQ_FLOAT(ReadWireFloat(auV2ResumeGolden, uV2_RESUME_POS_X_OFFSET),
		fMIGRATION_PROBE_X, 1.0e-4f, "the resume golden's stored X moved");
	ZENITH_ASSERT_EQ_FLOAT(ReadWireFloat(auV2ResumeGolden, uV2_RESUME_POS_Y_OFFSET),
		fMIGRATION_PROBE_CENTRE_Y, 1.0e-4f,
		"the resume golden's stored Y is not the body centre this test migrates");
	ZENITH_ASSERT_EQ_FLOAT(ReadWireFloat(auV2ResumeGolden, uV2_RESUME_POS_Z_OFFSET),
		fMIGRATION_PROBE_Z, 1.0e-4f, "the resume golden's stored Z moved");

	// (1) THE CLAIM: decoding it converts that centre to feet.
	Zenith_DataStream xInput(const_cast<uint8_t*>(auV2ResumeGolden),
		sizeof(auV2ResumeGolden));
	ZM_GameState xDecoded = MakeUntouchableDestination();
	const Zenith_Status xStatus =
		ZM_SaveSchema::Read(xInput, sizeof(auV2ResumeGolden), xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "the v2 resume golden failed to decode");
	if (!xStatus.IsOk()) { return; }
	ZENITH_ASSERT_EQ(xInput.GetCursor(), (uint64_t)sizeof(auV2ResumeGolden),
		"the decoder must consume the complete length-bounded payload");

	// Lowered by EXACTLY the frozen historical offset, landing on the marker the save
	// was taken standing on. An equality, so a shift by a DIFFERENT amount reds as
	// loudly as no shift at all.
	ZENITH_ASSERT_EQ_FLOAT(xDecoded.m_xWorldPosition.m_afPosition[1],
		fMIGRATION_PROBE_CENTRE_Y - ZM_SaveSchema::fHISTORICAL_BODY_HALF_HEIGHT,
		1.0e-4f, "a v2 save's body-centre Y was not converted to feet");

	// ...and ONLY the Y. A migration that shifted all three axes would satisfy the
	// clause above and quietly move every continue sideways.
	ZENITH_ASSERT_EQ_FLOAT(xDecoded.m_xWorldPosition.m_afPosition[0],
		fMIGRATION_PROBE_X, 1.0e-4f, "the migration moved X");
	ZENITH_ASSERT_EQ_FLOAT(xDecoded.m_xWorldPosition.m_afPosition[2],
		fMIGRATION_PROBE_Z, 1.0e-4f, "the migration moved Z");

	// (2) Everything else about the payload is untouched -- this is a coordinate
	// conversion, not a re-interpretation of the save.
	ZENITH_ASSERT_EQ(xDecoded.m_xWorldPosition.m_uSceneBuildIndex,
		ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex,
		"the migration moved the resume scene");
	ZENITH_ASSERT_EQ_FLOAT(xDecoded.m_xWorldPosition.m_fYaw, 0.0f, 1.0e-6f,
		"the migration disturbed the resume yaw");
	// Modules the migration does not own, spot-checked across the payload rather than
	// through AssertGoldenDecodedState -- that helper asserts an UNSET world position,
	// which is exactly the thing this fixture deliberately does not have.
	ZENITH_ASSERT_EQ(xDecoded.m_uMoney, 0x12345678u,
		"the migration disturbed a module it does not own (money)");
	ZENITH_ASSERT_EQ(xDecoded.m_uBadgeMask, 0x81u,
		"the migration disturbed a module it does not own (badges)");
	ZENITH_ASSERT_EQ(xDecoded.m_xParty.Count(), 1u,
		"the migration disturbed the party");
	// FALSE, and that is the codec's rule rather than the fixture's: pending whiteout
	// is transient and is forced off on every read, migrated or not. Asserted here so
	// a migration that started publishing raw decoded state would be caught.
	ZENITH_ASSERT_FALSE(xDecoded.m_bPendingWhiteout,
		"pending whiteout is transient and must not survive a migrated read");
	ZENITH_ASSERT_EQ(xDecoded.m_xCollectedGroundItems.Count(), 2u,
		"the migration disturbed the collected-prop set");

	// (3) A v3 payload is NOT touched. Without this, a migration that ran
	// unconditionally would pass every clause above while corrupting every save the
	// game writes from now on -- it would simply keep subtracting on each load.
	Zenith_DataStream xCurrent;
	ZENITH_ASSERT_TRUE(ZM_SaveSchema::Write(xDecoded, xCurrent).IsOk(),
		"the migrated state failed to re-encode");
	const uint64_t ulLength = xCurrent.GetCursor();
	Zenith_DataStream xReread(
		static_cast<uint8_t*>(const_cast<void*>(xCurrent.GetData())), ulLength);
	ZM_GameState xRedecoded = MakeUntouchableDestination();
	ZENITH_ASSERT_TRUE(ZM_SaveSchema::Read(xReread, ulLength, xRedecoded).IsOk(),
		"the re-encoded v3 payload failed to decode");
	ZENITH_ASSERT_EQ_FLOAT(xRedecoded.m_xWorldPosition.m_afPosition[1],
		xDecoded.m_xWorldPosition.m_afPosition[1], 1.0e-6f,
		"a v3 payload was migrated -- the conversion is not idempotent and a save "
		"would sink half a body every time it was loaded and re-saved");
}

// ★ THE HISTORICAL OFFSET IS FROZEN, AND THIS UNIT IS WHAT KEEPS IT THAT WAY.
//
// It asserts a LITERAL 0.9, and deliberately does NOT compare against
// fZM_HUMAN_BODY_HALF_HEIGHT. Comparing the two would pass forever -- including
// after a character-height retune that had silently changed how every save on every
// disk decodes -- because both sides would move together. That is the entire failure
// this constant exists to prevent, so the test that guards it must not be able to
// move with it either.
//
// If a retune ever makes this line LOOK wrong: it is not. The number is the
// centre-to-feet distance the v1/v2 writers actually used, fixed at the moment those
// bytes were written, and it must outlive any change to how tall a human is.
ZENITH_TEST(ZM_Save, HistoricalBodyHalfHeightIsFrozenIndependentlyOfGameplayTuning)
{
	ZENITH_ASSERT_EQ_FLOAT(ZM_SaveSchema::fHISTORICAL_BODY_HALF_HEIGHT, 0.9f, 0.0f,
		"the FROZEN v1/v2 centre-to-feet offset moved. It is a fact about bytes that "
		"already exist, not a gameplay dimension -- if the character was retuned, "
		"that is fZM_HUMAN_BODY_HALF_HEIGHT's business and this must not follow it");

	// The versions it applies to, and the one it must not.
	ZENITH_ASSERT_TRUE(
		ZM_SaveSchema::SchemaVersionStoresBodyCentre(ZM_SaveSchema::uSCHEMA_VERSION_V1),
		"v1 payloads store a body centre");
	ZENITH_ASSERT_TRUE(
		ZM_SaveSchema::SchemaVersionStoresBodyCentre(ZM_SaveSchema::uSCHEMA_VERSION_V2),
		"v2 payloads store a body centre");
	ZENITH_ASSERT_FALSE(
		ZM_SaveSchema::SchemaVersionStoresBodyCentre(
			ZM_SaveSchema::uSCHEMA_VERSION_CURRENT),
		"the CURRENT version stores feet -- migrating it would sink every save that "
		"is loaded and re-saved");
}
