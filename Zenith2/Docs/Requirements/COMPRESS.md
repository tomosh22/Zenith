# COMPRESS requirement catalogue

## Purpose

This catalogue breaks the COMPRESS inventory items (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "COMPRESS Compression") into testable requirements `Z2-R-COMPRESS-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of `Zenith2/Docs/Design/COMPRESS.md` that specify it. The design document's "Requirement index" is the reverse map. COMPRESS owns every requirement here; where a requirement is proved by another area's ticket (a cooked format embedding the descriptor, the cook linking one zstd copy), that area is named in the evidence and in the design's "Dependencies".

## Requirements

### Z2-R-COMPRESS-001 API library exports the interface and no factory

- **Statement:** `ZenithCompression` declares the interface `Zenith_Compression` and every shared type, depends only on `ZenithCore`, and exports no factory function; `declared_deps.json` records it with `kind` ENGINE and a null `factory`.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` gate step (declared-edge check against `declared_deps.json`) and `ApiContractCheck` in the base category on the ticket that lands the API slice; code review of `Zenith2/Engine/ZenithCompression/CMakeLists.txt`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Target summary"; Zenith2/Docs/Design/COMPRESS.md → "ZenithCompression files"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_Compression.h"

### Z2-R-COMPRESS-002 Interface contract blocks and results

- **Statement:** Every public type of the three COMPRESS libraries carries a contract block in the Contracts grammar, and every fallible public function returns `Zenith_Result<…, Zenith_CompressionError>`.
- **Covers:** Z2-I-COMPRESS-01, Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` gate steps over the eight API headers and two factory headers. `Test_Compression_Example_RoundTrips` compiles and runs the example the contract blocks cite, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Header map"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_Compression.h"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Backend factory headers"

### Z2-R-COMPRESS-003 Interface shape: virtual destructor and private hooks

- **Statement:** `Zenith_Compression`, `Zenith_CompressionStreamEngine` and `Zenith_CompressionDictionary` each declare a public `virtual ~T() = default` and no data member; `Zenith_Compression`'s public calls are non-virtual members implemented in `ZenithCompression/Internal/` that call private pure virtual `Do*` hooks; the live-object count is not part of the interface (it lives in each backend's final class).
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` (interface kind check); `Test_Compression_ResolveLevel_EveryBoundary` and `Test_Compression_OneShot_RestoresArenaMarker` exercise the facade on both real codecs in the `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_Compression.h"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionEngine.h"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionDictionary.h"; Zenith2/Docs/Design/COMPRESS.md → "Thread affinity per type"

### Z2-R-COMPRESS-004 Codec metadata matches the table

- **Statement:** `GetInfo()` returns, for zlib, codec 1, name `zlib`, levels 1–9 default 6, no dictionaries, library version `1.3.1`; for zstd, codec 2, name `zstd`, levels 1–19 default 3, dictionaries, library version `1.5.7`; the level constants of `Zenith_CompressionTypes.h` equal these values.
- **Covers:** Z2-I-COMPRESS-01, Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Metadata_MatchesTable` per backend and `Test_CompressionTypes_LevelConstants_MatchCodecInfo`, in the `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Codec metadata table"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionTypes.h"

### Z2-R-COMPRESS-005 Level resolution

- **Statement:** Level 0 resolves to the codec's default (or, with a dictionary, to the dictionary's level); a level inside the codec's range resolves to itself; any other level is `COMPRESSION_ERROR_INVALID_LEVEL` from every entry point, never an assert.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Compression_ResolveLevel_EveryBoundary` and `Test_CompressionConformance_Levels_OutOfRange_AreRejected` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_Compression.h"; Zenith2/Docs/Design/COMPRESS.md → "Codec metadata table"; Zenith2/Docs/Design/COMPRESS.md → "Context desc validation"

### Z2-R-COMPRESS-006 Error enumeration values and names

- **Statement:** `Zenith_CompressionError` is a `uint8` enumeration numbered 1–15 in the order of the design's table, has a `constexpr Zenith_ToString` overload in the same header returning each enumerator's own spelling, and an enumerator keeps its value and meaning in every subsequent fix version.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionTypes_ErrorToString_EveryEnumerator` and `Test_CompressionTypes_ErrorValues_AreStable` (`static_assert`s), `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionTypes.h"; Zenith2/Docs/Design/COMPRESS.md → "Error enumeration"

### Z2-R-COMPRESS-007 Factory signature and pointer type

- **Statement:** Each backend's root header `Zenith_Compression_<Backend>.h` declares exactly `Zenith_CreateCompression_<Backend>(const Zenith_CompressionDesc&)` returning `Zenith_Result<Zenith_Owned<Zenith_Compression>, Zenith_CompressionError>`, matching `Zenith_PfnCreateCompression`, and nothing else.
- **Covers:** Z2-I-COMPRESS-01, Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`; `Libgraph` (backend exports only its factory); the conformance adapters take the factories' addresses as `Zenith_PfnCreateCompression`, which fails to compile on a mismatch (`Build` step).
- **Design:** Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Backend factory headers"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_Compression.h"; Zenith2/Docs/Design/COMPRESS.md → "Target summary"

### Z2-R-COMPRESS-008 Factory failure unwinds

- **Statement:** A factory that cannot allocate its codec object (or whose fault injector fails `uFAULT_POINT_COMPRESSION_CODEC_CREATE`) returns `COMPRESSION_ERROR_OUT_OF_MEMORY` with nothing allocated, and one whose linked library reports a runtime version other than the pinned one returns `COMPRESSION_ERROR_INTERNAL`.
- **Covers:** Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_AllocationFailure_CreateFaultSweep` (factory points included) per backend; `Test_CompressionZlib_LinkedVersion_MatchesPin` and `Test_CompressionZstd_LinkedVersion_MatchesPin`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Backend factory headers"; Zenith2/Docs/Design/COMPRESS.md → "Failure table"

### Z2-R-COMPRESS-009 Codec set selects codecs by ID

- **Statement:** `Zenith_CompressionCodecSet::Add` accepts one codec per defined ID and rejects NONE, reserved, invalid and duplicate IDs with `INVALID_DESC` and experimental IDs without the opt-in with `UNKNOWN_CODEC`; `Find` returns the codec or `UNKNOWN_CODEC`; after filling, `Find` and `Contains` are safe from any thread.
- **Covers:** Z2-I-COMPRESS-01, Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionCodecSet_AddFind_ReturnsCodec`, `…_AddDuplicate_IsRejected`, `…_AddNoneReservedInvalid_IsRejected`, `…_Experimental_RequiresOptIn`, `…_FindAbsent_ReturnsUnknownCodec`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionCodecSet.h"; Zenith2/Docs/Design/COMPRESS.md → "Codec ID registry"

### Z2-R-COMPRESS-010 Third-party headers confined to backend Internal

- **Statement:** `zlib.h` is included only by `ZenithCompression_Zlib/Internal/Zenith_ZlibInclude.h`, and `zstd.h`, `zstd_errors.h` and `zdict.h` only by `ZenithCompression_Zstd/Internal/Zenith_ZstdInclude.h`; no public declaration of any COMPRESS library names a zlib or zstd type.
- **Covers:** Z2-I-COMPRESS-01, Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` leakage and third-party-owner rules and `ForbiddenCheck`, base category, every ticket touching these libraries.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "ZenithCompression_Zlib files"; Zenith2/Docs/Design/COMPRESS.md → "ZenithCompression_Zstd files"; Zenith2/Docs/Design/COMPRESS.md → "Header map"

### Z2-R-COMPRESS-011 Consumers depend on the API alone

- **Statement:** Every consumer of compression (ZenithAssets, ZenithSave, ZenithRuntime) uses only `ZenithCompression` symbols and types; the backend public headers and factories are included only by the composition root and by test, benchmark, fuzz and tool executables, as [Z2-N-CONTRACT-042] and the target-kind predicate of [Z2-N-CONTRACT-050] state; and no zlib or zstd symbol appears outside the two backends.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-007, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `${ZENITH_OUT}/gates/libgraph/swap_compression.json` clean, written by the `Libgraph` step and collected by the Stage 3 acceptance ticket.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Libgraph swap-contract evidence"; Zenith2/Docs/Design/COMPRESS.md → "Areas that consume COMPRESS"

### Z2-R-COMPRESS-012 Both codecs created at startup

- **Statement:** Every game executable links both backends, and ZenithRuntime creates both codecs at startup step 11 and adds them to one `Zenith_CompressionCodecSet` that it destroys before the codecs, after the asset registry and save system; `game.json` names no compression backend.
- **Covers:** Z2-I-COMPRESS-01, Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** The minimal game of the Stage 2 acceptance boots on Windows and the S24 (MOD's composition tests); the startup log line of step 11 names both codecs and their library versions.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Areas that consume COMPRESS"; Zenith2/Docs/Design/COMPRESS.md → "Lifetime and shutdown order"

### Z2-R-COMPRESS-013 No Luau compression surface

- **Statement:** No binding table for any COMPRESS type is registered with the script runtime, so no script can call a compression function.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** Code review of `Zenith2/Engine/ZenithRuntime/Internal/ScriptBindings/` in the MOD and SCRIPT binding tickets; the generated `.d.luau` of every game contains no compression declaration (checked by the Stage 2 acceptance's review of generated types).
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Luau surface"

### Z2-R-COMPRESS-014 zlib backend library target

- **Statement:** `ZenithCompression_Zlib` is `KIND BACKEND`, `BACKEND_OF ZenithCompression`, `BACKEND_NAME Zlib`, `FACTORY Zenith_CreateCompression_Zlib`, depends on `ZenithCompression` and `ZenithCore`, owns the manifest entry `zlib`, and is supported in every preset (`ZENITH_ALL`).
- **Covers:** Z2-I-COMPRESS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `Configure` and `CompileCoverage` over every preset; `declared_deps.json` record; `Build -Preset android-arm64`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Target summary"; Zenith2/Docs/Design/COMPRESS.md → "ZenithCompression_Zlib files"

### Z2-R-COMPRESS-015 zstd backend library target

- **Statement:** `ZenithCompression_Zstd` is `KIND BACKEND`, `BACKEND_OF ZenithCompression`, `BACKEND_NAME Zstd`, `FACTORY Zenith_CreateCompression_Zstd`, depends on `ZenithCompression` and `ZenithCore`, owns the manifest entry `zstd`, and is supported in every preset (`ZENITH_ALL`).
- **Covers:** Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `Configure` and `CompileCoverage` over every preset; `declared_deps.json` record; `Build -Preset android-arm64`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Target summary"; Zenith2/Docs/Design/COMPRESS.md → "ZenithCompression_Zstd files"

### Z2-R-COMPRESS-016 zlib pinned at 1.3.1

- **Statement:** The `zlib` manifest entry pins version 1.3.1, the archive URL of the design, licence `Zlib` and owner `ZenithCompression_Zlib`, with the archive and licence SHA-256 values verified against the downloaded archive by the Stage 0 reproducibility-manifest ticket (the design's values are candidates) and re-verified by `Setup-Zenith2.ps1 -Check`; `Zenith_ZlibInclude.h` asserts `ZLIB_VERNUM == 0x1310` at compile time.
- **Covers:** Z2-I-COMPRESS-02, Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Setup-Zenith2.ps1 -Check` exit 0 on a clean machine; `Test_CompressionZlib_LinkedVersion_MatchesPin`; the `static_assert` in the `Build` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Third-party integration of zlib"

### Z2-R-COMPRESS-017 zlib built as a wrapper with Z_SOLO

- **Statement:** zlib is compiled by `Zenith2/ThirdParty/zlib/CMakeLists.txt` from exactly `adler32.c crc32.c deflate.c inffast.c inflate.c inftrees.c trees.c zutil.c` of the verified cache with `Z_SOLO` as a public definition and none of the forbidden definitions, and its own `CMakeLists.txt` is never configured.
- **Covers:** Z2-I-COMPRESS-02, Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Build` in every preset; `Test_CompressionZlib_Allocations_RouteThroughAllocator`; `Setup-Zenith2.ps1 -Check` still verifying the cache after a configure (no file renamed).
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Third-party integration of zlib"; Zenith2/Docs/Design/COMPRESS.md → "Design rulings owned by COMPRESS"

### Z2-R-COMPRESS-018 zstd pinned at 1.5.7 with fixed options

- **Statement:** The `zstd` manifest entry pins version 1.5.7, the archive URL of the design, licence `BSD-3-Clause`, `cmakeSourceDir` `build/cmake`, target `libzstd_static` and the option set of the design (legacy support, multithreading, programs, tests and shared builds off), with the archive and licence SHA-256 values verified against the downloaded archive by the Stage 0 reproducibility-manifest ticket (the design's values are candidates) and re-verified by `Setup-Zenith2.ps1 -Check`; `Zenith_ZstdInclude.h` asserts `ZSTD_VERSION_NUMBER == 10507`.
- **Covers:** Z2-I-COMPRESS-03, Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Setup-Zenith2.ps1 -Check` exit 0; `Test_CompressionZstd_LinkedVersion_MatchesPin`; `Test_CompressionZstd_LegacyMagic_IsCorrupt` (legacy support compiled out).
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Third-party integration of zstd"

### Z2-R-COMPRESS-019 Libraries built from source in sanitizer presets

- **Statement:** In `win-asan`, `linux-tsan` and the `Hwasan` configuration zlib and zstd are compiled from the pinned sources with the sanitizer's flags, and neither has a `prebuilt` manifest entry.
- **Covers:** Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` configure and build; the Stage 2 acceptance's HWASan build type.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Third-party integration of zstd"; Zenith2/Docs/Design/COMPRESS.md → "Sanitizer coverage"

### Z2-R-COMPRESS-020 One copy of each library per link

- **Statement:** No Zenith2 target links a second copy of zlib or zstd symbols: the `win-tools` link of `ZenithCook`, which also links KTX-Software, reports no duplicate `ZSTD_`, `ZDICT_` or zlib symbol diagnostic (`LNK2005`, `LNK4006`).
- **Covers:** Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** The `Build` step's `win-tools` link log of the ASSET ticket that integrates KTX-Software into ZenithCook, checked by that ticket's DoD.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Third-party integration of zstd"

### Z2-R-COMPRESS-021 Library allocations routed through the adapter

- **Statement:** Every allocation zlib or zstd makes during compression and decompression goes through `Zenith_CompressionAllocator` (zlib `zalloc`/`zfree` thunks, zstd `ZSTD_customMem`), to the root heap's `TryAllocate` for contexts and dictionaries and to the caller's scratch arena for one-shot calls; only dictionary training uses the C runtime allocator, because zstd's dictionary builder offers no hook.
- **Covers:** Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionZlib_Allocations_RouteThroughAllocator`, `Test_CompressionZstd_Allocations_RouteThroughAllocator` and `Test_CompressionConformance_LibraryAllocations_AreAccounted`, `Test` step; `ForbiddenCheck` (no `malloc` in COMPRESS sources).
- **Design:** Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionAllocator.h"; Zenith2/Docs/Design/COMPRESS.md → "Dictionary creation and training"

### Z2-R-COMPRESS-022 Allocation adapter behaviour

- **Statement:** `Zenith_CompressionAllocator` returns 16-byte-aligned blocks, returns nullptr (never asserting, never invoking the OOM handler) on heap or arena failure, on size overflow and when the fault injector fails `uFAULT_POINT_COMPRESSION_LIBRARY_ALLOCATE`, tracks live and high-water bytes, and in arena mode releases nothing on `Free`.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionAllocator_HeapMode_TracksLiveAndHighWater`, `…_ArenaMode_FreeReleasesNothing`, `…_Alignment_Is16`, `…_FaultPoint_ReturnsNull`, `…_HeapFailure_ReturnsNull`, `…_DestroyWithLiveBytes_Checks`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionAllocator.h"; Zenith2/Docs/Design/COMPRESS.md → "Locks and blocking"

### Z2-R-COMPRESS-023 zlib stream format

- **Statement:** The zlib backend writes one RFC 1950 stream per frame (CM 8, CINFO 7, FDICT 0, Adler-32) and its decoder accepts only zlib streams with CM 8 and CINFO ≤ 7, refusing FDICT streams with `UNSUPPORTED` and gzip or raw deflate with `CORRUPT_INPUT`.
- **Covers:** Z2-I-COMPRESS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionZlib_HeaderRules_EachRejects`, `Test_CompressionZlib_GzipAndRawDeflate_AreRejected`, `Test_CompressionZlib_ZeroLength_IsEightBytes`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "zlib stream format as written"; Zenith2/Docs/Design/COMPRESS.md → "One-shot decompression with zlib"

### Z2-R-COMPRESS-024 zstd frame format

- **Statement:** The zstd backend writes exactly one RFC 8878 frame per stream with the content checksum always, the content size whenever the size is known, and the dictionary ID whenever a dictionary is used; its decoder refuses skippable frames, legacy formats and a second frame.
- **Covers:** Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionZstd_FrameHeader_ChecksumSizeDictionary`, `…_TwoFrames_AreTrailingData`, `…_SkippableFrame_IsCorrupt`, `…_LegacyMagic_IsCorrupt`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "zstd frame format as written"; Zenith2/Docs/Design/COMPRESS.md → "One-shot decompression with zstd"

### Z2-R-COMPRESS-025 zstd decode window cap

- **Statement:** Every zstd decoder (one-shot and streaming) refuses a frame whose window exceeds 2^23 bytes with `COMPRESSION_ERROR_LIMIT_EXCEEDED` before allocating a window, and every zstd level 1–19 compresses with a window of at most 2^23 bytes.
- **Covers:** Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionZstd_WindowTooLarge_IsLimitExceeded` and `Test_CompressionZstd_EveryLevel_WindowAtMost8MiB` (including a pledged 9 MiB Single_Segment frame at level 19), `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "zstd frame format as written"; Zenith2/Docs/Design/COMPRESS.md → "Untrusted input rules"

### Z2-R-COMPRESS-026 zlib error mapping

- **Statement:** Every zlib result code maps to the `Zenith_CompressionError` of the design's zlib table (including `Z_BUF_ERROR` by context), unknown values map to `INTERNAL`, and each `INTERNAL` or `CORRUPT_INPUT` is logged with zlib's message.
- **Covers:** Z2-I-COMPRESS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionZlib_ErrorMapping_EveryCode`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Error mapping for zlib"

### Z2-R-COMPRESS-027 zstd error mapping

- **Statement:** Every `ZSTD_ErrorCode` maps to the `Zenith_CompressionError` of the design's zstd table, unknown codes map to `INTERNAL`, and each `INTERNAL`, `CORRUPT_INPUT` or `INVALID_DICTIONARY` is logged with `ZSTD_getErrorName`.
- **Covers:** Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionZstd_ErrorMapping_EveryCode`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Error mapping for zstd"

### Z2-R-COMPRESS-028 zlib 64-bit counting on LLP64

- **Statement:** The zlib backend passes at most 2^30 bytes per `avail_in`/`avail_out` and keeps its own `uint64` counters, never reading `total_in` or `total_out` (32-bit `uLong` on Windows).
- **Covers:** Z2-I-COMPRESS-02, Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_LargeStream_Past4GiB_RoundTrips[Zlib]` (`z2-stress`, level 1, `Zenith_CompressionConformanceCorpusStream`), run only in `win-asan` through CORE's `if constexpr (Zenith_GetSanitizerKind() != SANITIZER_KIND_ASAN)` idiom by `Sanitizers -Preset win-asan` (change-triggered through `Zenith2Memory`, and every stage acceptance) within its 480-second budget; the one-shot inflate's 2^30-byte clamp is review-verified, because no test holds a buffer larger than 2^30 bytes.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "One-shot decompression with zlib"; Zenith2/Docs/Design/COMPRESS.md → "Conformance cases"

### Z2-R-COMPRESS-029 Streaming context creation

- **Statement:** `Zenith_CompressionContext::Create` validates its desc by the rules of the design's table in order, returning the tabled error for the first failure, and otherwise builds the context in the tabled steps, consulting each step's fault point first.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_DescValidation_EachRule` and `Test_CompressionContext_Create_FaultSweep_Unwinds`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Context desc validation"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionContext.h"

### Z2-R-COMPRESS-030 Context creation failure unwinds

- **Statement:** A context creation that fails at any step, or at any single allocation, returns `OUT_OF_MEMORY` (or `INTERNAL` for a non-allocation library-init failure) and leaves no allocation, no live context and no live engine behind.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_Create_FaultSweep_Unwinds` and `Test_CompressionConformance_AllocationFailure_CreateFaultSweep` per backend, `Test` step and `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Context desc validation"; Zenith2/Docs/Design/COMPRESS.md → "Failure table"

### Z2-R-COMPRESS-031 Bounded work per streaming call

- **Statement:** Each `Process`, `Flush` and `End` call performs at most one engine step, consuming at most 131,072 bytes of input per step.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_Process_AtMostOneStep`, `Test` step; the benchmark's per-call p99.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Compression driver loop"; Zenith2/Docs/Design/COMPRESS.md → "Frame-graph placement"

### Z2-R-COMPRESS-032 Bounded output

- **Statement:** A context never writes beyond the output span of a call; output the span cannot hold is kept in the context's staging (compress) or the library (decompress) and delivered by subsequent calls.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_Output_NeverPastSpan` and `Test_CompressionConformance_StreamingOutputChunking_DoesNotChangeBytes`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Compression driver loop"; Zenith2/Docs/Design/COMPRESS.md → "Streaming decompression context state machine"

### Z2-R-COMPRESS-033 Canonical input chunking

- **Statement:** For compression, the engine receives input only as the unconsumed tail of one 131,072-byte canonical chunk (a shorter last chunk only at a flush or end), with empty input for flush and end steps, so the sequence of engine calls is the same for every slicing of the caller's input.
- **Covers:** Z2-I-COMPRESS-04, Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_InputSlicing_DoesNotChangeEngineCalls` (recording fake) and `Test_CompressionConformance_StreamingInputChunking_DoesNotChangeBytes` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Canonical chunking"; Zenith2/Docs/Design/COMPRESS.md → "Compression driver loop"

### Z2-R-COMPRESS-034 Fixed output staging

- **Statement:** Every compression engine step writes into the empty 131,072-byte output staging buffer and bytes reach the caller only by copy, so the sequence of engine calls is the same for every size of the caller's output spans.
- **Covers:** Z2-I-COMPRESS-04, Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_OutputSlicing_DoesNotChangeEngineCalls`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Canonical chunking"; Zenith2/Docs/Design/COMPRESS.md → "Staging buffers and context layout"

### Z2-R-COMPRESS-035 zstd pledged size never derived from end

- **Statement:** The zstd compress engine makes an empty `ZSTD_e_continue` call at creation and after every reset, and never passes input with `ZSTD_e_end`, so zstd never derives a pledged size from the end call.
- **Covers:** Z2-I-COMPRESS-03, Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_OneShotEqualsStreamingWithPledgedSize[Zstd]` and `Test_CompressionZstd_FrameHeader_ChecksumSizeDictionary` (unpledged frames carry no content size), `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Canonical chunking"; Zenith2/Docs/Design/COMPRESS.md → "Context desc validation"

### Z2-R-COMPRESS-036 Streaming compression state machine

- **Statement:** A compression context follows the design's compression state table exactly: every non-check cell gives its transition and result, and every `ZENITH_CHECK` cell terminates through `ZENITH_CHECK`.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_StateTable_EveryNonCheckCell`, `Test_CompressionContext_ProcessAfterEnd_Checks`, `Test_CompressionContext_ProcessDuringFlush_Checks` (`z2-death-check`), `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Streaming compression context state machine"

### Z2-R-COMPRESS-037 Streaming decompression state machine

- **Statement:** A decompression context follows the design's decompression state table exactly, including `TRAILING_DATA` for input after the frame end and `TRUNCATED_INPUT` from `End` before the frame end; `Flush` on it is a `ZENITH_CHECK` failure.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_StateTable_EveryNonCheckCell`, `Test_CompressionContext_DecodeTrailingData_Fails`, `Test_CompressionContext_DecodeEndBeforeFrameEnd_IsTruncated`, `Test_CompressionContext_FlushOnDecompress_Checks`, `Test` step. `Test_CompressionConformance_StreamingDecompress_AnySlicing_RoundTrips` per backend.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Streaming decompression context state machine"

### Z2-R-COMPRESS-038 Flush yields a decodable prefix

- **Statement:** After a `Flush` reports `m_bFinished`, the bytes delivered so far decode to exactly the input consumed so far, and chunk boundaries restart at the flush offset.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Flush_ProducesDecodablePrefix` per backend and `Test_CompressionContext_Flush_RestartsSegment`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Compression driver loop"; Zenith2/Docs/Design/COMPRESS.md → "Streaming compression context state machine"

### Z2-R-COMPRESS-039 Pledged size enforced

- **Statement:** A compression context with a pledged size fails with `SIZE_MISMATCH` when input would exceed it (in `Process`) or falls short of it (at `End`, before the end step), so no frame with a wrong recorded size is completed.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_PledgedSize_Mismatch_Fails` and `Test_CompressionConformance_PledgedSize_Mismatch_IsRejected` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Compression driver loop"; Zenith2/Docs/Design/COMPRESS.md → "Streaming compression context state machine"

### Z2-R-COMPRESS-040 Decoded size cap and expected size

- **Statement:** A decompression context never keeps a decoded byte beyond `m_ulMaxDecodedBytes` or `m_ulExpectedDecodedBytes`, failing with `LIMIT_EXCEEDED` or `SIZE_MISMATCH`, and at the frame end a declared expected size must equal the decoded total.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_DecodeCap_DetectsOverLongFrame` and `Test_CompressionConformance_DecodedSizeCap_IsEnforced` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Streaming decompression context state machine"; Zenith2/Docs/Design/COMPRESS.md → "Untrusted input rules"

### Z2-R-COMPRESS-041 Sticky errors

- **Statement:** After a context call fails, the context is FAILED and every subsequent `Process`, `Flush` and `End` returns the first error until `Reset`.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_StickyError_RepeatsFirstError`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Streaming compression context state machine"; Zenith2/Docs/Design/COMPRESS.md → "Streaming decompression context state machine"

### Z2-R-COMPRESS-042 Reset reuses a context

- **Statement:** `Reset` returns a context in any state to READY with zero totals, keeps its allocations, and a reset context produces exactly the bytes a fresh context with the same desc produces.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_Reset_FromEveryState_ReturnsReady`, `Test_CompressionContext_ResetReuse_MatchesFreshContext`, `Test_CompressionConformance_Cancellation_ResetThenReuse_IdenticalOutput`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Streaming compression context state machine"; Zenith2/Docs/Design/COMPRESS.md → "Cancellation"

### Z2-R-COMPRESS-043 Totals exact beyond 4 GiB

- **Statement:** Context totals are `uint64` and exact for streams longer than 4 GiB, and a 4 GiB + 1 MiB stream round-trips through both codecs.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_TotalsPast4GiB_AreExact` (fake engine, `Test` step); `Test_CompressionConformance_LargeStream_Past4GiB_RoundTrips` per backend (`z2-stress`, level 1, synthetic streaming source; about 140 s for `[Zlib]` and 70 s for `[Zstd]`), run only in `win-asan` through CORE's single-preset idiom by `Sanitizers -Preset win-asan` and every stage acceptance, each entry within its 480-second budget.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Conformance cases"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionContext.h"

### Z2-R-COMPRESS-044 Context memory bound

- **Statement:** `GetContextMemoryBound(desc)` is an upper bound of the context's `GetMemoryBytes()` after every call, and `GetMemoryBytes()` equals the context's actual heap use (object, staging, engine and library blocks).
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_ContextMemoryBoundHolds` and `Test_CompressionConformance_LibraryAllocations_AreAccounted` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Memory per context"; Zenith2/Docs/Design/COMPRESS.md → "Scratch and memory bounds"

### Z2-R-COMPRESS-045 Stuck engine detection

- **Statement:** A decompression step that consumes and produces nothing with non-empty input and output and no frame end fails the context with `COMPRESSION_ERROR_INTERNAL` and a log line.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionContext_StuckEngine_IsInternal`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Streaming decompression context state machine"

### Z2-R-COMPRESS-046 One-shot compression

- **Statement:** `Zenith_Compression::Compress` compresses a whole source into a destination with scratch from the caller's arena and returns the compressed size, or `DESTINATION_TOO_SMALL` without writing past the destination.
- **Covers:** Z2-I-COMPRESS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_RoundTrip_CorpusThreeLevels` and `…_DestinationTooSmall_NoWritePastEnd` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "One-shot compression"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_Compression.h"

### Z2-R-COMPRESS-047 One-shot decompression

- **Statement:** `Zenith_Compression::Decompress` decodes exactly one frame occupying the whole source into the destination and returns the decoded size, with `TRUNCATED_INPUT`, `TRAILING_DATA`, `CORRUPT_INPUT` or `DESTINATION_TOO_SMALL` for the corresponding inputs.
- **Covers:** Z2-I-COMPRESS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Truncated_EveryPrefix_IsTruncated`, `…_TrailingData_IsRejected`, `…_Corrupt_BitFlips_DetectedOrHarmless`, `…_DestinationTooSmall_NoWritePastEnd`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "One-shot decompression with zlib"; Zenith2/Docs/Design/COMPRESS.md → "One-shot decompression with zstd"

### Z2-R-COMPRESS-048 One-shot calls restore the scratch arena

- **Statement:** Every one-shot call (success or failure) destroys its engine and resets the scratch arena to the marker it took on entry before returning.
- **Covers:** Z2-I-COMPRESS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Compression_OneShot_RestoresArenaMarker`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "One-shot compression"; Zenith2/Docs/Design/COMPRESS.md → "Programmer errors"

### Z2-R-COMPRESS-049 Scratch bounds are sufficient

- **Statement:** A one-shot call succeeds with an arena capped at exactly `GetCompressScratchBound` or `GetDecompressScratchBound` for that size, level and dictionary, and fails with `OUT_OF_MEMORY` (leaving the arena at its marker) with a smaller arena that cannot hold a required allocation.
- **Covers:** Z2-I-COMPRESS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_ScratchBoundIsSufficient`, `Test_CompressionConformance_AllocationFailure_OneShotArena_IsOutOfMemory`, `Test_Compression_OneShot_ArenaTooSmall_IsOutOfMemory`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Scratch and memory bounds"; Zenith2/Docs/Design/COMPRESS.md → "One-shot compression"

### Z2-R-COMPRESS-050 Compress bound

- **Statement:** `GetCompressBound(n)` is at least the one-shot compressed size of every input of `n` bytes at every level, and returns `SIZE_OVERFLOW` when the bound does not fit a `uint64`.
- **Covers:** Z2-I-COMPRESS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_CompressBound_Holds` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Scratch and memory bounds"

### Z2-R-COMPRESS-051 One-shot equals pledged streaming

- **Statement:** For every input and level, one-shot `Compress` output is byte-identical to the output of a compression context created with `m_ulPledgedSourceBytes` equal to the input size and never flushed.
- **Covers:** Z2-I-COMPRESS-05, Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_OneShotEqualsStreamingWithPledgedSize` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "One-shot compression"; Zenith2/Docs/Design/COMPRESS.md → "Canonical chunking"

### Z2-R-COMPRESS-052 Zero-length input

- **Statement:** A zero-length input compresses, one-shot and streaming, to a valid frame (8 bytes for zlib) that decodes to zero bytes.
- **Covers:** Z2-I-COMPRESS-04, Z2-I-COMPRESS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_RoundTrip_ZeroLength` per backend and `Test_CompressionZlib_ZeroLength_IsEightBytes`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "zlib stream format as written"; Zenith2/Docs/Design/COMPRESS.md → "zstd frame format as written"

### Z2-R-COMPRESS-053 Frame inspection

- **Statement:** `ReadFrameInfo` reports the codec, window size, header size, checksum flag and, for zstd, the content size and dictionary ID, without decoding and without allocating, and returns `TRUNCATED_INPUT` or `CORRUPT_INPUT` for a short or malformed header.
- **Covers:** Z2-I-COMPRESS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_FrameInfo_ReportsHeader` per backend and `Test_CompressionZlib_FrameInfo_WindowFromCinfo`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionTypes.h"; Zenith2/Docs/Design/COMPRESS.md → "One-shot decompression with zstd"

### Z2-R-COMPRESS-054 Section compression

- **Statement:** `Zenith_CompressSection` requires a destination of at least the compress bound (raised to the source size when storing is allowed), compresses, stores the source as codec NONE when the compressed size is not smaller and storing is allowed, and returns the section descriptor; `Zenith_StoreSection` writes a NONE section for the setting `none`.
- **Covers:** Z2-I-COMPRESS-05, Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressSection_Incompressible_StoresRaw`, `…_StoreDisabled_KeepsCompressed`, `…_DestinationBelowBound_IsRejected`, `Test_StoreSection_CopiesAndDescribesNone`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Section compression and decompression"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressedSection.h"

### Z2-R-COMPRESS-055 Section decompression

- **Statement:** `Zenith_DecompressSection` validates the descriptor, requires the stored span to equal the stored size, selects the codec from the descriptor through the codec set, resolves a dictionary by ID from the supplied span, requires a zstd content checksum, and decodes exactly the declared number of bytes or fails with the tabled error.
- **Covers:** Z2-I-COMPRESS-05, Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_DecompressSection_None_CopiesExactly`, `…_StoredSizeMismatch_IsRejected`, `…_DictionaryMissing_IsMismatch`, `…_DictionarySpanRules_AreEnforced`, `…_UnknownCodec_IsRejected`, `…_ZstdWithoutChecksum_IsCorrupt`, `…_ContentSizeDiffers_IsSizeMismatch`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Section compression and decompression"

### Z2-R-COMPRESS-056 Determinism definition

- **Statement:** For a codec at a resolved level with an optional dictionary, compressed bytes depend only on the input bytes, the flush offsets, whether a pledged size was declared, and the library build, and are identical across repetitions, codec instances, threads and processes.
- **Covers:** Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Deterministic_RepeatedRunsAndInstances` and `Test_CompressionConformance_ThreadSafety_SharedCodecConcurrentOneShot` per backend, `Test` step and `Sanitizers -Preset linux-tsan`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Determinism definition and parameter pinning"

### Z2-R-COMPRESS-057 Output independent of caller slicing

- **Statement:** Compressed bytes are identical for every slicing of input across `Process` calls and every size of output spans.
- **Covers:** Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_StreamingInputChunking_DoesNotChangeBytes` and `…_StreamingOutputChunking_DoesNotChangeBytes` per backend, `Test` step. `Test_CompressionConformance_RoundTrip_ChunkBoundaries` per backend covers the chunk-boundary sizes.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Canonical chunking"; Zenith2/Docs/Design/COMPRESS.md → "Determinism definition and parameter pinning"

### Z2-R-COMPRESS-058 Flush offsets determine bytes

- **Statement:** Two compression streams of the same input flushed at the same offsets produce identical bytes.
- **Covers:** Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Flush_SameOffsets_SameBytes` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Determinism definition and parameter pinning"

### Z2-R-COMPRESS-059 zlib parameters pinned

- **Statement:** The zlib deflate engine calls `deflateInit2(level, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY)` and never `deflateParams`, `deflateTune`, `deflateSetDictionary`, `deflatePrime`, `deflateSetHeader` or `deflateCopy`, with the flush mode fixed per directive.
- **Covers:** Z2-I-COMPRESS-02, Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_GoldenVectors_Match[Zlib]`, `Test` step; code review of `Zenith_CompressionZlibEngine.cpp`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Determinism definition and parameter pinning"

### Z2-R-COMPRESS-060 zstd parameters pinned

- **Statement:** The zstd compress engine sets exactly the parameters of the design's zstd table (level, checksum, content size, dictionary ID, zero workers, long-distance matching disabled, row match finder enabled, `ZSTD_f_zstd1`) and leaves the others at their level-derived values.
- **Covers:** Z2-I-COMPRESS-03, Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionZstd_Parameters_ArePinned` and `Test_CompressionConformance_GoldenVectors_Match[Zstd]`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Determinism definition and parameter pinning"

### Z2-R-COMPRESS-061 Golden vectors on host builds

- **Statement:** The committed golden fixture's compressed sizes and FNV-1a 64 hashes match on `win-debug`, `win-release`, `win-asan` and `linux-tsan`, and its `library` lines equal the linked libraries' versions; the fixture is re-recorded only through the test-only environment variable `ZENITH2_COMPRESSION_RECORD_GOLDEN`, which no shipping target reads.
- **Covers:** Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_GoldenVectors_Match` and `…_GoldenVectors_LibraryVersionsMatch` per backend, `Test` step and both `Sanitizers` steps; `TestSeparationCheck` (no test-only switch in a shipping target) and review that the variable is read only under `Tests/Conformance/`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Golden vectors"; Zenith2/Docs/Design/COMPRESS.md → "Golden vector fixture format"

### Z2-R-COMPRESS-062 Golden vectors on the S24

- **Statement:** The golden fixture matches on the S24 in the Android Release build.
- **Covers:** Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The Stage 2 acceptance device run of `ZenithCompression_Conformance` on the S24 (a `needs-human` step), its log committed with the acceptance evidence.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Golden vectors"; Zenith2/Docs/Design/COMPRESS.md → "Stage table"

### Z2-R-COMPRESS-063 Pin changes re-record and re-cook

- **Statement:** A change to a zlib or zstd version pin or to any pinned parameter lands in one diff with the manifest entry, the compile-time version asserts, the re-recorded golden fixture, a benchmark re-run and the engine cook-version bump.
- **Covers:** Z2-I-COMPRESS-06, Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-082, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_GoldenVectors_LibraryVersionsMatch` fails any pin change that did not re-record; review of the upgrade diff against "Re-cook and migration".
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Re-cook and migration"; Zenith2/Docs/Design/COMPRESS.md → "Golden vector fixture format"

### Z2-R-COMPRESS-064 No library-internal threads or clocks

- **Statement:** Neither library is built with multithreading support and no COMPRESS code reads a clock, a random source or thread identity, so compressed bytes cannot depend on scheduling.
- **Covers:** Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionZstd_Parameters_ArePinned` (`nbWorkers` 0) and `ForbiddenCheck` rule F14 over COMPRESS sources.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Third-party integration of zstd"; Zenith2/Docs/Design/COMPRESS.md → "Determinism definition and parameter pinning"

### Z2-R-COMPRESS-065 Codec ID registry values

- **Statement:** `Zenith_CompressionCodecID` defines NONE 0, ZLIB 1, ZSTD 2; values 3–127 are reserved, 128–254 experimental and 255 invalid; no value is ever renumbered or reused; and a new codec lands in one diff with its registry row, its enumerator, its backend library, its conformance adapter and its `Z2-D-` decision.
- **Covers:** Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionTypes_ErrorValues_AreStable` (`static_assert`s of the codec IDs and range constants), `Test` step; review of any diff adding an enumerator against the one-diff rule.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Codec ID registry"

### Z2-R-COMPRESS-066 Descriptor on-disk layout

- **Statement:** `Zenith_CompressedSectionDesc` is serialized as the 24-byte little-endian record of the design (codec, level, flags, dictionary ID, stored bytes, decoded bytes), encoded and decoded field by field.
- **Covers:** Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressedSectionDesc_Encode_MatchesWorkedExample` and `Test_CompressedSectionDesc_EncodeDecode_RoundTrip`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Compressed section descriptor on disk"

### Z2-R-COMPRESS-067 Descriptor validation

- **Statement:** `Zenith_ValidateCompressedSectionDesc` applies the ten rules of the design in order and returns the tabled error for the first failure; decode of fewer than 24 bytes is `TRUNCATED_INPUT`; encode into fewer than 24 bytes is `DESTINATION_TOO_SMALL`.
- **Covers:** Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressedSectionDesc_ValidationRules_EachRejects`, `…_DecodeShort_IsTruncated`, `…_EncodeShort_IsTooSmall`, `…_SizeLimits_AtAndAboveLimit`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Descriptor validation"

### Z2-R-COMPRESS-068 Cooked formats embed the descriptor

- **Statement:** Every compressed section of every cooked asset file, archive entry, save and replay is described by one descriptor in its container's header or table of contents, and its stored bytes are exactly one stream, one frame or the raw payload.
- **Covers:** Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** The ASSET, PACK and SAVE cooked-format tickets' format tests (each container's fixture shows the 24-byte record per section), recorded in their DoD; the cross-area notes carry the request.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "How cooked formats embed the descriptor"

### Z2-R-COMPRESS-069 Readers select the codec from the data

- **Statement:** A reader of cooked data selects the codec only from the section descriptor through a `Zenith_CompressionCodecSet`, never from configuration, and an unknown or reserved codec ID is the recoverable `UNKNOWN_CODEC`.
- **Covers:** Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_DecompressSection_UnknownCodec_IsRejected`, `Test_CompressedSectionDesc_ValidationRules_EachRejects` (rule 1), `Test` step; review of the ASSET loader ticket.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "How cooked formats embed the descriptor"; Zenith2/Docs/Design/COMPRESS.md → "Section compression and decompression"

### Z2-R-COMPRESS-070 Codec names and setting validation

- **Statement:** `Zenith_ParseCompressionCodecName` accepts exactly `none`, `zlib` and `zstd`, and `Zenith_ValidateCompressionSetting` returns the result of the design's setting table for every codec, level and dictionary combination.
- **Covers:** Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionTypes_ParseCodecName_AcceptsExactNames` and `Test_CompressionTypes_ValidateSetting_EveryRow`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Compression setting fragment"; Zenith2/Docs/Design/COMPRESS.md → "Editor surfaces"

### Z2-R-COMPRESS-071 Library version exposed for cache keys

- **Statement:** `GetInfo().m_szLibraryVersion` returns the linked library's runtime version string, so the cook keys compressed derived data on codec, level, dictionary ID and library version.
- **Covers:** Z2-I-COMPRESS-06, Z2-I-COMPRESS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionZlib_LinkedVersion_MatchesPin`, `Test_CompressionZstd_LinkedVersion_MatchesPin`, `Test` step; ASSET's derived-data-cache ticket cites the field.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Cook integration"; Zenith2/Docs/Design/COMPRESS.md → "Codec metadata table"

### Z2-R-COMPRESS-072 Dictionary creation

- **Statement:** `CreateDictionary` on zstd accepts only zstd-format dictionaries (magic `0xEC30A437`, ID in [32,768, 2,147,483,647], at most 1 MiB), binds the dictionary to a resolved level, and returns `INVALID_DICTIONARY`, `LIMIT_EXCEEDED` or `OUT_OF_MEMORY` as the design's steps state.
- **Covers:** Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionZstd_DictionaryID_RangeEnforced`, `Test_CompressionZstd_RawContentDictionary_IsRejected`, `Test_CompressionConformance_AllocationFailure_CreateFaultSweep[Zstd]`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Dictionary creation and training"; Zenith2/Docs/Design/COMPRESS.md → "Dictionary format and identifiers"

### Z2-R-COMPRESS-073 Dictionaries unsupported on zlib

- **Statement:** On zlib, `CreateDictionary` and `TrainDictionary` return `UNSUPPORTED` without allocating, and a context or call given a dictionary returns `UNSUPPORTED`.
- **Covers:** Z2-I-COMPRESS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Dictionary_CapabilityMatchesInfo[Zlib]` and `…_ThreadSafety_SharedDictionary[Zlib]`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Dictionary creation and training"; Zenith2/Docs/Design/COMPRESS.md → "Codec metadata table"

### Z2-R-COMPRESS-074 Dictionary round trip

- **Statement:** A zstd dictionary compresses and decompresses through the one-shot, section and streaming paths, and its ID appears in every frame and section descriptor that used it.
- **Covers:** Z2-I-COMPRESS-03, Z2-I-COMPRESS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Dictionary_CapabilityMatchesInfo[Zstd]`, `Test_CompressionZstd_FrameHeader_ChecksumSizeDictionary`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Dictionary format and identifiers"; Zenith2/Docs/Design/COMPRESS.md → "zstd frame format as written"

### Z2-R-COMPRESS-075 Dictionary mismatch detected

- **Statement:** Decoding a dictionary frame without its dictionary, with a different dictionary, or with a dictionary of another codec returns `DICTIONARY_MISMATCH` before decoding.
- **Covers:** Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Dictionary_WrongOrMissing_IsMismatch[Zstd]`, `Test_DecompressSection_DictionaryMissing_IsMismatch`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "One-shot decompression with zstd"; Zenith2/Docs/Design/COMPRESS.md → "Section compression and decompression"

### Z2-R-COMPRESS-076 Dictionary level must match

- **Statement:** Compressing with a dictionary at a level other than the dictionary's returns `INVALID_LEVEL`; level 0 uses the dictionary's level.
- **Covers:** Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Dictionary_LevelMustMatch[Zstd]`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "One-shot compression"; Zenith2/Docs/Design/COMPRESS.md → "Context desc validation"

### Z2-R-COMPRESS-077 Deterministic dictionary training

- **Statement:** `TrainDictionary` with the same samples in the same order, capacity and level produces identical dictionary bytes and ID, using the pinned fast-cover parameters, and enforces the sample-count, total-size and capacity limits.
- **Covers:** Z2-I-COMPRESS-03, Z2-I-COMPRESS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Dictionary_TrainingIsDeterministic[Zstd]` and `…_Dictionary_TrainingLimits[Zstd]`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Dictionary creation and training"; Zenith2/Docs/Design/COMPRESS.md → "Dictionary training in the cook"

### Z2-R-COMPRESS-078 Dictionaries shared across threads

- **Statement:** One zstd dictionary is used concurrently by contexts and one-shot calls on several threads without data races.
- **Covers:** Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_ThreadSafety_SharedDictionary[Zstd]` under `Sanitizers -Preset linux-tsan`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Thread affinity per type"; Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionDictionary.h"

### Z2-R-COMPRESS-079 Shared codec is thread-safe

- **Statement:** Concurrent one-shot calls on one codec object from 2, 8 and hardware-concurrency threads, each with its own arena, give results identical to a serial run and no TSan report.
- **Covers:** Z2-I-COMPRESS-01, Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_ThreadSafety_SharedCodecConcurrentOneShot` per backend under `Sanitizers -Preset linux-tsan`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Thread affinity per type"

### Z2-R-COMPRESS-080 Context is single-owner

- **Statement:** A context used by one thread at a time, handed between threads through a synchronised hand-off, and one context per thread in parallel, produce correct bytes with no TSan report.
- **Covers:** Z2-I-COMPRESS-04, Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_ThreadSafety_ContextHandOff` and `…_ThreadSafety_ContextPerThread` per backend under `Sanitizers -Preset linux-tsan`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Thread affinity per type"

### Z2-R-COMPRESS-081 No locks and no blocking

- **Statement:** COMPRESS creates no `Zenith_Mutex`, adds no lock-order row, starts no thread and calls no blocking primitive, and every COMPRESS allocation uses `TryAllocate`, never the OOM handler.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** Review of the three libraries' `Internal/` sources; `Test_CompressionAllocator_HeapFailure_ReturnsNull` (no OOM handler), `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Locks and blocking"; Zenith2/Docs/Design/COMPRESS.md → "Frame-graph placement"

### Z2-R-COMPRESS-082 Codec outlives its objects

- **Statement:** Each backend's final class owns `mutable std::atomic<uint32> m_uLiveObjects` and an Internal-only `AdjustLiveObjects(sint32)`; every engine (streaming and one-shot) and dictionary of that backend calls `AdjustLiveObjects(+1)` as the first statement of its constructor and `AdjustLiveObjects(−1)` as the last statement of its destructor (relaxed increments, release decrements), so an object destroyed after a failed library init balances the count; contexts are counted through the one engine each owns; the backend destructor fails `ZENITH_CHECK` on an acquire load that is not zero; and the Player destroys the codecs after the asset registry, the save system and the codec set.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Compression_DestroyCodecWithLiveContext_Checks` (`z2-death-check`) and its companion default-suite case (clean destruction once the context is gone); `Test_CompressionContext_Create_FaultSweep_Unwinds` and `Test_CompressionConformance_AllocationFailure_CreateFaultSweep`, whose init-failure branches end with the codec destroyed cleanly (`Test` step); MOD's shutdown-order tests.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Lifetime and shutdown order"; Zenith2/Docs/Design/COMPRESS.md → "Programmer errors"

### Z2-R-COMPRESS-083 Truncated input detected

- **Statement:** Every proper prefix of a frame is refused with `TRUNCATED_INPUT` by one-shot decompression and by a decompression context's `End`.
- **Covers:** Z2-I-COMPRESS-05, Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Truncated_EveryPrefix_IsTruncated` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Failure table"; Zenith2/Docs/Design/COMPRESS.md → "One-shot decompression with zlib"; Zenith2/Docs/Design/COMPRESS.md → "One-shot decompression with zstd"

### Z2-R-COMPRESS-084 Corrupt input detected or harmless

- **Statement:** A single-bit flip anywhere in a frame makes decoding fail or succeed with byte-identical output, never succeed with different output, never crash and never write outside the destination; random bytes never decode successfully.
- **Covers:** Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Corrupt_BitFlips_DetectedOrHarmless` and `…_Corrupt_RandomBytes_AreRejected` per backend, `Test` step and `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Failure table"; Zenith2/Docs/Design/COMPRESS.md → "Untrusted input rules"

### Z2-R-COMPRESS-085 Destination never overrun

- **Statement:** No compress or decompress path writes outside its destination span for any input; a destination one byte too small yields `DESTINATION_TOO_SMALL` with 64 guard bytes after it unchanged.
- **Covers:** Z2-I-COMPRESS-05, Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_DestinationTooSmall_NoWritePastEnd` per backend, `Test` step and `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Untrusted input rules"; Zenith2/Docs/Design/COMPRESS.md → "Failure table"

### Z2-R-COMPRESS-086 Mid-stream allocation failure

- **Statement:** A library allocation failing during a stream step fails the context with `OUT_OF_MEMORY`, and destroying it afterwards leaks nothing.
- **Covers:** Z2-I-COMPRESS-04, Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_AllocationFailure_MidStream_IsOutOfMemory` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Context desc validation"; Zenith2/Docs/Design/COMPRESS.md → "Failure table"

### Z2-R-COMPRESS-087 Cancellation releases everything

- **Statement:** Destroying a context in any state after any number of chunks returns the root heap's live allocations to their value before creation, and the bytes delivered before cancellation never decode as a complete frame.
- **Covers:** Z2-I-COMPRESS-04, Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Cancellation_DestroyAtEveryState_ReleasesAll`, `…_Cancellation_PartialOutput_NeverDecodesComplete`, `Test_CompressionContext_Destroy_FromEveryState_ReleasesAll`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Cancellation"

### Z2-R-COMPRESS-088 Cancellation stress

- **Statement:** 1,000 cycles of create, feed 0–256 KiB of `mixed` in seeded random slices, and destroy or reset at seeded random points, in both modes at the default level and on both codecs, leave no live allocation and no live context.
- **Covers:** Z2-I-COMPRESS-04, Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Cancellation_StressThousandCycles` (`z2-stress`) per backend, `Test` step and `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Cancellation"; Zenith2/Docs/Design/COMPRESS.md → "Conformance cases"

### Z2-R-COMPRESS-089 Untrusted input never asserts

- **Statement:** No property of compressed bytes, descriptors, dictionary bytes or settings reaches an assert; every such failure is a `Zenith_Result` error, and every size check uses overflow-checked 64-bit arithmetic.
- **Covers:** Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** The six fuzz targets' 60-second smoke in `win-asan` (`Sanitizers -Preset win-asan`) and their full-duration runs in stage acceptance. The programmer-error side is `Test_Compression_NullSpanWithCount_Asserts` (`z2-death-assert`) and `Test_CompressionContext_GetErrorWhenNotFailed_Checks` (`z2-death-check`), `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Untrusted input rules"; Zenith2/Docs/Design/COMPRESS.md → "Programmer errors"

### Z2-R-COMPRESS-090 Fuzz targets

- **Statement:** The six libFuzzer targets of the design exist under their libraries' `Tests/Fuzz/`, each creating its codec per input and asserting the design's properties, run for 60 seconds in `win-asan`.
- **Covers:** Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_CompressedSectionDesc`, `Zenith_Fuzz_ZlibDecode`, `Zenith_Fuzz_ZlibRoundTrip`, `Zenith_Fuzz_ZstdDecode`, `Zenith_Fuzz_ZstdRoundTrip`, `Zenith_Fuzz_ZstdDictionaryLoad` smoke runs in `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Fuzz targets"

### Z2-R-COMPRESS-091 Fuzz full-duration evidence

- **Statement:** Each COMPRESS fuzz target runs for its full stage-acceptance duration without a finding, as part of the security hardening evidence.
- **Covers:** Z2-I-COMPRESS-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Run-Committed.ps1 -Acceptance -Stage 6` fuzz results, collected by the Stage 6 acceptance ticket.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Fuzz targets"; Zenith2/Docs/Design/COMPRESS.md → "Stage table"

### Z2-R-COMPRESS-092 Failure logging

- **Statement:** Each failed public COMPRESS call records exactly one `ZENITH_LOG_ERROR` record, in category `compression.api`, `compression.zlib` or `compression.zstd`, emitted by the code that detects the failure (the backend for a library result, the API library for its own checks, never both), naming the codec, the operation, the enumerator and the library message or rule; no success path logs.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Compression_FailureLog_OneLinePerFailure` with CORE's `Zenith_CapturingHooks` recording log records, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Log category and messages"

### Z2-R-COMPRESS-093 Compression counters

- **Statement:** COMPRESS emits the seven counters per codec of the design through the installed profiler hooks at the stated points; every value is a delta and every name ends in `_delta` (`contexts_live_delta` ±1, `context_memory_bytes_delta` the change since `m_ulLastReportedMemoryBytes`), so a sink can tell them from the cumulative counters other producers publish on the same hook.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Compression_Counters_EmittedPerCall` with CORE's `Zenith_CapturingHooks`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Counters"

### Z2-R-COMPRESS-094 Compression zones

- **Statement:** Every public one-shot call, every context call and every dictionary operation opens exactly one profiler zone with the design's name.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Compression_Zones_OnePerCall` with CORE's `Zenith_CapturingHooks`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Profiler zones"

### Z2-R-COMPRESS-095 Context memory reported

- **Statement:** `Zenith_CompressionContext::GetMemoryBytes` and `Zenith_CompressionDictionary::GetMemoryBytes` report the objects' live bytes so their owners' memory consumers can report them to the memory governor.
- **Covers:** Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_LibraryAllocations_AreAccounted` per backend, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Memory per context"; Zenith2/Docs/Design/COMPRESS.md → "Counters"

### Z2-R-COMPRESS-096 Conformance executable

- **Statement:** `ZenithCompression_Conformance` links the API library and both backends, runs every conformance case once per backend under the names `ZenithCompression/Conformance/<Behaviour>[Zlib]` and `[Zstd]`, and is registered with CTest in `win`, `win-asan` and `linux-tsan`.
- **Covers:** Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** The `Test` step's JUnit report `${ZENITH_OUT}/gates/tests/ZenithCompression_Conformance.xml` listing both backend variants of every case.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Conformance benchmark and fuzz targets"; Zenith2/Docs/Design/COMPRESS.md → "Conformance cases"

### Z2-R-COMPRESS-097 Conformance corpus

- **Statement:** The conformance corpus generator produces every class of the design's corpus table from `(class, seed, bytes)` with platform-independent integer arithmetic, so the same triple gives the same bytes on every host.
- **Covers:** Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_GoldenVectors_Match` passing on MSVC and clang hosts (it fails if a corpus byte differs).
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Conformance corpus"

### Z2-R-COMPRESS-098 Conformance case list

- **Statement:** The conformance suite contains every case of the design's conformance table with the assertions it states.
- **Covers:** Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** The `Test` step's JUnit report of `ZenithCompression_Conformance` containing every listed case name for both backends; review of the conformance ticket.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Conformance cases"

### Z2-R-COMPRESS-099 Conformance under sanitizers

- **Statement:** The conformance suite and the three unit-test executables pass under `win-asan` and `linux-tsan` and on the S24 HWASan build.
- **Covers:** Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` (change-triggered and in stage acceptance); the Stage 2 acceptance's HWASan device run.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Sanitizer coverage"; Zenith2/Docs/Design/COMPRESS.md → "Test targets and gate steps"

### Z2-R-COMPRESS-100 Benchmark harness

- **Statement:** `Zenith_Bench_Compression` in `Zenith2/Benchmarks/Compression/` measures, with nanobench, the design's metrics for every codec, level, corpus class and mode, including the dictionary-gain rows, and is built for `win` and `android-arm64`.
- **Covers:** Z2-I-COMPRESS-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Bench_Compression --smoke` passing as a CTest test in the `Test` step; the benchmark ticket's report fixture.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Benchmarks"; Zenith2/Docs/Design/COMPRESS.md → "Conformance benchmark and fuzz targets"

### Z2-R-COMPRESS-101 Benchmark smoke

- **Statement:** `Zenith_Bench_Compression --smoke` runs one repetition over 64 KiB of every class at each codec's minimum, default and maximum level, checks every round trip, and exits 0.
- **Covers:** Z2-I-COMPRESS-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** The CTest smoke registered by `zenith_add_benchmark`, `Test` step in `win-release`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Benchmarks"

### Z2-R-COMPRESS-102 Benchmark procedure and report

- **Statement:** A reported benchmark run uses a Release build under controlled conditions, one discarded warm-up, at least 10 repetitions, medians with the order-statistic 95% interval and the variance, and writes the `zenith2.bench.compression.v1` JSON report with every repetition's value.
- **Covers:** Z2-I-COMPRESS-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** The reports committed under `Zenith2/Docs/Research/Compression/Stage2/<platform>/` by the budget ruling ticket, on the reference desktop and the S24.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Benchmarks"

### Z2-R-COMPRESS-103 Benchmark on reference hardware

- **Statement:** The benchmark is run on the reference desktop and on the S24 and its reports and summary are committed before the COMPRESS throughput rows are allocated.
- **Covers:** Z2-I-COMPRESS-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-012
- **Budgets:** Z2-B-COMPRESS-001, Z2-B-COMPRESS-002
- **Acceptance evidence:** The `needs-human` ruling ticket's diff: `Zenith2/Docs/Research/Compression/Stage2/{win,s24}/…json`, `report.md`, and `Zenith2/Docs/Research/Budgets/Stage1/COMPRESS/allocation.json`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Rows pending allocation"; Zenith2/Docs/Design/COMPRESS.md → "Ticket slices and ordering"

### Z2-R-COMPRESS-104 Throughput floors allocated

- **Statement:** Z2-B-COMPRESS-001 (zstd decompression throughput floor) and Z2-B-COMPRESS-002 (zlib decompression throughput floor) are allocated from the benchmark report under MP-DECOMP by the Stage 1 per-system budget ticket for COMPRESS, whose harness half is the ticket delivering `Zenith_Bench_Compression`.
- **Covers:** Z2-I-COMPRESS-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-COMPRESS-001, Z2-B-COMPRESS-002
- **Acceptance evidence:** The two rows `adopted` in `Zenith2/Docs/Budgets.md` and `Zenith2/Docs/Research/Budgets/Stage1/COMPRESS/allocation.json`, checked by the coverage gate step in acceptance mode `-Stage 1`.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Rows pending allocation"; Zenith2/Docs/Design/COMPRESS.md → "Rows COMPRESS is measured against"

### Z2-R-COMPRESS-105 Decode cost counted in the asset and boot rows

- **Statement:** Decode scratch arenas and decompression contexts are counted in Z2-B-ASSET-001, decode throughput sets the drain rate of the decode queue bounded by Z2-B-ASSET-002, and the decompression of boot content is part of the measured cold boot of Z2-B-SHIP-013 and Z2-B-SHIP-014, attributed through the `Compression/Decompress/*` zones.
- **Covers:** Z2-I-COMPRESS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-013, Z2-B-SHIP-014, Z2-B-ASSET-001, Z2-B-ASSET-002
- **Acceptance evidence:** The Stage 2 mid-sprint integration check and the Stage 2 acceptance W-Load measurement, where decompression is attributed through the `Compression/Decompress/*` zones.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Rows COMPRESS is measured against"

### Z2-R-COMPRESS-106 Benchmark ratios supplied to package measurement

- **Statement:** The committed benchmark report gives the compression ratio of every codec and level on every corpus class and the dictionary gains, which PACK's Stage 6 package measurement cites for the codecs and levels it chose; meeting the package rows themselves is PACK's requirement.
- **Covers:** Z2-I-COMPRESS-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-027
- **Budgets:** Z2-B-PACK-001, Z2-B-PACK-002, Z2-B-PACK-003, Z2-B-PACK-004
- **Acceptance evidence:** The benchmark reports under `Zenith2/Docs/Research/Compression/Stage2/`, cited by PACK's Stage 6 package-measurement (MP-PKG) ticket.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Rows COMPRESS is measured against"

### Z2-R-COMPRESS-107 API-only slice lands first

- **Statement:** The public headers of `ZenithCompression` with their contract blocks, the concrete context, descriptor and codec-set implementations, and the fake-codec tests land in one API ticket before either backend ticket.
- **Covers:** Z2-I-COMPRESS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** The API ticket passing `Build`, `Test`, `HeaderSelfContained` and `ApiContractCheck` with no backend present; the backend tickets `blocks`-linked from it.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Ticket slices and ordering"; Zenith2/Docs/Design/COMPRESS.md → "Stage table"

### Z2-R-COMPRESS-108 Pulled forward into Stage 1

- **Statement:** Every Stage 2 COMPRESS ticket is scheduled in the Stage 1 sprint while keeping fix version `Z2 Stage 2`, because no open decision governs COMPRESS.
- **Covers:** Z2-I-COMPRESS-01, Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** The filed board: every COMPRESS Stage 2 ticket in sprint `Stage 1 — Architecture validation` with fix version `Z2 Stage 2`, checked by the session coverage checker's routing rules.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Stage table"; Zenith2/Docs/Design/COMPRESS.md → "Open decisions"

### Z2-R-COMPRESS-109 Licence inventory entries

- **Statement:** The SHIP licence inventory lists zlib (`Zlib`) and zstd (`BSD-3-Clause`) with their licence files verified against the manifest's licence hashes.
- **Covers:** Z2-I-COMPRESS-02, Z2-I-COMPRESS-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Setup-Zenith2.ps1 -Check` licence-hash verification and the Stage 6 packaged build's licence inventory, collected by the Stage 6 acceptance ticket.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Third-party integration of zlib"; Zenith2/Docs/Design/COMPRESS.md → "Third-party integration of zstd"; Zenith2/Docs/Design/COMPRESS.md → "Stage table"

### Z2-R-COMPRESS-110 zlib engine step semantics

- **Statement:** Each zlib engine step makes exactly one `deflate` or `inflate` call with at most 2^30 bytes of output space, maps `Z_BUF_ERROR` in a step to zero progress, and reports `m_bDirectiveComplete` as the design defines it: `CONTINUE` ⇔ `avail_in == 0`, `FLUSH` ⇔ `avail_out != 0` after `Z_SYNC_FLUSH`, `END` and the decoder's frame end ⇔ `Z_STREAM_END`; the streams allocate only through the zlib thunks bound to the context's or call's allocator.
- **Covers:** Z2-I-COMPRESS-02, Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Flush_ProducesDecodablePrefix[Zlib]`, `…_StreamingOutputChunking_DoesNotChangeBytes[Zlib]`, `…_TrailingData_IsRejected[Zlib]`, `Test_CompressionZlib_Allocations_RouteThroughAllocator`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Zlib engine step"; Zenith2/Docs/Design/COMPRESS.md → "Library call enumerations and allocation thunks"

### Z2-R-COMPRESS-111 zstd engine step semantics

- **Statement:** Each zstd engine step makes exactly one `ZSTD_compressStream2` or `ZSTD_decompressStream` call and reports `m_bDirectiveComplete` as the design defines it (`CONTINUE` ⇔ all input consumed, `FLUSH`/`END` and the decoder's frame end ⇔ a return value of 0); the decoder's 18-byte header probe refuses skippable frames, windows above 2^23 bytes and dictionary disagreement before zstd decodes; contexts allocate only through `ZSTD_customMem` bound to the allocator.
- **Covers:** Z2-I-COMPRESS-03, Z2-I-COMPRESS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CompressionConformance_Dictionary_WrongOrMissing_IsMismatch[Zstd]` (streaming path), `Test_CompressionZstd_WindowTooLarge_IsLimitExceeded` (one-shot and streaming), `Test_CompressionZstd_SkippableFrame_IsCorrupt` (one-shot and streaming), `Test_CompressionConformance_Flush_ProducesDecodablePrefix[Zstd]`, `Test_CompressionZstd_Allocations_RouteThroughAllocator`, `Test` step.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Zstd engine step"; Zenith2/Docs/Design/COMPRESS.md → "Library call enumerations and allocation thunks"

### Z2-R-COMPRESS-112 Conformance cases fit their time caps

- **Statement:** Every default-suite conformance case completes within the 60-second CTest cap in `win-debug` and runs its sweep set at the minimum, default and maximum levels only, with two exceptions: `CompressBound_Holds` runs its nine sizes of at most 131,073 bytes at every level, and `GoldenVectors_Match` runs every fixture row at its recorded level; every other every-level sweep uses the 64 KiB sweep set and carries the `z2-stress` label (600 seconds); no case feeds more than 8 MiB of input per level at levels ≥ 15; under `linux-tsan` exactly `RoundTrip_CorpusThreeLevels`, the 1 MiB and 3 MiB legs of `CompressBound_Holds`, `OneShotEqualsStreamingWithPledgedSize`, `Deterministic_RepeatedRunsAndInstances`, `ScratchBoundIsSufficient` and `ContextMemoryBoundHolds` drop their maximum-level leg through CORE's single-preset idiom, while `GoldenVectors_Match` runs in full.
- **Covers:** Z2-I-COMPRESS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** The `Test` step's JUnit report `${ZENITH_OUT}/gates/tests/ZenithCompression_Conformance.xml` in `win-debug` (per-case durations under 60 s, and under 600 s for `z2-stress` cases); review of the sizes against the design's time-budget table.
- **Design:** Zenith2/Docs/Design/COMPRESS.md → "Conformance time budget"; Zenith2/Docs/Design/COMPRESS.md → "Conformance cases"

## Coverage of inventory items

| Item | Text | Requirements |
|---|---|---|
| Z2-I-COMPRESS-01 | the neutral `Zenith_Compression` API | Z2-R-COMPRESS-001, Z2-R-COMPRESS-002, Z2-R-COMPRESS-003, Z2-R-COMPRESS-004, Z2-R-COMPRESS-005, Z2-R-COMPRESS-006, Z2-R-COMPRESS-007, Z2-R-COMPRESS-009, Z2-R-COMPRESS-010, Z2-R-COMPRESS-011, Z2-R-COMPRESS-012, Z2-R-COMPRESS-013, Z2-R-COMPRESS-022, Z2-R-COMPRESS-079, Z2-R-COMPRESS-081, Z2-R-COMPRESS-082, Z2-R-COMPRESS-092, Z2-R-COMPRESS-093, Z2-R-COMPRESS-094, Z2-R-COMPRESS-107, Z2-R-COMPRESS-108 |
| Z2-I-COMPRESS-02 | zlib backend | Z2-R-COMPRESS-002, Z2-R-COMPRESS-004, Z2-R-COMPRESS-007, Z2-R-COMPRESS-008, Z2-R-COMPRESS-010, Z2-R-COMPRESS-012, Z2-R-COMPRESS-014, Z2-R-COMPRESS-016, Z2-R-COMPRESS-017, Z2-R-COMPRESS-019, Z2-R-COMPRESS-020, Z2-R-COMPRESS-021, Z2-R-COMPRESS-023, Z2-R-COMPRESS-026, Z2-R-COMPRESS-028, Z2-R-COMPRESS-059, Z2-R-COMPRESS-073, Z2-R-COMPRESS-108, Z2-R-COMPRESS-109, Z2-R-COMPRESS-110 |
| Z2-I-COMPRESS-03 | zstd backend | Z2-R-COMPRESS-002, Z2-R-COMPRESS-004, Z2-R-COMPRESS-007, Z2-R-COMPRESS-008, Z2-R-COMPRESS-010, Z2-R-COMPRESS-012, Z2-R-COMPRESS-015, Z2-R-COMPRESS-018, Z2-R-COMPRESS-019, Z2-R-COMPRESS-020, Z2-R-COMPRESS-021, Z2-R-COMPRESS-024, Z2-R-COMPRESS-025, Z2-R-COMPRESS-027, Z2-R-COMPRESS-035, Z2-R-COMPRESS-060, Z2-R-COMPRESS-072, Z2-R-COMPRESS-074, Z2-R-COMPRESS-075, Z2-R-COMPRESS-076, Z2-R-COMPRESS-077, Z2-R-COMPRESS-078, Z2-R-COMPRESS-108, Z2-R-COMPRESS-109, Z2-R-COMPRESS-111 |
| Z2-I-COMPRESS-04 | streaming mode | Z2-R-COMPRESS-028, Z2-R-COMPRESS-029, Z2-R-COMPRESS-030, Z2-R-COMPRESS-031, Z2-R-COMPRESS-032, Z2-R-COMPRESS-033, Z2-R-COMPRESS-034, Z2-R-COMPRESS-036, Z2-R-COMPRESS-037, Z2-R-COMPRESS-038, Z2-R-COMPRESS-039, Z2-R-COMPRESS-040, Z2-R-COMPRESS-041, Z2-R-COMPRESS-042, Z2-R-COMPRESS-043, Z2-R-COMPRESS-044, Z2-R-COMPRESS-045, Z2-R-COMPRESS-052, Z2-R-COMPRESS-080, Z2-R-COMPRESS-086, Z2-R-COMPRESS-087, Z2-R-COMPRESS-088, Z2-R-COMPRESS-095, Z2-R-COMPRESS-110, Z2-R-COMPRESS-111 |
| Z2-I-COMPRESS-05 | one-shot mode | Z2-R-COMPRESS-046, Z2-R-COMPRESS-047, Z2-R-COMPRESS-048, Z2-R-COMPRESS-049, Z2-R-COMPRESS-050, Z2-R-COMPRESS-051, Z2-R-COMPRESS-052, Z2-R-COMPRESS-053, Z2-R-COMPRESS-054, Z2-R-COMPRESS-055, Z2-R-COMPRESS-074, Z2-R-COMPRESS-083, Z2-R-COMPRESS-085 |
| Z2-I-COMPRESS-06 | deterministic output (per codec) | Z2-R-COMPRESS-016, Z2-R-COMPRESS-017, Z2-R-COMPRESS-018, Z2-R-COMPRESS-033, Z2-R-COMPRESS-034, Z2-R-COMPRESS-035, Z2-R-COMPRESS-051, Z2-R-COMPRESS-056, Z2-R-COMPRESS-057, Z2-R-COMPRESS-058, Z2-R-COMPRESS-059, Z2-R-COMPRESS-060, Z2-R-COMPRESS-061, Z2-R-COMPRESS-062, Z2-R-COMPRESS-063, Z2-R-COMPRESS-064, Z2-R-COMPRESS-071, Z2-R-COMPRESS-077 |
| Z2-I-COMPRESS-07 | codec IDs in cooked headers | Z2-R-COMPRESS-009, Z2-R-COMPRESS-054, Z2-R-COMPRESS-055, Z2-R-COMPRESS-063, Z2-R-COMPRESS-065, Z2-R-COMPRESS-066, Z2-R-COMPRESS-067, Z2-R-COMPRESS-068, Z2-R-COMPRESS-069, Z2-R-COMPRESS-070, Z2-R-COMPRESS-071 |
| Z2-I-COMPRESS-08 | a compression conformance suite | Z2-R-COMPRESS-079, Z2-R-COMPRESS-080, Z2-R-COMPRESS-083, Z2-R-COMPRESS-084, Z2-R-COMPRESS-085, Z2-R-COMPRESS-086, Z2-R-COMPRESS-087, Z2-R-COMPRESS-088, Z2-R-COMPRESS-089, Z2-R-COMPRESS-090, Z2-R-COMPRESS-091, Z2-R-COMPRESS-096, Z2-R-COMPRESS-097, Z2-R-COMPRESS-098, Z2-R-COMPRESS-099, Z2-R-COMPRESS-112 |
| Z2-I-COMPRESS-09 | compression benchmarks | Z2-R-COMPRESS-100, Z2-R-COMPRESS-101, Z2-R-COMPRESS-102, Z2-R-COMPRESS-103, Z2-R-COMPRESS-104, Z2-R-COMPRESS-105, Z2-R-COMPRESS-106 |

## Excluded and deferred

Every COMPRESS inventory item is classified required, and no COMPRESS requirement is deferred, excluded, game-side or research. The boundaries that keep the area small are exclusions of *other* capabilities from COMPRESS, not exclusions of COMPRESS items; each is owned elsewhere or rejected by a settled decision:

| Not in COMPRESS | Rationale | Decision or owner |
|---|---|---|
| a third codec (LZ4, Brotli, Oodle) | ratio below zstd on cooked content, or not buildable from pinned source for the sanitizer presets | Z2-D-004 (alternatives considered) |
| zstd negative levels, levels 20–22, zlib level 0 | slower or larger than an offered level for every consumer, or break the 8 MiB decode window cap | `Zenith2/Docs/Design/COMPRESS.md → "Codec metadata table"` (a design ruling under Z2-D-004) |
| library-internal multithreading | parallelism is the caller's jobs; keeps output scheduling-independent | Z2-D-004, `Zenith2/Docs/Design/COMPRESS.md → "Determinism definition and parameter pinning"` |
| archive layout, chunking, dictionary policy per asset class | packaging | PACK (Z2-I-PACK-03) |
| decode queues and back-pressure | asset loading | ASSET (Z2-I-ASSET-16, Z2-I-ASSET-17) |
| audio and video codecs | different selections | D-SEL-CODEC (open, Stage 2), D-SEL-VIDEO (open, Stage 2) |
| a Luau compression API | scripts never compress | Z2-D-016 |
| network message compression | networked multiplayer is Stage 7 | Z2-D-073 |
