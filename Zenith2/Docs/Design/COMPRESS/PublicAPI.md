# COMPRESS design: public API

This file is part of the COMPRESS design document family. The entry document is `Zenith2/Docs/Design/COMPRESS.md`; this file holds the full declarations of its section `Zenith2/Docs/Design/COMPRESS.md → "Public API"`: every public header of `ZenithCompression`, `ZenithCompression_Zlib` and `ZenithCompression_Zstd`, with the contract block of every public type in the grammar of `Zenith2/Docs/Contracts.md → "Contract blocks per type"`. The algorithms these declarations implement are in the entry document's `Zenith2/Docs/Design/COMPRESS.md → "Algorithms and state machines"`; a cite of a heading below is written `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "<heading>"`.

## Zenith_CompressionTypes.h

```cpp
// Zenith2/Engine/ZenithCompression/Zenith_CompressionTypes.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Types.h"
#include <cstdint>

class Zenith_CompressionDictionary;
class Zenith_FaultInjector;   // named by pointer only ([Z2-N-CONTRACT-029])

/// @contract enum Zenith_CompressionCodecID
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value never changes after construction.
/// Invalidation: None.
/// Units: None.
/// Errors: None. A value outside the defined enumerators read from data is refused by
///   Zenith_DecodeCompressedSectionDesc and Zenith_CompressionCodecSet::Find with COMPRESSION_ERROR_UNKNOWN_CODEC.
/// Version: since Z2 Stage 2. Serialized as one byte in every cooked format; an enumerator keeps its value and
///   meaning forever; 3-127 are reserved, 128-254 experimental (never cooked), 255 invalid.
/// @endcontract
enum Zenith_CompressionCodecID : uint8
{
	COMPRESSION_CODEC_ID_NONE = 0,   // stored: the section bytes are the payload
	COMPRESSION_CODEC_ID_ZLIB = 1,   // one RFC 1950 zlib stream
	COMPRESSION_CODEC_ID_ZSTD = 2,   // one RFC 8878 zstd frame
};

/// @contract enum Zenith_CompressionError
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value never changes after construction.
/// Invalidation: None.
/// Units: None.
/// Errors: None. The table "Error enumeration" of the COMPRESS design document states when each enumerator is returned.
/// Version: since Z2 Stage 2. An enumerator shipped in a fix version keeps its meaning; a new failure gets a new enumerator.
/// @endcontract
enum Zenith_CompressionError : uint8
{
	COMPRESSION_ERROR_INVALID_DESC = 1,
	COMPRESSION_ERROR_INVALID_LEVEL,
	COMPRESSION_ERROR_OUT_OF_MEMORY,
	COMPRESSION_ERROR_DESTINATION_TOO_SMALL,
	COMPRESSION_ERROR_CORRUPT_INPUT,
	COMPRESSION_ERROR_TRUNCATED_INPUT,
	COMPRESSION_ERROR_TRAILING_DATA,
	COMPRESSION_ERROR_SIZE_MISMATCH,
	COMPRESSION_ERROR_LIMIT_EXCEEDED,
	COMPRESSION_ERROR_UNKNOWN_CODEC,
	COMPRESSION_ERROR_UNSUPPORTED,
	COMPRESSION_ERROR_INVALID_DICTIONARY,
	COMPRESSION_ERROR_DICTIONARY_MISMATCH,
	COMPRESSION_ERROR_SIZE_OVERFLOW,
	COMPRESSION_ERROR_INTERNAL,
};

/// @contract enum Zenith_CompressionMode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value never changes after construction.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
enum Zenith_CompressionMode : uint8
{
	COMPRESSION_MODE_COMPRESS = 1,
	COMPRESSION_MODE_DECOMPRESS,
};

/// @contract enum Zenith_CompressionContextState
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value never changes after construction.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized. Transitions are the tables "Streaming compression context state
///   machine" and "Streaming decompression context state machine".
/// @endcontract
enum Zenith_CompressionContextState : uint8
{
	COMPRESSION_CONTEXT_STATE_READY = 1,   // created or reset; no call since
	COMPRESSION_CONTEXT_STATE_ACTIVE,      // accepting input
	COMPRESSION_CONTEXT_STATE_FLUSHING,    // compress: a Flush has started and not yet reported m_bFinished
	COMPRESSION_CONTEXT_STATE_ENDING,      // compress: End has started and not yet reported m_bFinished
	COMPRESSION_CONTEXT_STATE_FINISHED,    // the frame is complete and every output byte has been delivered
	COMPRESSION_CONTEXT_STATE_FAILED,      // an error occurred; every subsequent call returns it
};

/// @contract functions Zenith_CompressionConstants
/// Ownership: constexpr data.
/// Thread affinity: IMMUTABLE. Compile-time constants.
/// Invalidation: None.
/// Units: Bytes for every ul constant; levels are codec-defined integers; window sizes are log2 of bytes.
/// Errors: None.
/// Version: since Z2 Stage 2. The format limits (window, section sizes, dictionary IDs) are part of the cooked
///   format: changing one is a cooked-format version change and a re-cook.
/// @endcontract
constexpr uint64 ulCOMPRESSION_UNKNOWN_SIZE = 0xFFFFFFFFFFFFFFFFull;
constexpr uint64 ulCOMPRESSION_STREAM_CHUNK_BYTES = 131072;              // canonical input chunk and output staging
constexpr uint64 ulCOMPRESSION_SECTION_MAX_STORED_BYTES = 4294967296ull;  // 4 GiB
constexpr uint64 ulCOMPRESSION_SECTION_MAX_DECODED_BYTES = 4294967296ull; // 4 GiB
constexpr sint32 iCOMPRESSION_LEVEL_DEFAULT = 0;                          // "the codec's default level"
constexpr sint32 iCOMPRESSION_ZLIB_LEVEL_MIN = 1;
constexpr sint32 iCOMPRESSION_ZLIB_LEVEL_MAX = 9;
constexpr sint32 iCOMPRESSION_ZLIB_LEVEL_DEFAULT = 6;
constexpr sint32 iCOMPRESSION_ZSTD_LEVEL_MIN = 1;
constexpr sint32 iCOMPRESSION_ZSTD_LEVEL_MAX = 19;
constexpr sint32 iCOMPRESSION_ZSTD_LEVEL_DEFAULT = 3;
constexpr uint32 uCOMPRESSION_ZLIB_WINDOW_BITS = 15;
constexpr uint32 uCOMPRESSION_ZLIB_MEM_LEVEL = 8;
constexpr uint32 uCOMPRESSION_ZSTD_WINDOW_LOG_MAX = 23;                  // 8 MiB decode window cap
constexpr uint64 ulCOMPRESSION_DICTIONARY_MIN_BYTES = 256;
constexpr uint64 ulCOMPRESSION_DICTIONARY_MAX_BYTES = 1048576;
constexpr uint64 ulCOMPRESSION_DICTIONARY_DEFAULT_BYTES = 112640;
constexpr uint32 uCOMPRESSION_DICTIONARY_ID_MIN = 32768;
constexpr uint32 uCOMPRESSION_DICTIONARY_ID_MAX = 2147483647;
constexpr uint32 uCOMPRESSION_TRAINING_MIN_SAMPLES = 16;
constexpr uint32 uCOMPRESSION_TRAINING_MAX_SAMPLES = 1048576;
constexpr uint64 ulCOMPRESSION_TRAINING_MAX_SAMPLE_BYTES = 268435456;    // 256 MiB of samples in total
constexpr uint32 uCOMPRESSION_MAX_DICTIONARIES_PER_LOOKUP = 64;
constexpr uint8 uCOMPRESSION_CODEC_ID_FIRST_RESERVED = 3;
constexpr uint8 uCOMPRESSION_CODEC_ID_LAST_RESERVED = 127;
constexpr uint8 uCOMPRESSION_CODEC_ID_FIRST_EXPERIMENTAL = 128;
constexpr uint8 uCOMPRESSION_CODEC_ID_LAST_EXPERIMENTAL = 254;
constexpr uint8 uCOMPRESSION_CODEC_ID_INVALID = 255;

// Fault points consulted through the codec's Zenith_FaultInjector (Zenith2/Docs/Contracts/Memory.md → "Fallible construction").
constexpr uint32 uFAULT_POINT_COMPRESSION_CODEC_CREATE = 0x434D0001;        // the codec object allocation in a factory
constexpr uint32 uFAULT_POINT_COMPRESSION_CONTEXT_CREATE = 0x434D0002;      // the context object allocation
constexpr uint32 uFAULT_POINT_COMPRESSION_STAGING_CREATE = 0x434D0003;      // the staging buffer allocation
constexpr uint32 uFAULT_POINT_COMPRESSION_ENGINE_CREATE = 0x434D0004;       // the backend engine object allocation
constexpr uint32 uFAULT_POINT_COMPRESSION_LIBRARY_INIT = 0x434D0005;        // the library's init and parameter calls
constexpr uint32 uFAULT_POINT_COMPRESSION_LIBRARY_ALLOCATE = 0x434D0006;    // every allocation zlib or zstd requests
constexpr uint32 uFAULT_POINT_COMPRESSION_DICTIONARY_CREATE = 0x434D0007;   // the dictionary object and its CDict/DDict

/// @contract value Zenith_CompressionDesc
/// Ownership: Plain value passed by const reference to Zenith_CreateCompression_Zlib or Zenith_CreateCompression_Zstd.
/// Thread affinity: IMMUTABLE. Copied into the codec at creation.
///   Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: None.
/// Errors: A factory returns COMPRESSION_ERROR_INVALID_DESC for no field today; the struct exists so a future field
///   is not an ABI or signature change of Zenith_PfnCreateCompression.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
struct Zenith_CompressionDesc
{
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;   // tests alone; nullptr in production; outlives the codec
};

/// @contract value Zenith_CompressionContextDesc
/// Ownership: Plain value passed by const reference to Zenith_CompressionContext::Create, which copies it.
/// Thread affinity: IMMUTABLE. Copied at creation.
///   Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: Sizes in bytes; ulCOMPRESSION_UNKNOWN_SIZE means "not declared".
/// Errors: Create returns COMPRESSION_ERROR_INVALID_DESC or COMPRESSION_ERROR_INVALID_LEVEL per the table
///   "Context desc validation" of the COMPRESS design document, and COMPRESSION_ERROR_UNSUPPORTED for a dictionary
///   on a codec without dictionaries.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
struct Zenith_CompressionContextDesc
{
	Zenith_CompressionMode m_eMode = COMPRESSION_MODE_COMPRESS;
	sint32 m_iLevel = iCOMPRESSION_LEVEL_DEFAULT;                  // compress only; decompress requires the default
	uint64 m_ulPledgedSourceBytes = ulCOMPRESSION_UNKNOWN_SIZE;    // compress: the exact total input the caller will supply
	uint64 m_ulExpectedDecodedBytes = ulCOMPRESSION_UNKNOWN_SIZE;  // decompress: the declared decoded size, checked at End
	uint64 m_ulMaxDecodedBytes = ulCOMPRESSION_UNKNOWN_SIZE;       // decompress: a hard cap on decoded bytes
	const Zenith_CompressionDictionary* m_pxDictionary = nullptr;   // outlives the context
	const char* m_szName = "compression";                           // diagnostics; storage outlives the context
};

/// @contract value Zenith_CompressionTrainingDesc
/// Ownership: Plain value passed by const reference to Zenith_Compression::TrainDictionary.
/// Thread affinity: IMMUTABLE. Read during the call.
///   Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: m_ulDictionaryBytes in bytes; m_iLevel a zstd level.
/// Errors: TrainDictionary returns COMPRESSION_ERROR_INVALID_DESC for a capacity outside
///   [ulCOMPRESSION_DICTIONARY_MIN_BYTES, ulCOMPRESSION_DICTIONARY_MAX_BYTES] and COMPRESSION_ERROR_INVALID_LEVEL
///   for a level outside the codec's range.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
struct Zenith_CompressionTrainingDesc
{
	sint32 m_iLevel = iCOMPRESSION_LEVEL_DEFAULT;                        // the level the dictionary will compress at
	uint64 m_ulDictionaryBytes = ulCOMPRESSION_DICTIONARY_DEFAULT_BYTES; // the dictionary capacity to produce
};

/// @contract value Zenith_CompressionCodecInfo
/// Ownership: Owned by the codec; returned by const reference from Zenith_Compression::GetInfo.
/// Thread affinity: IMMUTABLE. Filled at codec creation and never changed.
/// Invalidation: The reference and m_szLibraryVersion are valid until the codec is destroyed.
/// Units: Levels are codec-defined integers.
/// Errors: None.
/// Version: since Z2 Stage 2. m_szName values are stable identifiers used in settings files ("zlib", "zstd").
/// @endcontract
struct Zenith_CompressionCodecInfo
{
	const Zenith_CompressionCodecID m_eCodec = COMPRESSION_CODEC_ID_NONE;
	const char* const m_szName = nullptr;             // "zlib" or "zstd"
	const char* const m_szLibraryVersion = nullptr;   // the linked library's runtime version string: "1.3.1", "1.5.7"
	const sint32 m_iMinLevel = 0;
	const sint32 m_iMaxLevel = 0;
	const sint32 m_iDefaultLevel = 0;
	const bool m_bSupportsDictionaries = false;
};

/// @contract value Zenith_CompressionFrameInfo
/// Ownership: Plain value returned by Zenith_Compression::ReadFrameInfo.
/// Thread affinity: IMMUTABLE. Never changes after it is returned.
/// Invalidation: None.
/// Units: Sizes in bytes; ulCOMPRESSION_UNKNOWN_SIZE when the frame does not record the decoded size.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Zenith_CompressionFrameInfo
{
	const Zenith_CompressionCodecID m_eCodec = COMPRESSION_CODEC_ID_NONE;
	const uint64 m_ulDecodedBytes = ulCOMPRESSION_UNKNOWN_SIZE;   // zstd Frame_Content_Size when present; zlib never
	const uint64 m_ulWindowBytes = 0;                              // zlib 2^(CINFO + 8); zstd Window_Size
	const uint32 m_uDictionaryID = 0;                              // zstd Dictionary_ID; 0 when absent; zlib 0
	const uint32 m_uHeaderBytes = 0;                               // zlib 2; zstd the frame header size
	const bool m_bHasChecksum = false;                              // zlib true (Adler-32); zstd Content_Checksum_flag
};

/// @contract value Zenith_CompressionProgress
/// Ownership: Plain value returned by the Zenith_CompressionContext calls.
/// Thread affinity: IMMUTABLE. Never changes after it is returned.
///   Members non-const: a streaming loop reassigns it on every call.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Zenith_CompressionProgress
{
	uint64 m_ulInputConsumed = 0;    // bytes of this call's input the context has taken ownership of
	uint64 m_ulOutputProduced = 0;   // bytes written to the start of this call's output span
	bool m_bFinished = false;         // Flush: the flush is complete; End and decompress Process: the frame is complete
};

/// @contract functions Zenith_CompressionTypeFunctions
/// Ownership: No state.
/// Thread affinity: ANY_THREAD. Pure functions.
/// Invalidation: The returned C strings are string literals.
/// Units: Levels are codec-defined integers.
/// Errors: Zenith_ParseCompressionCodecName returns COMPRESSION_ERROR_UNKNOWN_CODEC for any name other than "none",
///   "zlib" and "zstd" (exact, case-sensitive). Zenith_ValidateCompressionSetting returns COMPRESSION_ERROR_UNKNOWN_CODEC,
///   COMPRESSION_ERROR_INVALID_LEVEL or COMPRESSION_ERROR_UNSUPPORTED per the table "Compression setting fragment".
/// Version: since Z2 Stage 2. The names are stable: a settings file written in any fix version parses in every subsequent one.
/// @endcontract
constexpr const char* Zenith_ToString(Zenith_CompressionError eError)
{
	switch (eError)
	{
	case COMPRESSION_ERROR_INVALID_DESC: return "COMPRESSION_ERROR_INVALID_DESC";
	case COMPRESSION_ERROR_INVALID_LEVEL: return "COMPRESSION_ERROR_INVALID_LEVEL";
	case COMPRESSION_ERROR_OUT_OF_MEMORY: return "COMPRESSION_ERROR_OUT_OF_MEMORY";
	case COMPRESSION_ERROR_DESTINATION_TOO_SMALL: return "COMPRESSION_ERROR_DESTINATION_TOO_SMALL";
	case COMPRESSION_ERROR_CORRUPT_INPUT: return "COMPRESSION_ERROR_CORRUPT_INPUT";
	case COMPRESSION_ERROR_TRUNCATED_INPUT: return "COMPRESSION_ERROR_TRUNCATED_INPUT";
	case COMPRESSION_ERROR_TRAILING_DATA: return "COMPRESSION_ERROR_TRAILING_DATA";
	case COMPRESSION_ERROR_SIZE_MISMATCH: return "COMPRESSION_ERROR_SIZE_MISMATCH";
	case COMPRESSION_ERROR_LIMIT_EXCEEDED: return "COMPRESSION_ERROR_LIMIT_EXCEEDED";
	case COMPRESSION_ERROR_UNKNOWN_CODEC: return "COMPRESSION_ERROR_UNKNOWN_CODEC";
	case COMPRESSION_ERROR_UNSUPPORTED: return "COMPRESSION_ERROR_UNSUPPORTED";
	case COMPRESSION_ERROR_INVALID_DICTIONARY: return "COMPRESSION_ERROR_INVALID_DICTIONARY";
	case COMPRESSION_ERROR_DICTIONARY_MISMATCH: return "COMPRESSION_ERROR_DICTIONARY_MISMATCH";
	case COMPRESSION_ERROR_SIZE_OVERFLOW: return "COMPRESSION_ERROR_SIZE_OVERFLOW";
	case COMPRESSION_ERROR_INTERNAL: return "COMPRESSION_ERROR_INTERNAL";
	}
	return "COMPRESSION_ERROR_<unnamed>";
}

constexpr const char* Zenith_ToString(Zenith_CompressionCodecID eCodec)
{
	switch (eCodec)
	{
	case COMPRESSION_CODEC_ID_NONE: return "none";
	case COMPRESSION_CODEC_ID_ZLIB: return "zlib";
	case COMPRESSION_CODEC_ID_ZSTD: return "zstd";
	}
	return "<unregistered>";
}

Zenith_Result<Zenith_CompressionCodecID, Zenith_CompressionError> Zenith_ParseCompressionCodecName(
	Zenith_StringView strName);
Zenith_Result<void, Zenith_CompressionError> Zenith_ValidateCompressionSetting(
	Zenith_CompressionCodecID eCodec, sint32 iLevel, bool bUsesDictionary);
```

## Zenith_Compression.h

The interface follows the non-virtual-interface pattern: every public call is a non-virtual member implemented once in `ZenithCompression/Internal/` (argument validation, level resolution, arena restore, zones, counters, failure logging), which calls a private pure virtual `Do*` hook that the backend overrides. `Zenith_CompressionContext` is a friend so it can create engines.

```cpp
// Zenith2/Engine/ZenithCompression/Zenith_Compression.h
#pragma once
#include "ZenithCompression/Zenith_CompressionTypes.h"
#include "ZenithCore/Zenith_Owned.h"

class Zenith_Arena;           // named by reference only ([Z2-N-CONTRACT-029])
class Zenith_CompressionAllocator;
class Zenith_CompressionContext;
class Zenith_CompressionStreamEngine;
struct Zenith_CompressionEngineDesc;

/// @contract interface Zenith_Compression
/// Ownership: Created by Zenith_CreateCompression_Zlib or Zenith_CreateCompression_Zstd as a Zenith_Owned<Zenith_Compression>
///   on the general heap; the composition root (and each tool's main) owns one instance per codec for the process
///   lifetime and destroys it after every context, engine and dictionary created from it. The interface holds no data
///   member; each backend's final class owns an atomic count of its live engines and dictionaries (every context owns
///   exactly one engine, so contexts are counted through it), and its destructor fails ZENITH_CHECK when the count is not zero.
/// Thread affinity: ANY_THREAD. Every public member is const and may run concurrently from any thread; the backend's live-object
///   count is the only state a call changes (atomic: relaxed increments, release decrements, an acquire load in the backend destructor).
/// Invalidation: GetInfo's reference is valid until the codec is destroyed.
/// Units: Sizes in bytes. Levels are codec-defined integers: iCOMPRESSION_LEVEL_DEFAULT (0) selects GetInfo().m_iDefaultLevel;
///   any other value must lie in [m_iMinLevel, m_iMaxLevel].
/// Errors: Zenith_Result<…, Zenith_CompressionError>. Corrupt input returns COMPRESSION_ERROR_CORRUPT_INPUT, truncated input
///   COMPRESSION_ERROR_TRUNCATED_INPUT, a short destination COMPRESSION_ERROR_DESTINATION_TOO_SMALL; no path taken by
///   input bytes asserts. A span whose data pointer is null with a non-zero count is a programmer error (ZENITH_ASSERT).
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callbacks.
/// Version: since Z2 Stage 2. Compress output bytes are identical for the same input, codec, level, dictionary,
///   pledged-size presence and pinned library version; cooked headers record the codec ID, so a codec change is a
///   re-cook, never a runtime migration.
/// Example: Zenith2/Engine/ZenithCompression/Tests/Examples/Zenith_Compression_Example.cpp
/// @endcontract
class Zenith_Compression
{
public:
	virtual ~Zenith_Compression() = default;

	virtual const Zenith_CompressionCodecInfo& GetInfo() const = 0;
	virtual Zenith_FaultInjector* GetFaultInjector() const = 0;   // the factory desc's; nullptr in production

	Zenith_Result<sint32, Zenith_CompressionError> ResolveLevel(sint32 iLevel) const;
	Zenith_Result<uint64, Zenith_CompressionError> GetCompressBound(uint64 ulSourceBytes) const;
	Zenith_Result<uint64, Zenith_CompressionError> GetCompressScratchBound(
		uint64 ulSourceBytes, sint32 iLevel, const Zenith_CompressionDictionary* pxDictionary) const;
	Zenith_Result<uint64, Zenith_CompressionError> GetDecompressScratchBound(
		uint64 ulDecodedBytes, const Zenith_CompressionDictionary* pxDictionary) const;
	Zenith_Result<uint64, Zenith_CompressionError> GetContextMemoryBound(
		const Zenith_CompressionContextDesc& xDesc) const;

	Zenith_Result<uint64, Zenith_CompressionError> Compress(Zenith_ByteSpan xSource,
		Zenith_MutableByteSpan xDestination, sint32 iLevel, const Zenith_CompressionDictionary* pxDictionary,
		Zenith_Arena& xScratch) const;
	Zenith_Result<uint64, Zenith_CompressionError> Decompress(Zenith_ByteSpan xSource,
		Zenith_MutableByteSpan xDestination, const Zenith_CompressionDictionary* pxDictionary,
		Zenith_Arena& xScratch) const;
	Zenith_Result<Zenith_CompressionFrameInfo, Zenith_CompressionError> ReadFrameInfo(
		Zenith_ByteSpan xSource) const;

	Zenith_Result<Zenith_Owned<Zenith_CompressionDictionary>, Zenith_CompressionError> CreateDictionary(
		Zenith_ByteSpan xDictionaryBytes, sint32 iLevel) const;
	Zenith_Result<uint64, Zenith_CompressionError> TrainDictionary(Zenith_Span<const Zenith_ByteSpan> axSamples,
		Zenith_MutableByteSpan xDestination, const Zenith_CompressionTrainingDesc& xDesc, Zenith_Arena& xScratch) const;

private:
	friend class Zenith_CompressionContext;

	virtual Zenith_Result<uint64, Zenith_CompressionError> DoGetCompressBound(uint64 ulSourceBytes) const = 0;
	virtual Zenith_Result<uint64, Zenith_CompressionError> DoGetEngineMemoryBound(
		const Zenith_CompressionEngineDesc& xDesc) const = 0;
	virtual Zenith_Result<uint64, Zenith_CompressionError> DoGetDecompressScratchBound(
		uint64 ulDecodedBytes, const Zenith_CompressionDictionary* pxDictionary) const = 0;
	virtual Zenith_Result<Zenith_Owned<Zenith_CompressionStreamEngine>, Zenith_CompressionError> DoCreateEngine(
		const Zenith_CompressionEngineDesc& xDesc, Zenith_CompressionAllocator& xAllocator) const = 0;
	virtual Zenith_Result<Zenith_CompressionStreamEngine*, Zenith_CompressionError> DoConstructEngine(
		const Zenith_CompressionEngineDesc& xDesc, Zenith_CompressionAllocator& xAllocator, Zenith_Arena& xArena) const = 0;
	virtual Zenith_Result<uint64, Zenith_CompressionError> DoDecompress(Zenith_ByteSpan xSource,
		Zenith_MutableByteSpan xDestination, const Zenith_CompressionDictionary* pxDictionary,
		Zenith_CompressionAllocator& xAllocator) const = 0;
	virtual Zenith_Result<Zenith_CompressionFrameInfo, Zenith_CompressionError> DoReadFrameInfo(
		Zenith_ByteSpan xSource) const = 0;
	virtual Zenith_Result<Zenith_Owned<Zenith_CompressionDictionary>, Zenith_CompressionError> DoCreateDictionary(
		Zenith_ByteSpan xDictionaryBytes, sint32 iResolvedLevel) const = 0;
	virtual Zenith_Result<uint64, Zenith_CompressionError> DoTrainDictionary(Zenith_Span<const Zenith_ByteSpan> axSamples,
		Zenith_MutableByteSpan xDestination, sint32 iResolvedLevel, Zenith_CompressionAllocator& xAllocator) const = 0;
};

using Zenith_PfnCreateCompression = Zenith_Result<Zenith_Owned<Zenith_Compression>, Zenith_CompressionError> (*)(
	const Zenith_CompressionDesc& xDesc);
```

Live-object mechanism. The count lives in each backend's final class and is reached only from that backend's `Internal/` code, never through the interface:

```cpp
// ZenithCompression_Zstd/Internal/Zenith_CompressionZstd.h (ZenithCompression_Zlib/Internal/Zenith_CompressionZlib.h alike)
class Zenith_CompressionZstd final : public Zenith_Compression
{
public:
	~Zenith_CompressionZstd() override;          // ZENITH_CHECK(m_uLiveObjects.load(std::memory_order_acquire) == 0)
	void AdjustLiveObjects(sint32 iDelta) const;
		// +1: m_uLiveObjects.fetch_add(1, std::memory_order_relaxed)
		// -1: const uint32 uPrevious = m_uLiveObjects.fetch_sub(1, std::memory_order_release); ZENITH_ASSERT(uPrevious != 0)
	// … the Do* overrides …
private:
	mutable std::atomic<uint32> m_uLiveObjects{0};
};
```

1. Every engine class and the dictionary class of a backend store `const Zenith_CompressionZstd& m_xCodec` (the final class, which the backend's `Internal/` names; the backend's `DoCreateEngine`, `DoConstructEngine` and `DoCreateDictionary` pass `*this`).
2. Each such constructor calls `m_xCodec.AdjustLiveObjects(+1)` as its first statement, before any fallible step, and each destructor calls `m_xCodec.AdjustLiveObjects(−1)` as its last statement. The pair is unconditional and lives in one object: every object that will decrement has already incremented, so an object destroyed after a failed library init (a failed `deflateInit2`, `ZSTD_createCCtx_advanced`, CDict creation) balances the count by construction.
3. Library initialisation runs after construction, in the backend's `Do*` function (see "Zlib engine step", "Zstd engine step", "Dictionary creation and training"), never in a constructor, so constructors stay infallible ([Z2-N-CONTRACT-207]).
4. `Zenith_CompressionContext` is not counted itself: it owns exactly one engine from its `CreateParts` step to its destructor, and a context whose engine was never created is destroyed inside `Create`. The `contexts_live_delta` counter of "Counters" is a diagnostic emitted on `Create` success and by the context's destructor when `m_xEngine` is non-null; it is not the check.
5. Ordering: increments are `relaxed`; decrements are `release` and the destructor's load is `acquire`, the standard last-owner pattern ([Z2-N-CONTRACT-312]): every object's destruction, on any thread, happens-before the codec destructor's check that observes its decrement.

The interface's own `virtual ~Zenith_Compression() = default;` stays a plain interface destructor ([Z2-N-CONTRACT-033]); `Zenith_Owned<Zenith_Compression>` deletes through it, so the derived destructor and its check run first.

What each public member does (the algorithms are in `Zenith2/Docs/Design/COMPRESS.md → "Algorithms and state machines"`):

| Member | Behaviour |
|---|---|
| `GetInfo` | the codec's row of `Zenith2/Docs/Design/COMPRESS.md → "Codec metadata table"`, with `m_szLibraryVersion` from the linked library (`zlibVersion()`, `ZSTD_versionString()`) |
| `ResolveLevel` | 0 → `m_iDefaultLevel`; a value in `[m_iMinLevel, m_iMaxLevel]` → itself; anything else → `COMPRESSION_ERROR_INVALID_LEVEL` |
| `GetCompressBound` | the worst-case one-shot compressed size of `ulSourceBytes` bytes (formulas in `Zenith2/Docs/Design/COMPRESS.md → "Scratch and memory bounds"`); `COMPRESSION_ERROR_SIZE_OVERFLOW` when the bound does not fit a `uint64` |
| `GetCompressScratchBound` | the arena bytes one `Compress` call needs for that size, level and dictionary |
| `GetDecompressScratchBound` | the arena bytes one `Decompress` call needs |
| `GetContextMemoryBound` | the general-heap bytes a `Zenith_CompressionContext` created with `xDesc` can reach, staging included |
| `Compress` | one-shot compression of `xSource` into `xDestination`; returns the compressed size; scratch from `xScratch`, restored to its entry marker before return |
| `Decompress` | one-shot decompression of exactly one frame occupying all of `xSource`; returns the decoded size; scratch as above |
| `ReadFrameInfo` | parses the frame header at the start of `xSource` without decoding |
| `CreateDictionary` | a dictionary object from zstd-format dictionary bytes, bound to a resolved level; zlib → `COMPRESSION_ERROR_UNSUPPORTED` |
| `TrainDictionary` | writes a trained zstd dictionary into `xDestination`, returns its size; zlib → `COMPRESSION_ERROR_UNSUPPORTED` |

## Zenith_CompressionEngine.h

The engine is the backend's per-stream library state. Only `Zenith_CompressionContext` and the one-shot driver call it; the declarations are public because the backend libraries implement them.

```cpp
// Zenith2/Engine/ZenithCompression/Zenith_CompressionEngine.h
#pragma once
#include "ZenithCompression/Zenith_CompressionTypes.h"

/// @contract enum Zenith_CompressionEngineDirective
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. A value never changes after construction.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
enum Zenith_CompressionEngineDirective : uint8
{
	COMPRESSION_ENGINE_DIRECTIVE_CONTINUE = 1,   // compress: Z_NO_FLUSH / ZSTD_e_continue; decompress: decode
	COMPRESSION_ENGINE_DIRECTIVE_FLUSH,          // compress only: Z_SYNC_FLUSH / ZSTD_e_flush, with empty input
	COMPRESSION_ENGINE_DIRECTIVE_END,            // compress only: Z_FINISH / ZSTD_e_end, with empty input
};

/// @contract value Zenith_CompressionEngineDesc
/// Ownership: Plain value built by the API library from a validated Zenith_CompressionContextDesc.
/// Thread affinity: IMMUTABLE. Read by DoCreateEngine and DoConstructEngine during the call.
///   Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: Sizes in bytes; m_iLevel is already resolved (never 0 for compression).
/// Errors: Backends treat every field as validated; a violated invariant is ZENITH_ASSERT.
/// Version: since Z2 Stage 2. Backend-facing source API.
/// @endcontract
struct Zenith_CompressionEngineDesc
{
	Zenith_CompressionMode m_eMode = COMPRESSION_MODE_COMPRESS;
	sint32 m_iLevel = 0;
	uint64 m_ulPledgedSourceBytes = ulCOMPRESSION_UNKNOWN_SIZE;
	const Zenith_CompressionDictionary* m_pxDictionary = nullptr;
	uint32 m_uMaxWindowLog = uCOMPRESSION_ZSTD_WINDOW_LOG_MAX;
};

/// @contract value Zenith_CompressionEngineStep
/// Ownership: Plain value returned by Zenith_CompressionStreamEngine::Step.
/// Thread affinity: IMMUTABLE. Never changes after it is returned.
///   Members non-const: the context's step loop reassigns it on every call.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2. Backend-facing source API.
/// @endcontract
struct Zenith_CompressionEngineStep
{
	uint64 m_ulInputConsumed = 0;
	uint64 m_ulOutputProduced = 0;
	bool m_bDirectiveComplete = false;   // per engine and directive, as `Zenith2/Docs/Design/COMPRESS.md → "Zlib engine step"`
	                                     // and `Zenith2/Docs/Design/COMPRESS.md → "Zstd engine step"` define it
};

/// @contract interface Zenith_CompressionStreamEngine
/// Ownership: Created by a backend's DoCreateEngine (general heap, Zenith_Owned) or DoConstructEngine (in the caller's
///   arena, ended with Zenith_Destroy); owned by one Zenith_CompressionContext or one one-shot call; every library
///   allocation it makes goes through the Zenith_CompressionAllocator it was created with, which outlives it.
/// Thread affinity: OWNER_THREAD. Used by one thread at a time, like its owning context.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Step returns the library result mapped by the tables "Error mapping for zlib" and "Error mapping for zstd";
///   an input span longer than ulCOMPRESSION_STREAM_CHUNK_BYTES, or a FLUSH or END with non-empty input, is ZENITH_ASSERT.
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callbacks.
/// Version: since Z2 Stage 2. Backend-facing source API.
/// @endcontract
class Zenith_CompressionStreamEngine
{
public:
	virtual ~Zenith_CompressionStreamEngine() = default;
	virtual Zenith_Result<Zenith_CompressionEngineStep, Zenith_CompressionError> Step(Zenith_ByteSpan xInput,
		Zenith_MutableByteSpan xOutput, Zenith_CompressionEngineDirective eDirective) = 0;
	virtual Zenith_Result<void, Zenith_CompressionError> Reset() = 0;   // frame start, same parameters, no allocation
	virtual uint64 GetMemoryBytes() const = 0;
};
```

## Zenith_CompressionAllocator.h

```cpp
// Zenith2/Engine/ZenithCompression/Zenith_CompressionAllocator.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

class Zenith_Arena;           // named by reference and pointer only ([Z2-N-CONTRACT-029])
class Zenith_FaultInjector;
class Zenith_Heap;

/// @contract type Zenith_CompressionAllocator
/// Ownership: Created on the stack or as a member by the API library (one per context, one per one-shot call) and passed
///   by reference to a backend; it owns no memory itself: zlib and zstd own their blocks and return them through Free.
///   It stores a pointer to a heap (the root heap) or an arena, which outlives it, and the codec's fault injector.
/// Thread affinity: OWNER_THREAD. Used by the one thread running the owning context or call.
/// Invalidation: In arena mode every block becomes invalid when the arena is reset past the entry marker.
/// Units: Bytes. Every block is 16-byte aligned.
/// Errors: TryAllocate returns nullptr (never asserts) on heap or arena failure, on a size of 0 after rounding overflow,
///   and when the fault injector fails uFAULT_POINT_COMPRESSION_LIBRARY_ALLOCATE; that nullptr is what zlib (Z_MEM_ERROR)
///   and zstd (memory_allocation) expect.
/// Version: since Z2 Stage 2. Backend-facing source API.
/// @endcontract
class Zenith_CompressionAllocator
{
public:
	Zenith_CompressionAllocator(Zenith_Heap& xHeap, Zenith_FaultInjector* pxFaultInjector);
	Zenith_CompressionAllocator(Zenith_Arena& xArena, Zenith_FaultInjector* pxFaultInjector);
	Zenith_CompressionAllocator(const Zenith_CompressionAllocator&) = delete;
	Zenith_CompressionAllocator& operator=(const Zenith_CompressionAllocator&) = delete;
	~Zenith_CompressionAllocator();   // heap mode: ZENITH_CHECK(m_ulLiveBytes == 0)

	void* TryAllocate(uint64 ulBytes);
	void Free(void* pMemory);         // nullptr is a no-op; arena mode records the free and releases nothing
	uint64 GetLiveBytes() const;
	uint64 GetHighWaterBytes() const;
	uint64 GetFailedAllocations() const;
	bool IsArenaMode() const;

private:
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_Arena* m_pxArena = nullptr;
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;
	uint64 m_ulLiveBytes = 0;
	uint64 m_ulHighWaterBytes = 0;
	uint64 m_ulFailedAllocations = 0;
};
```

In heap mode `m_ulLiveBytes` adds `Zenith_Heap::GetUsableSize(p)` on allocation and subtracts it on free. In arena mode it counts every byte allocated since construction (frees are recorded but release nothing), so `GetHighWaterBytes` is the call's scratch use, which the "ScratchBoundIsSufficient" conformance case compares against the bound.

## Zenith_CompressionContext.h

```cpp
// Zenith2/Engine/ZenithCompression/Zenith_CompressionContext.h
#pragma once
#include "ZenithCompression/Zenith_Compression.h"
#include "ZenithCompression/Zenith_CompressionAllocator.h"
#include "ZenithCompression/Zenith_CompressionEngine.h"
#include "ZenithCore/Zenith_Owned.h"

/// @contract type Zenith_CompressionContext
/// Ownership: Created by Zenith_CompressionContext::Create as a Zenith_Owned on the general heap and owned by its caller;
///   holds a reference to the codec and a pointer to the desc's dictionary, both of which outlive it; every byte it and its
///   engine allocate comes from the root heap through its Zenith_CompressionAllocator and is released by its destructor.
/// Thread affinity: OWNER_THREAD. One thread at a time; ownership moves only at a frame-graph edge, a sync point or a
///   hand-off the caller synchronises (the conformance case ThreadSafety_ContextHandOff).
/// Invalidation: Destroying the context, or Reset, abandons the frame in progress: output already delivered is the
///   caller's and never decodes as a complete frame.
/// Units: Bytes; totals are uint64 and exact beyond 4 GiB.
/// Errors: Process, Flush and End return Zenith_Result<Zenith_CompressionProgress, Zenith_CompressionError>; the first error
///   moves the context to COMPRESSION_CONTEXT_STATE_FAILED and every subsequent call returns the same error until Reset. Calls
///   outside the state machine (Process after End began, Flush on a decompression context) are ZENITH_CHECK failures.
/// Version: since Z2 Stage 2. Compressed bytes depend only on the input bytes, the flush offsets, the codec, the level, the
///   dictionary, whether a pledged size was given, and the pinned library version; never on how input and output are sliced.
/// Example: Zenith2/Engine/ZenithCompression/Tests/Examples/Zenith_Compression_Example.cpp
/// @endcontract
class Zenith_CompressionContext
{
	struct PrivateTag {};

public:
	static Zenith_Result<Zenith_Owned<Zenith_CompressionContext>, Zenith_CompressionError> Create(
		const Zenith_Compression& xCodec, const Zenith_CompressionContextDesc& xDesc);

	Zenith_CompressionContext(PrivateTag xTag, const Zenith_Compression& xCodec, const Zenith_CompressionContextDesc& xDesc,
		sint32 iResolvedLevel);
	~Zenith_CompressionContext();
	Zenith_CompressionContext(const Zenith_CompressionContext&) = delete;
	Zenith_CompressionContext& operator=(const Zenith_CompressionContext&) = delete;

	Zenith_Result<Zenith_CompressionProgress, Zenith_CompressionError> Process(
		Zenith_ByteSpan xInput, Zenith_MutableByteSpan xOutput);
	Zenith_Result<Zenith_CompressionProgress, Zenith_CompressionError> Flush(Zenith_MutableByteSpan xOutput);
	Zenith_Result<Zenith_CompressionProgress, Zenith_CompressionError> End(Zenith_MutableByteSpan xOutput);
	void Reset();

	Zenith_CompressionContextState GetState() const;
	Zenith_CompressionMode GetMode() const;
	Zenith_CompressionCodecID GetCodecID() const;
	sint32 GetLevel() const;                 // the resolved level; 0 for decompression
	uint64 GetTotalInputBytes() const;
	uint64 GetTotalOutputBytes() const;
	uint64 GetMemoryBytes() const;           // context object, staging and engine, live now
	Zenith_CompressionError GetError() const; // ZENITH_CHECK: GetState() == COMPRESSION_CONTEXT_STATE_FAILED

private:
	struct Zenith_CompressionStaging
	{
		uint8 m_auInput[ulCOMPRESSION_STREAM_CHUNK_BYTES];
		uint8 m_auOutput[ulCOMPRESSION_STREAM_CHUNK_BYTES];
	};

	Zenith_Result<void, Zenith_CompressionError> CreateParts(const Zenith_CompressionEngineDesc& xEngineDesc);

	const Zenith_Compression* m_pxCodec = nullptr;
	Zenith_CompressionContextDesc m_xDesc;
	sint32 m_iLevel = 0;
	Zenith_CompressionAllocator m_xAllocator;
	Zenith_Owned<Zenith_CompressionStaging> m_xStaging;      // compress contexts only
	Zenith_Owned<Zenith_CompressionStreamEngine> m_xEngine;
	uint64 m_ulStagedInputBytes = 0;          // bytes in m_auInput not yet given to the engine
	uint64 m_ulStagedOutputOffset = 0;        // first byte of m_auOutput not yet delivered
	uint64 m_ulStagedOutputBytes = 0;         // bytes in m_auOutput produced by the engine
	uint64 m_ulChunkOffset = 0;               // bytes of the current canonical chunk already given to the engine
	uint64 m_ulTotalInputBytes = 0;
	uint64 m_ulTotalOutputBytes = 0;
	Zenith_CompressionContextState m_eState = COMPRESSION_CONTEXT_STATE_READY;
	Zenith_CompressionError m_eError = COMPRESSION_ERROR_INTERNAL;   // meaningful only in FAILED
	bool m_bFrameEndSeen = false;              // decompress: the engine reported the frame's end
	uint64 m_ulLastReportedMemoryBytes = 0;   // GetMemoryBytes() at the last context_memory_bytes_delta emission
};
```

The members are declared in the header because the type is concrete; their layout is identical in every configuration (`Zenith2/Docs/Contracts.md → "No layout-changing conditionals"`). `Zenith_CompressionStaging` is 256 KiB and is allocated once per compress context with `Zenith_TryNew`, so it is owned by a `Zenith_Owned` like every other heap object.

<!-- informative -->
A compress context costs its engine plus 256 KiB of staging. The staging is what makes compressed bytes independent of the caller's slicing by construction (C1): without it, correctness would rest on an undocumented property of each library's internal buffering, which a library upgrade could change without any API change.
<!-- /informative -->

## Zenith_CompressionDictionary.h

```cpp
// Zenith2/Engine/ZenithCompression/Zenith_CompressionDictionary.h
#pragma once
#include "ZenithCompression/Zenith_CompressionTypes.h"

/// @contract interface Zenith_CompressionDictionary
/// Ownership: Created by Zenith_Compression::CreateDictionary as a Zenith_Owned on the general heap; owned by its creator
///   (for cooked content, the archive or asset system that loaded the dictionary bytes); it copies the bytes it was created
///   from, so the source span may be released after creation; it outlives every context and call that uses it, and the codec
///   that created it outlives it.
/// Thread affinity: IMMUTABLE. Read-only after creation; any number of threads and contexts may use it concurrently.
/// Invalidation: None.
/// Units: GetMemoryBytes in bytes; GetLevel a codec level.
/// Errors: None; creation errors are returned by Zenith_Compression::CreateDictionary.
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callbacks.
/// Version: since Z2 Stage 2. Dictionary bytes are the zstd dictionary format; the ID is recorded in every frame and
///   section descriptor that uses it.
/// @endcontract
class Zenith_CompressionDictionary
{
public:
	virtual ~Zenith_CompressionDictionary() = default;
	virtual Zenith_CompressionCodecID GetCodecID() const = 0;
	virtual uint32 GetDictionaryID() const = 0;   // in [uCOMPRESSION_DICTIONARY_ID_MIN, uCOMPRESSION_DICTIONARY_ID_MAX]
	virtual sint32 GetLevel() const = 0;          // the resolved level it compresses at
	virtual uint64 GetMemoryBytes() const = 0;    // bytes, CDict and DDict
};
```

## Zenith_CompressionCodecSet.h

```cpp
// Zenith2/Engine/ZenithCompression/Zenith_CompressionCodecSet.h
#pragma once
#include "ZenithCompression/Zenith_Compression.h"

/// @contract value Zenith_CompressionCodecSetDesc
/// Ownership: Plain value copied by the Zenith_CompressionCodecSet constructor.
/// Thread affinity: IMMUTABLE. Copied at construction.
///   Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
struct Zenith_CompressionCodecSetDesc
{
	bool m_bAllowExperimentalCodecs = false;   // tests alone: admits IDs 128-254
};

/// @contract type Zenith_CompressionCodecSet
/// Ownership: A value owned by its creator (ZenithRuntime for the Player, each tool's main for tools); stores non-owning
///   pointers to codecs whose owner (the same creator) destroys them after the set.
/// Thread affinity: ANY_THREAD_EXCEPT(Add). Add is called by the owning thread before the set is shared; afterwards the set is
///   read-only and Find and Contains may run concurrently from any thread.
/// Invalidation: A pointer returned by Find is valid while the codec lives.
/// Units: None.
/// Errors: Add returns COMPRESSION_ERROR_INVALID_DESC for a codec whose ID is NONE, reserved, invalid, or already present, and
///   COMPRESSION_ERROR_UNKNOWN_CODEC for an experimental ID when the desc does not allow it; Find returns
///   COMPRESSION_ERROR_UNKNOWN_CODEC for an ID with no codec (NONE included, which callers handle before Find).
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_CompressionCodecSet
{
public:
	explicit Zenith_CompressionCodecSet(const Zenith_CompressionCodecSetDesc& xDesc);
	Zenith_Result<void, Zenith_CompressionError> Add(const Zenith_Compression& xCodec);
	Zenith_Result<const Zenith_Compression*, Zenith_CompressionError> Find(Zenith_CompressionCodecID eCodec) const;
	bool Contains(Zenith_CompressionCodecID eCodec) const;
	uint32 GetCount() const;

private:
	const Zenith_Compression* m_apxCodecs[256] = {};
	Zenith_CompressionCodecSetDesc m_xDesc;
	uint32 m_uCount = 0;
};
```

## Zenith_CompressedSection.h

```cpp
// Zenith2/Engine/ZenithCompression/Zenith_CompressedSection.h
#pragma once
#include "ZenithCompression/Zenith_CompressionCodecSet.h"
#include "ZenithCompression/Zenith_CompressionDictionary.h"

/// @contract value Zenith_CompressedSectionDesc
/// Ownership: Plain value; its serialized form is the 24-byte record of "Compressed section descriptor on disk", read and
///   written only through Zenith_DecodeCompressedSectionDesc and Zenith_EncodeCompressedSectionDesc, never by memcpy.
/// Thread affinity: IMMUTABLE. A value never changes after construction.
///   Members non-const: a member of section tables that other libraries fill and copy-assign.
/// Invalidation: None.
/// Units: Sizes in bytes; m_uLevel the resolved codec level (0 for NONE).
/// Errors: Zenith_ValidateCompressedSectionDesc states every rule a valid value meets.
/// Version: since Z2 Stage 2. Part of every cooked format that embeds it; a layout change is a cooked-format version change.
/// @endcontract
struct Zenith_CompressedSectionDesc
{
	Zenith_CompressionCodecID m_eCodec = COMPRESSION_CODEC_ID_NONE;
	uint8 m_uLevel = 0;
	uint16 m_uFlags = 0;               // bit 0: COMPRESSED_SECTION_FLAG_DICTIONARY; bits 1-15 reserved, zero
	uint32 m_uDictionaryID = 0;        // non-zero exactly when bit 0 is set
	uint64 m_ulStoredBytes = 0;        // bytes of the section in the file
	uint64 m_ulDecodedBytes = 0;       // bytes after decoding
};

/// @contract value Zenith_CompressedSectionLimits
/// Ownership: Plain value passed by const reference.
/// Thread affinity: IMMUTABLE. Read during the call.
///   Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None; values above the defaults are clamped to the defaults (a reader may only tighten the format limits).
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
struct Zenith_CompressedSectionLimits
{
	uint64 m_ulMaxStoredBytes = ulCOMPRESSION_SECTION_MAX_STORED_BYTES;
	uint64 m_ulMaxDecodedBytes = ulCOMPRESSION_SECTION_MAX_DECODED_BYTES;
	bool m_bAllowExperimentalCodecs = false;
};

/// @contract value Zenith_CompressSectionOptions
/// Ownership: Plain value passed by const reference.
/// Thread affinity: IMMUTABLE. Read during the call.
///   Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
struct Zenith_CompressSectionOptions
{
	bool m_bStoreIfNotSmaller = true;   // write codec NONE when the compressed size is not smaller than the source
};

/// @contract functions Zenith_CompressedSectionFunctions
/// Ownership: No state; spans are borrowed for the call; the destination belongs to the caller.
/// Thread affinity: ANY_THREAD. Each call uses only its arguments; xScratch follows its own OWNER_THREAD rule.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Zenith_Result with the enumerators of the tables "Descriptor validation" and "Section compression and
///   decompression"; no path taken by descriptor or section bytes asserts.
/// Version: since Z2 Stage 2. The 24-byte layout is fixed for the cooked-format versions that embed it.
/// @endcontract
constexpr uint16 uCOMPRESSED_SECTION_FLAG_DICTIONARY = 0x0001;
constexpr uint16 uCOMPRESSED_SECTION_FLAGS_DEFINED = 0x0001;
constexpr uint64 ulCOMPRESSED_SECTION_DESC_BYTES = 24;

Zenith_Result<void, Zenith_CompressionError> Zenith_ValidateCompressedSectionDesc(
	const Zenith_CompressedSectionDesc& xDesc, const Zenith_CompressedSectionLimits& xLimits);
Zenith_Result<void, Zenith_CompressionError> Zenith_EncodeCompressedSectionDesc(
	const Zenith_CompressedSectionDesc& xDesc, Zenith_MutableByteSpan xDestination);
Zenith_Result<Zenith_CompressedSectionDesc, Zenith_CompressionError> Zenith_DecodeCompressedSectionDesc(
	Zenith_ByteSpan xSource, const Zenith_CompressedSectionLimits& xLimits);
Zenith_Result<Zenith_CompressedSectionDesc, Zenith_CompressionError> Zenith_CompressSection(
	const Zenith_Compression& xCodec, Zenith_ByteSpan xSource, sint32 iLevel, const Zenith_CompressionDictionary* pxDictionary,
	Zenith_MutableByteSpan xDestination, Zenith_Arena& xScratch, const Zenith_CompressSectionOptions& xOptions);
Zenith_Result<Zenith_CompressedSectionDesc, Zenith_CompressionError> Zenith_StoreSection(
	Zenith_ByteSpan xSource, Zenith_MutableByteSpan xDestination);
Zenith_Result<void, Zenith_CompressionError> Zenith_DecompressSection(const Zenith_CompressionCodecSet& xCodecs,
	const Zenith_CompressedSectionDesc& xDesc, Zenith_Span<const Zenith_CompressionDictionary* const> apxDictionaries,
	Zenith_ByteSpan xStored, Zenith_MutableByteSpan xDestination, Zenith_Arena& xScratch,
	const Zenith_CompressedSectionLimits& xLimits);
```

## Backend factory headers

```cpp
// Zenith2/Engine/ZenithCompression_Zlib/Zenith_Compression_Zlib.h
#pragma once
#include "ZenithCompression/Zenith_Compression.h"

/// @contract functions Zenith_CompressionZlibFactory
/// Ownership: Returns a Zenith_Owned<Zenith_Compression> on the general heap, owned by the caller.
/// Thread affinity: ANY_THREAD. The factory holds no state.
/// Invalidation: None.
/// Units: None.
/// Errors: COMPRESSION_ERROR_OUT_OF_MEMORY when the codec object cannot be allocated (or the fault injector fails
///   uFAULT_POINT_COMPRESSION_CODEC_CREATE); COMPRESSION_ERROR_INTERNAL when the linked zlib's runtime version differs from
///   the version compiled against (1.3.1).
/// Version: since Z2 Stage 2. The signature is Zenith_PfnCreateCompression.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_Compression>, Zenith_CompressionError> Zenith_CreateCompression_Zlib(
	const Zenith_CompressionDesc& xDesc);
```

```cpp
// Zenith2/Engine/ZenithCompression_Zstd/Zenith_Compression_Zstd.h
#pragma once
#include "ZenithCompression/Zenith_Compression.h"

/// @contract functions Zenith_CompressionZstdFactory
/// Ownership: Returns a Zenith_Owned<Zenith_Compression> on the general heap, owned by the caller.
/// Thread affinity: ANY_THREAD. The factory holds no state.
/// Invalidation: None.
/// Units: None.
/// Errors: COMPRESSION_ERROR_OUT_OF_MEMORY when the codec object cannot be allocated (or the fault injector fails
///   uFAULT_POINT_COMPRESSION_CODEC_CREATE); COMPRESSION_ERROR_INTERNAL when ZSTD_versionNumber() differs from 10507.
/// Version: since Z2 Stage 2. The signature is Zenith_PfnCreateCompression.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_Compression>, Zenith_CompressionError> Zenith_CreateCompression_Zstd(
	const Zenith_CompressionDesc& xDesc);
```

Each backend's implementation class is declared in its `Internal/` and names `Zenith_Compression` as its first base (`Zenith2/Docs/Contracts.md → "API boundary"`), so the factory returns it through the same-address `Zenith_Owned<Derived>` → `Zenith_Owned<Base>` conversion, as `Zenith2/Docs/WorkerGuide.md` shows.
