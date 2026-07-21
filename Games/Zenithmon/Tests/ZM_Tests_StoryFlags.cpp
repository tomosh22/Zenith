#include "Zenith.h"

// ============================================================================
// ZM_Tests_StoryFlags -- S7 item 2 SC1 contract for the story-flag identity
// registry: the save-stable ZM_STORY_FLAG_ID enum, its compiled const table, the
// typed accessors ZM_GameState deliberately lacks, and the pure content gate.
//
// Everything here is PURE and headless: a compiled table, free functions over a
// by-value ZM_GameState, and (for the codec units) the frozen ZM_SaveSchema
// writer driven into a local owning stream. No slots, no disk, no ECS, no scene,
// no baked assets -- so no RequestSkip is needed.
//
// The load-bearing unit is Codec_EnumValueIsTheWireBitIndex. The enum VALUE *is*
// the persisted bit index in save module 4 (ZM_SaveSchema.cpp:981-993), so a
// renumbering silently re-points every shipped save at the wrong story beat. That
// unit therefore compares the produced WIRE BYTES against bit indices spelled as
// LITERALS in this file: any assertion phrased against (u_int)eFlag would move
// consistently with the enum under a renumbering and could never fail.
//
// The codec is read here, never written to. Nothing in this file touches
// ZM_SaveSchema's frozen surface.
// ============================================================================

#include <cstring>   // strcmp (debug-name distinctness), memcpy/memcmp (the no-mutation snapshot)

#include "Core/Zenith_ErrorCode.h"
#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"
#include "Zenithmon/Source/Core/ZM_SaveSchema.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"

namespace
{
	// ---- ZMSV framing, restated from ZM_SaveSchema.cpp so this file can WALK to
	// module 4 rather than hardcode a byte offset. Module 4's offset depends on the
	// party / box / dex payload widths ahead of it, so a literal offset would be
	// silently wrong the moment any earlier module's content changed.
	constexpr u_int uWIRE_INNER_HEADER_BYTES = 12u;   // magic + schemaVersion + moduleCount
	constexpr u_int uWIRE_MODULE_HEADER_BYTES = 12u;  // id + version + payloadLength
	constexpr u_int uWIRE_MODULE_COUNT = 11u;
	constexpr u_int uWIRE_STORY_MODULE_ID = 4u;       // ZM_SaveSchema.cpp:19, uMODULE_STORY
	constexpr u_int uWIRE_STORY_COUNT_BYTES = 2u;     // the u16 "highest set index + 1" prefix

	// A sanity ceiling on module 4's whole payload, spelled as a LITERAL rather than
	// derived from ZM_STORY_FLAG_COUNT: derived it would move with the registry and
	// could never say "this got too expensive". It is the only RUNTIME bound on what
	// module 4 costs in EVERY save, and it trips once the registry grows past 64
	// flags. 10 bytes == the u16 count plus room for those 64; going past that is a
	// deliberate decision, not something a careless append gets to make.
	//
	// It is NOT what catches a TRAILING sparse assignment (the last enumerator set to
	// `= 4000u`): s_axFlags has a DEDUCED bound plus a row-count static_assert
	// (ZM_StoryFlags.cpp:28 and :38), so that shape is a COMPILE error and no unit in
	// this file ever runs against it.
	constexpr u_int uWIRE_STORY_MODULE_SANE_BYTES = 10u;

	// The registry's wire contract, spelled OUT BY HAND. m_uWireBitIndex is a literal
	// and is NEVER derived from m_eFlag -- that is the whole point of this table.
	struct ZM_ExpectedWireBit
	{
		ZM_STORY_FLAG_ID m_eFlag;
		u_int            m_uWireBitIndex;
	};

	const ZM_ExpectedWireBit axEXPECTED_WIRE_BITS[] =
	{
		{ ZM_STORY_FLAG_INTRO_LEFT_HOME,  0u },
		{ ZM_STORY_FLAG_MET_PROFESSOR,    1u },
		{ ZM_STORY_FLAG_STARTER_RECEIVED, 2u },
		{ ZM_STORY_FLAG_WARDEN_CLEARED,   3u },
		{ ZM_STORY_FLAG_ROUTE1_OPEN,      4u },
		{ ZM_STORY_FLAG_GYM1_DEFEATED,    5u },
	};
	constexpr u_int uEXPECTED_WIRE_BIT_COUNT =
		(u_int)(sizeof(axEXPECTED_WIRE_BITS) / sizeof(axEXPECTED_WIRE_BITS[0]));

	struct ZM_StoryModuleSpan
	{
		const u_int8* m_puPayload = nullptr;
		u_int         m_uLength = 0u;
		bool          m_bFound = false;
	};

	u_int ReadWireU16(const u_int8* puBytes)
	{
		return (u_int)puBytes[0] | ((u_int)puBytes[1] << 8u);
	}

	u_int ReadWireU32(const u_int8* puBytes)
	{
		return (u_int)puBytes[0]
			| ((u_int)puBytes[1] << 8u)
			| ((u_int)puBytes[2] << 16u)
			| ((u_int)puBytes[3] << 24u);
	}

	// Walks the module chain from the inner header to module 4. Every structural
	// failure REPORTS and returns unfound, because the assert macros continue: a
	// caller that walked on with a null payload would crash the whole boot run.
	ZM_StoryModuleSpan FindStoryModule(const u_int8* puBlob, u_int64 ulLength)
	{
		ZM_StoryModuleSpan xSpan;
		ZENITH_ASSERT_NOT_NULL(puBlob, "the encoded payload has no data");
		if (puBlob == nullptr || ulLength < (u_int64)uWIRE_INNER_HEADER_BYTES)
		{
			ZENITH_ASSERT_TRUE(false, "the encoded payload is shorter than the ZMSV header");
			return xSpan;
		}

		u_int64 ulCursor = (u_int64)uWIRE_INNER_HEADER_BYTES;
		for (u_int uModule = 0u; uModule < uWIRE_MODULE_COUNT; ++uModule)
		{
			if (ulCursor + (u_int64)uWIRE_MODULE_HEADER_BYTES > ulLength)
			{
				ZENITH_ASSERT_TRUE(false, "the payload ended before module header %u", uModule);
				return xSpan;
			}
			const u_int uId = ReadWireU32(puBlob + ulCursor);
			const u_int uPayloadLength = ReadWireU32(puBlob + ulCursor + 8u);
			const u_int64 ulPayload = ulCursor + (u_int64)uWIRE_MODULE_HEADER_BYTES;
			if (ulPayload + (u_int64)uPayloadLength > ulLength)
			{
				ZENITH_ASSERT_TRUE(false, "module %u claims %u bytes past the end of the payload",
					uId, uPayloadLength);
				return xSpan;
			}
			if (uId == uWIRE_STORY_MODULE_ID)
			{
				xSpan.m_puPayload = puBlob + ulPayload;
				xSpan.m_uLength = uPayloadLength;
				xSpan.m_bFound = true;
				return xSpan;
			}
			ulCursor = ulPayload + (u_int64)uPayloadLength;
		}

		ZENITH_ASSERT_TRUE(false, "the payload carries no module %u", uWIRE_STORY_MODULE_ID);
		return xSpan;
	}

	// Encodes xState with the FROZEN codec into xStreamOut (which the caller owns, so
	// the returned span stays valid) and hands back module 4's span.
	ZM_StoryModuleSpan EncodeAndFindStoryModule(const ZM_GameState& xState,
		Zenith_DataStream& xStreamOut)
	{
		const Zenith_Status xStatus = ZM_SaveSchema::Write(xState, xStreamOut);
		ZENITH_ASSERT_TRUE(xStatus.IsOk(), "the frozen codec refused the fixture state (error %u)",
			(u_int)xStatus.Error());
		if (!xStatus.IsOk())
		{
			return ZM_StoryModuleSpan();
		}
		return FindStoryModule(static_cast<const u_int8*>(xStreamOut.GetData()),
			xStreamOut.GetCursor());
	}

	// Fixtures are built through ZM_StoryFlagSet's OWN raw-index setter, never through
	// ZM_SetStoryFlag: a gate unit whose fixture was populated by the accessor it is
	// standing next to would inherit that accessor's bugs and agree with them.
	ZM_StoryFlagSet MakeAllRegisteredFlagsSet()
	{
		ZM_StoryFlagSet xFlags;
		for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
		{
			xFlags.Set(u, true);
		}
		return xFlags;
	}

	// Additionally sets the bit at the SENTINEL's numeric index. Without this fixture a
	// gate that mistook ZM_STORY_FLAG_NONE for a real index would hide behind that bit
	// happening to be clear in every other fixture.
	ZM_StoryFlagSet MakeSentinelIndexAlsoSet()
	{
		ZM_StoryFlagSet xFlags = MakeAllRegisteredFlagsSet();
		xFlags.Set((u_int)ZM_STORY_FLAG_NONE, true);
		return xFlags;
	}
}

// ---- The registry table -----------------------------------------------------

// Row index == m_eId. The enum value IS the wire bit index, so a row inserted out
// of order, duplicated, or omitted mis-addresses every flag past it.
ZENITH_TEST(ZM_Story, Registry_IdEqualsRowIndexForEveryFlag)
{
	ZENITH_ASSERT_GT((u_int)ZM_STORY_FLAG_COUNT, 0u,
		"an empty registry makes every walk in this suite vacuous");
	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_GetStoryFlagInfo((ZM_STORY_FLAG_ID)u).m_eId, u,
			"story flag row %u carries a mismatched m_eId", u);
	}

	// ...and the row accessor is TOTAL at the far end of the table too: an id the
	// registry does not name yields the shared UNKNOWN row instead of reading past
	// s_axFlags. This clause is REACHABLE ONLY because the accessor does not assert
	// -- a Zenith_Assert there would break the process in every configuration
	// (Zenith.h:138 defines ZENITH_ASSERT unconditionally) and take the rest of the
	// boot unit run with it. Both calls log a non-fatal "[Gameplay] ... not a
	// registered story flag" line; those lines are EXPECTED output of this unit.
	const ZM_STORY_FLAG_ID aeUNREGISTERED[] = { ZM_STORY_FLAG_NONE, (ZM_STORY_FLAG_ID)9999u };
	for (const ZM_STORY_FLAG_ID eUnregistered : aeUNREGISTERED)
	{
		const ZM_StoryFlagInfo& xRow = ZM_GetStoryFlagInfo(eUnregistered);
		ZENITH_ASSERT_EQ((u_int)xRow.m_eId, (u_int)ZM_STORY_FLAG_NONE,
			"id %u must yield the UNKNOWN row, not a table read", (u_int)eUnregistered);
		ZENITH_ASSERT_NOT_NULL(xRow.m_szDebugName,
			"the UNKNOWN row handed back for id %u has no debug name", (u_int)eUnregistered);
		if (xRow.m_szDebugName != nullptr)
		{
			ZENITH_ASSERT_TRUE(strcmp(xRow.m_szDebugName, "UNKNOWN") == 0,
				"id %u named itself '%s' -- it read a real row", (u_int)eUnregistered,
				xRow.m_szDebugName);
		}
	}
}

ZENITH_TEST(ZM_Story, Registry_EveryFlagHasNonEmptyUniqueDebugName)
{
	ZENITH_ASSERT_GT((u_int)ZM_STORY_FLAG_COUNT, 0u,
		"an empty registry makes the walks below vacuous");

	// Null / empty FIRST, across every row, before any strcmp: the assert macros
	// record and CONTINUE, so an interleaved single loop would strcmp a row that had
	// not been null-checked yet and turn a named failure into a hard UB crash during
	// units-at-boot. s_axFlags has a deduced bound, so it cannot carry a
	// value-initialised hole -- but a row whose name column is hand-written as nullptr
	// (or as "") lands in exactly that case.
	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		const char* szName = ZM_GetStoryFlagInfo((ZM_STORY_FLAG_ID)u).m_szDebugName;
		ZENITH_ASSERT_NOT_NULL(szName, "story flag %u has no debug name", u);
		if (szName != nullptr)
		{
			ZENITH_ASSERT_TRUE(szName[0] != '\0', "story flag %u has an EMPTY debug name", u);
		}
	}

	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		const char* szA = ZM_GetStoryFlagInfo((ZM_STORY_FLAG_ID)u).m_szDebugName;
		for (u_int v = u + 1u; v < (u_int)ZM_STORY_FLAG_COUNT; ++v)
		{
			const char* szB = ZM_GetStoryFlagInfo((ZM_STORY_FLAG_ID)v).m_szDebugName;
			if ((szA == nullptr) || (szB == nullptr))
			{
				continue;   // already reported above
			}
			ZENITH_ASSERT_FALSE(strcmp(szA, szB) == 0,
				"story flags %u and %u share the debug name '%s' -- a copy-pasted row", u, v, szA);
		}
	}
}

ZENITH_TEST(ZM_Story, Registry_CountMatchesEnumAndFitsSaveCeiling)
{
	ZENITH_ASSERT_EQ(ZM_GetStoryFlagCount(), (u_int)ZM_STORY_FLAG_COUNT,
		"the accessor and the enum disagree about how many flags exist");
	ZENITH_ASSERT_GT((u_int)ZM_STORY_FLAG_COUNT, 0u, "the registry names no flags at all");
	ZENITH_ASSERT_LE((u_int)ZM_STORY_FLAG_COUNT, uZM_MAX_STORY_FLAGS,
		"the registry has outgrown the model's fixed 4096-bit story-flag ceiling");
	ZENITH_ASSERT_EQ((u_int)ZM_STORY_FLAG_NONE, (u_int)ZM_STORY_FLAG_COUNT,
		"the sentinel must sit exactly one past the last registered flag");
}

// Indices are allocated DENSELY FROM ZERO. Module 4 encodes highest-set-index+1 and
// ceil(count/8) bytes, so one hand-assigned sparse value adds ~(index/8) bytes to
// EVERY save forever and cannot be reclaimed without a versioned codec change.
//
// What this unit covers is the sparse shapes that still COMPILE. A TRAILING
// `= 4000u` is not one of them -- s_axFlags has a deduced bound and a row-count
// static_assert (ZM_StoryFlags.cpp:28 and :38), so that never builds. But C++
// enumerator values need not be monotonic: `ZM_STORY_FLAG_ROUTE1_OPEN = 4000u` with
// GYM1_DEFEATED still `= 5u` after it leaves COUNT at 6 and the table at six rows,
// compiles clean, and is caught only at RUNTIME. A numeric gap padded with a
// placeholder row behaves the same way.
//
// Both walks below are logically IMPLIED by Registry_IdEqualsRowIndexForEveryFlag --
// id == row index for every row already forces both the maximum and surjectivity.
// They stay because they name the COST directly, so the failure reads "the
// allocation is not dense from zero" rather than "row 4 has a mismatched id"; but
// neither walk covers a registry shape that unit misses.
ZENITH_TEST(ZM_Story, Registry_IndicesAreDenseFromZero)
{
	ZENITH_ASSERT_GT((u_int)ZM_STORY_FLAG_COUNT, 0u,
		"an empty registry makes the walks below vacuous");

	u_int uMaxId = 0u;
	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		const u_int uId = (u_int)ZM_GetStoryFlagInfo((ZM_STORY_FLAG_ID)u).m_eId;
		if (uId > uMaxId) { uMaxId = uId; }
	}
	ZENITH_ASSERT_EQ(uMaxId, (u_int)ZM_STORY_FLAG_COUNT - 1u,
		"the highest registered flag index must be COUNT-1 -- a sparse index costs "
		"every save ~(index/8) bytes forever");

	for (u_int uWanted = 0u; uWanted < (u_int)ZM_STORY_FLAG_COUNT; ++uWanted)
	{
		bool bFound = false;
		for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT && !bFound; ++u)
		{
			bFound = ((u_int)ZM_GetStoryFlagInfo((ZM_STORY_FLAG_ID)u).m_eId == uWanted);
		}
		ZENITH_ASSERT_TRUE(bFound,
			"no registered flag owns index %u -- the allocation is not dense from zero",
			uWanted);
	}
}

// ---- The wire contract ------------------------------------------------------

// THE load-bearing unit of SC1. For each NAMED flag: set only that flag, encode with
// the frozen codec, walk to module 4, and assert the bit landed at the index this
// file spells as a LITERAL. Nothing here is phrased in terms of (u_int)eFlag, so a
// renumbering of ZM_STORY_FLAG_ID -- which would silently re-point every shipped
// save at the wrong story beat -- reds this and nothing else.
ZENITH_TEST(ZM_Story, Codec_EnumValueIsTheWireBitIndex)
{
	ZENITH_ASSERT_EQ(uEXPECTED_WIRE_BIT_COUNT, (u_int)ZM_STORY_FLAG_COUNT,
		"the literal wire-bit table must name EVERY registered flag; a flag added to "
		"the enum without a row here would ship with its wire index unpinned");

	for (u_int u = 0u; u < uEXPECTED_WIRE_BIT_COUNT; ++u)
	{
		const ZM_ExpectedWireBit& xCase = axEXPECTED_WIRE_BITS[u];

		// A default-constructed state has every flag byte zero, so "only this flag" is
		// true by construction rather than by a clearing call that could itself be wrong.
		ZM_GameState xState;
		ZENITH_ASSERT_TRUE(ZM_SetStoryFlag(xState, xCase.m_eFlag, true),
			"the typed setter refused registered flag '%s'",
			ZM_StoryFlagName(xCase.m_eFlag));

		Zenith_DataStream xStream;
		const ZM_StoryModuleSpan xSpan = EncodeAndFindStoryModule(xState, xStream);
		if (!xSpan.m_bFound)
		{
			continue;   // already reported
		}

		const u_int uExpectedBit = xCase.m_uWireBitIndex;
		const u_int uExpectedFlagBytes = (uExpectedBit / 8u) + 1u;
		ZENITH_ASSERT_EQ(xSpan.m_uLength, uWIRE_STORY_COUNT_BYTES + uExpectedFlagBytes,
			"module 4 for '%s' is %u bytes; the u16 count plus ceil((%u+1)/8) flag bytes "
			"is %u", ZM_StoryFlagName(xCase.m_eFlag), xSpan.m_uLength, uExpectedBit,
			uWIRE_STORY_COUNT_BYTES + uExpectedFlagBytes);
		if (xSpan.m_uLength < uWIRE_STORY_COUNT_BYTES)
		{
			continue;   // reported above; walking on would read off the payload
		}

		ZENITH_ASSERT_EQ(ReadWireU16(xSpan.m_puPayload), uExpectedBit + 1u,
			"module 4 encodes highest-set-index+1, so '%s' must report %u",
			ZM_StoryFlagName(xCase.m_eFlag), uExpectedBit + 1u);

		const u_int uFlagBytes = xSpan.m_uLength - uWIRE_STORY_COUNT_BYTES;
		for (u_int uByte = 0u; uByte < uFlagBytes; ++uByte)
		{
			const u_int uExpected = (uByte == (uExpectedBit / 8u))
				? (1u << (uExpectedBit % 8u))
				: 0u;
			ZENITH_ASSERT_EQ((u_int)xSpan.m_puPayload[uWIRE_STORY_COUNT_BYTES + uByte], uExpected,
				"'%s' must occupy wire bit %u and NOTHING else; flag byte %u is 0x%02X",
				ZM_StoryFlagName(xCase.m_eFlag), uExpectedBit, uByte,
				(u_int)xSpan.m_puPayload[uWIRE_STORY_COUNT_BYTES + uByte]);
		}
	}
}

// The cost side of the dense-allocation policy: with the HIGHEST registered flag set
// (the worst case), module 4 is still the u16 count plus a handful of bytes, and the
// flag still survives a full round trip through the frozen codec.
ZENITH_TEST(ZM_Story, Codec_DenseAllocationKeepsModuleFourMinimal)
{
	ZENITH_ASSERT_GT((u_int)ZM_STORY_FLAG_COUNT, 0u, "an empty registry has no highest flag");
	const ZM_STORY_FLAG_ID eHighest = (ZM_STORY_FLAG_ID)((u_int)ZM_STORY_FLAG_COUNT - 1u);

	ZM_GameState xState;
	ZENITH_ASSERT_TRUE(ZM_SetStoryFlag(xState, eHighest, true),
		"the typed setter refused the highest registered flag");

	Zenith_DataStream xStream;
	const ZM_StoryModuleSpan xSpan = EncodeAndFindStoryModule(xState, xStream);
	const u_int64 ulWritten = xStream.GetCursor();
	if (!xSpan.m_bFound)
	{
		return;   // already reported
	}

	// The LITERAL ceiling is the runtime bound on per-save cost -- it has teeth once
	// the registry passes 64 flags, which nothing else here bounds; the derived
	// equality below only proves the writer is not padding to a fixed width.
	ZENITH_ASSERT_LE(xSpan.m_uLength, uWIRE_STORY_MODULE_SANE_BYTES,
		"module 4 costs %u bytes in EVERY save -- a flag index was allocated sparsely",
		xSpan.m_uLength);
	ZENITH_ASSERT_EQ(xSpan.m_uLength,
		uWIRE_STORY_COUNT_BYTES + (((u_int)ZM_STORY_FLAG_COUNT + 7u) / 8u),
		"module 4 must be the u16 count plus exactly ceil(COUNT/8) flag bytes");
	if (xSpan.m_uLength >= uWIRE_STORY_COUNT_BYTES)
	{
		ZENITH_ASSERT_EQ(ReadWireU16(xSpan.m_puPayload), (u_int)ZM_STORY_FLAG_COUNT,
			"the highest registered flag makes the encoded count exactly COUNT");
	}

	// Read back into a SEPARATE state: comparing against the object that was written
	// from would only prove an object equals itself.
	xStream.SetCursor(0u);
	ZM_GameState xDecoded;
	const Zenith_Status xStatus = ZM_SaveSchema::Read(xStream, ulWritten, xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "the encoded payload did not decode (error %u)",
		(u_int)xStatus.Error());
	if (!xStatus.IsOk())
	{
		return;
	}
	ZENITH_ASSERT_TRUE(ZM_IsStoryFlagSet(xDecoded, eHighest),
		"the highest registered flag did not survive the round trip");
	ZENITH_ASSERT_EQ(xDecoded.m_xStoryFlags.Count(), 1u,
		"the round trip invented story flags that were never set");
}

// ---- Names ------------------------------------------------------------------

ZENITH_TEST(ZM_Story, Name_OutOfRangeReturnsUnknownAndNeverNull)
{
	// COUNT *is* the sentinel, so it is named rather than unknown (pinned by
	// Name_NoneSentinelNamesSafely). What matters here is only that it never indexes
	// the table and never returns null.
	const char* szAtCount = ZM_StoryFlagName((ZM_STORY_FLAG_ID)(u_int)ZM_STORY_FLAG_COUNT);
	ZENITH_ASSERT_NOT_NULL(szAtCount, "the name formatter returned null for COUNT");
	if (szAtCount != nullptr)
	{
		ZENITH_ASSERT_TRUE(szAtCount[0] != '\0', "the name formatter returned an EMPTY string for COUNT");
	}

	ZENITH_ASSERT_STREQ(ZM_StoryFlagName((ZM_STORY_FLAG_ID)((u_int)ZM_STORY_FLAG_COUNT + 9u)),
		"UNKNOWN", "an id past the sentinel must be UNKNOWN, never a table read");
	ZENITH_ASSERT_STREQ(ZM_StoryFlagName((ZM_STORY_FLAG_ID)9999u), "UNKNOWN",
		"a garbage id must be UNKNOWN, never a table read");
}

ZENITH_TEST(ZM_Story, Name_NoneSentinelNamesSafely)
{
	ZENITH_ASSERT_STREQ(ZM_StoryFlagName(ZM_STORY_FLAG_NONE), "NONE",
		"the sentinel is not a real row and must name itself, not the row it collides with");
}

// ---- Typed accessors --------------------------------------------------------

// Set / read / clear over every registered flag, cross-checking that setting one flag
// leaves every OTHER one clear. The cross-check is what catches an off-by-one: an
// accessor that wrote index+1 would still answer true for the flag it just set.
ZENITH_TEST(ZM_Story, Accessor_SetThenIsSetRoundTripsEveryRegisteredFlag)
{
	ZENITH_ASSERT_GT((u_int)ZM_STORY_FLAG_COUNT, 0u, "an empty registry makes this walk vacuous");

	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		const ZM_STORY_FLAG_ID eFlag = (ZM_STORY_FLAG_ID)u;
		ZM_GameState xState;

		ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, eFlag),
			"'%s' is already set on a fresh state", ZM_StoryFlagName(eFlag));
		ZENITH_ASSERT_TRUE(ZM_SetStoryFlag(xState, eFlag, true),
			"the setter refused registered flag '%s'", ZM_StoryFlagName(eFlag));
		ZENITH_ASSERT_TRUE(ZM_IsStoryFlagSet(xState, eFlag),
			"'%s' did not read back as set", ZM_StoryFlagName(eFlag));

		// Exactly ONE bit anywhere in the 4096-bit set, not merely "the one we asked
		// about" -- this is the clause that catches a setter that smears extra bits.
		ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), 1u,
			"setting '%s' set %u flags in total", ZM_StoryFlagName(eFlag),
			xState.m_xStoryFlags.Count());
		for (u_int v = 0u; v < (u_int)ZM_STORY_FLAG_COUNT; ++v)
		{
			if (v == u) { continue; }
			ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, (ZM_STORY_FLAG_ID)v),
				"setting '%s' also set '%s'", ZM_StoryFlagName(eFlag),
				ZM_StoryFlagName((ZM_STORY_FLAG_ID)v));
		}

		ZENITH_ASSERT_TRUE(ZM_SetStoryFlag(xState, eFlag, false),
			"the setter refused to clear '%s'", ZM_StoryFlagName(eFlag));
		ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, eFlag),
			"'%s' survived being cleared", ZM_StoryFlagName(eFlag));
	}

	// The raw-flag-set overload pair carries the same contract for callers that hold a
	// ZM_StoryFlagSet directly (the gate's argument type).
	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		const ZM_STORY_FLAG_ID eFlag = (ZM_STORY_FLAG_ID)u;
		ZM_StoryFlagSet xFlags;
		ZENITH_ASSERT_TRUE(ZM_SetStoryFlag(xFlags, eFlag, true),
			"the flag-set overload refused '%s'", ZM_StoryFlagName(eFlag));
		ZENITH_ASSERT_TRUE(ZM_IsStoryFlagSet(xFlags, eFlag),
			"the flag-set overload did not read '%s' back", ZM_StoryFlagName(eFlag));
		ZENITH_ASSERT_EQ(xFlags.Count(), 1u,
			"the flag-set overload set %u flags for '%s'", xFlags.Count(), ZM_StoryFlagName(eFlag));
	}
}

// bSet must actually be read. A setter that ignores it and always sets would pass
// every "set then read" unit above and only fail here.
ZENITH_TEST(ZM_Story, Accessor_ClearActuallyClears)
{
	ZENITH_ASSERT_GT((u_int)ZM_STORY_FLAG_COUNT, 0u, "an empty registry makes this walk vacuous");

	ZM_GameState xState;
	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		ZM_SetStoryFlag(xState, (ZM_STORY_FLAG_ID)u, true);
	}
	ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), (u_int)ZM_STORY_FLAG_COUNT,
		"the fixture did not populate -- the clears below would be asserted against "
		"a state that was never dirty");

	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		const ZM_STORY_FLAG_ID eFlag = (ZM_STORY_FLAG_ID)u;
		ZENITH_ASSERT_TRUE(ZM_SetStoryFlag(xState, eFlag, false),
			"the setter refused to clear '%s'", ZM_StoryFlagName(eFlag));
		ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, eFlag),
			"'%s' is still set after being cleared", ZM_StoryFlagName(eFlag));
		ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), (u_int)ZM_STORY_FLAG_COUNT - (u + 1u),
			"clearing '%s' changed the wrong number of flags", ZM_StoryFlagName(eFlag));
	}
	ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), 0u, "clearing every flag left some set");
}

// A rejected write must be a NO-OP, proven by memcmp over the whole raw storage rather
// than by re-reading the flag that was asked about. ZM_STORY_FLAG_NONE numerically
// EQUALS ZM_STORY_FLAG_COUNT, so an accessor that forwards it unchecked would quietly
// set a real bit one past the registry.
ZENITH_TEST(ZM_Story, Accessor_RejectsNoneAndOutOfRangeWithoutMutation)
{
	ZENITH_ASSERT_GT((u_int)ZM_STORY_FLAG_COUNT, 0u, "an empty registry cannot build a fixture");

	ZM_GameState xState;
	// A deliberately non-trivial fixture: an all-zero snapshot would memcmp-match a
	// storage buffer that had been wiped as well as one that was left alone.
	ZM_SetStoryFlag(xState, (ZM_STORY_FLAG_ID)0u, true);
	ZM_SetStoryFlag(xState, (ZM_STORY_FLAG_ID)((u_int)ZM_STORY_FLAG_COUNT - 1u), true);
	ZENITH_ASSERT_GT(xState.m_xStoryFlags.Count(), 0u,
		"the fixture is empty, so the memcmp below would pass over untouched zeroes");

	u_int8 auSnapshot[uZM_STORY_FLAG_BYTE_COUNT];
	memcpy(auSnapshot, xState.m_xStoryFlags.m_auFlags, sizeof(auSnapshot));

	const ZM_STORY_FLAG_ID aeREJECTED[] =
	{
		ZM_STORY_FLAG_NONE,
		(ZM_STORY_FLAG_ID)((u_int)ZM_STORY_FLAG_COUNT + 1u),
		(ZM_STORY_FLAG_ID)9999u,
	};
	for (const ZM_STORY_FLAG_ID eRejected : aeREJECTED)
	{
		ZENITH_ASSERT_FALSE(ZM_SetStoryFlag(xState, eRejected, true),
			"the setter accepted id %u", (u_int)eRejected);
		ZENITH_ASSERT_FALSE(ZM_SetStoryFlag(xState, eRejected, false),
			"the clearer accepted id %u", (u_int)eRejected);
		ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, eRejected),
			"the reader answered true for id %u", (u_int)eRejected);
	}

	ZENITH_ASSERT_TRUE(
		memcmp(auSnapshot, xState.m_xStoryFlags.m_auFlags, sizeof(auSnapshot)) == 0,
		"a rejected story-flag write mutated the flag storage");

	// The raw-flag-set overload pair carries the same guarantee.
	ZM_StoryFlagSet xFlags;
	xFlags.Set(0u, true);
	u_int8 auFlagSnapshot[uZM_STORY_FLAG_BYTE_COUNT];
	memcpy(auFlagSnapshot, xFlags.m_auFlags, sizeof(auFlagSnapshot));
	for (const ZM_STORY_FLAG_ID eRejected : aeREJECTED)
	{
		ZENITH_ASSERT_FALSE(ZM_SetStoryFlag(xFlags, eRejected, true),
			"the flag-set setter accepted id %u", (u_int)eRejected);
		ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xFlags, eRejected),
			"the flag-set reader answered true for id %u", (u_int)eRejected);
	}
	ZENITH_ASSERT_TRUE(memcmp(auFlagSnapshot, xFlags.m_auFlags, sizeof(auFlagSnapshot)) == 0,
		"a rejected write through the flag-set overload mutated the storage");
}

// ---- The content gate -------------------------------------------------------

// ZM_STORY_FLAG_NONE means "ungated" and must pass under BOTH polarities and under
// every flag state -- including one where the sentinel's own numeric index is set, so
// a gate that mistook it for a real flag cannot hide behind a clear bit.
ZENITH_TEST(ZM_Story, Gate_NoneFlagPassesRegardlessOfFlagState)
{
	const ZM_StoryFlagSet xClear;
	const ZM_StoryFlagSet xAllSet = MakeAllRegisteredFlagsSet();
	const ZM_StoryFlagSet xSentinelSet = MakeSentinelIndexAlsoSet();

	const ZM_StoryGate xRequireSet{ ZM_STORY_FLAG_NONE, true };
	const ZM_StoryGate xRequireClear{ ZM_STORY_FLAG_NONE, false };

	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(xRequireSet, xClear),
		"an ungated gate must pass with no flags set");
	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(xRequireSet, xAllSet),
		"an ungated gate must pass with every registered flag set");
	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(xRequireSet, xSentinelSet),
		"an ungated gate must not consult the sentinel's numeric bit");
	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(xRequireClear, xClear),
		"the require-CLEAR polarity is equally unconditional for NONE");
	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(xRequireClear, xAllSet),
		"the require-CLEAR polarity is equally unconditional for NONE");
	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(xRequireClear, xSentinelSet),
		"the require-CLEAR polarity must not read the sentinel's numeric bit either");
}

ZENITH_TEST(ZM_Story, Gate_RequireSetOnlyPassesWhenSet)
{
	const ZM_StoryGate xGate{ ZM_STORY_FLAG_WARDEN_CLEARED, true };

	const ZM_StoryFlagSet xClear;
	ZENITH_ASSERT_FALSE(ZM_StoryGatePasses(xGate, xClear),
		"a require-SET gate must FAIL while its flag is clear");

	ZM_StoryFlagSet xSet;
	xSet.Set((u_int)ZM_STORY_FLAG_WARDEN_CLEARED, true);
	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(xGate, xSet),
		"a require-SET gate must PASS once its flag is set");
}

// The "before you did X" shape: content that is only available while a flag is still
// clear. m_bRequireSet has to actually be read for this to differ from the unit above.
ZENITH_TEST(ZM_Story, Gate_RequireClearOnlyPassesWhenClear)
{
	const ZM_StoryGate xGate{ ZM_STORY_FLAG_WARDEN_CLEARED, false };

	const ZM_StoryFlagSet xClear;
	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(xGate, xClear),
		"a require-CLEAR gate must PASS while its flag is clear");

	ZM_StoryFlagSet xSet;
	xSet.Set((u_int)ZM_STORY_FLAG_WARDEN_CLEARED, true);
	ZENITH_ASSERT_FALSE(ZM_StoryGatePasses(xGate, xSet),
		"a require-CLEAR gate must FAIL once its flag is set");
}

// A mis-authored gate must LOCK content, never open it. The require-CLEAR case is the
// one that matters: written as `IsSet(idx) == m_bRequireSet`, an out-of-range id reads
// false, false == false, and the gate OPENS.
//
// This unit drives the gate's unregistered-id branch FOUR times on purpose, so four
// non-fatal "[Gameplay] ... UNREGISTERED story flag" error lines in the boot log are
// this unit working, not a failure. That branch must never be a Zenith_Assert: asserts
// break the process in EVERY configuration here (Zenith.h:138 defines ZENITH_ASSERT
// unconditionally), so one placed there would kill the boot unit run at this line and
// the whole gate would be lost -- which is exactly what happened once already.
ZENITH_TEST(ZM_Story, Gate_OutOfRangeFailsClosed)
{
	const ZM_StoryFlagSet xClear;
	const ZM_StoryFlagSet xAllSet = MakeAllRegisteredFlagsSet();

	const ZM_STORY_FLAG_ID eGarbage = (ZM_STORY_FLAG_ID)((u_int)ZM_STORY_FLAG_COUNT + 7u);
	const ZM_StoryGate xRequireSet{ eGarbage, true };
	const ZM_StoryGate xRequireClear{ eGarbage, false };

	ZENITH_ASSERT_FALSE(ZM_StoryGatePasses(xRequireSet, xClear),
		"an out-of-range require-SET gate must fail closed");
	ZENITH_ASSERT_FALSE(ZM_StoryGatePasses(xRequireSet, xAllSet),
		"an out-of-range require-SET gate must fail closed");
	ZENITH_ASSERT_FALSE(ZM_StoryGatePasses(xRequireClear, xClear),
		"an out-of-range require-CLEAR gate must fail CLOSED -- this is the case an "
		"`IsSet(idx) == m_bRequireSet` guard gets backwards");
	ZENITH_ASSERT_FALSE(ZM_StoryGatePasses(xRequireClear, xAllSet),
		"an out-of-range require-CLEAR gate must fail closed");
}

// A data row that GAINS a gate field must keep its old behaviour by construction. If a
// default member initialiser ever names a real flag, every existing ungated row in the
// tree starts gating silently.
ZENITH_TEST(ZM_Story, Gate_DefaultConstructedGateIsUnconditional)
{
	const ZM_StoryGate xGate;
	const ZM_StoryFlagSet xClear;
	const ZM_StoryFlagSet xAllSet = MakeAllRegisteredFlagsSet();

	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(xGate, xClear),
		"a default-constructed gate must pass with no flags set");
	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(xGate, xAllSet),
		"a default-constructed gate must pass with every flag set");
	ZENITH_ASSERT_EQ((u_int)xGate.m_eFlag, (u_int)ZM_STORY_FLAG_NONE,
		"the default gate names no flag");
}

// ---- Milestones -------------------------------------------------------------

// SC3's autosave hangs off this predicate. An empty milestone set makes that autosave
// dead code; a predicate that answers true for everything makes it fire on every beat.
ZENITH_TEST(ZM_Story, Milestone_IsTotalAndNamesAtLeastOneFlag)
{
	ZENITH_ASSERT_GT((u_int)ZM_STORY_FLAG_COUNT, 1u,
		"the bounds below need at least two registered flags to be meaningful");

	ZENITH_ASSERT_FALSE(ZM_IsMilestoneStoryFlag(ZM_STORY_FLAG_NONE),
		"the sentinel is not a milestone");
	ZENITH_ASSERT_FALSE(ZM_IsMilestoneStoryFlag(
		(ZM_STORY_FLAG_ID)((u_int)ZM_STORY_FLAG_COUNT + 7u)),
		"an out-of-range id is not a milestone");
	ZENITH_ASSERT_FALSE(ZM_IsMilestoneStoryFlag((ZM_STORY_FLAG_ID)9999u),
		"a garbage id is not a milestone");

	u_int uMilestones = 0u;
	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		if (ZM_IsMilestoneStoryFlag((ZM_STORY_FLAG_ID)u)) { ++uMilestones; }
	}
	ZENITH_ASSERT_GT(uMilestones, 0u,
		"no registered flag is a milestone -- SC3's autosave would never fire");
	ZENITH_ASSERT_LT(uMilestones, (u_int)ZM_STORY_FLAG_COUNT,
		"every registered flag is a milestone -- the predicate is unbounded and the "
		"autosave would fire on every story beat");

	// The SC1 demonstration flag is the one SC3 is written against.
	ZENITH_ASSERT_TRUE(ZM_IsMilestoneStoryFlag(ZM_STORY_FLAG_WARDEN_CLEARED),
		"clearing the warden is a milestone");
}

// ---- The starter contract ---------------------------------------------------

// A new game starts with the whole story unwritten. If a future starter pre-sets a
// flag, that is a save-visible decision and must be made deliberately here first.
ZENITH_TEST(ZM_Story, StarterState_HasNoRegisteredFlagSet)
{
	ZENITH_ASSERT_GT((u_int)ZM_STORY_FLAG_COUNT, 0u, "an empty registry makes this walk vacuous");

	const ZM_GameState xStarter = ZM_MakeStarterGameState();
	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xStarter, (ZM_STORY_FLAG_ID)u),
			"the starter state ships with '%s' already set",
			ZM_StoryFlagName((ZM_STORY_FLAG_ID)u));
	}
	// Stronger than the walk: catches a bit set OUTSIDE the registry, which would still
	// cost module 4 its ceil(index/8) bytes in every save.
	ZENITH_ASSERT_EQ(xStarter.m_xStoryFlags.Count(), 0u,
		"the starter state ships with story-flag bits set");
}
