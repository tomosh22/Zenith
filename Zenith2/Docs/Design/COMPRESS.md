# COMPRESS design: the neutral compression API and its zlib and zstd backends

This is the design document of the COMPRESS area (`Z2 COMPRESS — Compression`). Workers implement `ZenithCompression`, `ZenithCompression_Zlib` and `ZenithCompression_Zstd`, their conformance suite, fuzz targets and benchmark harness against it. It elaborates `Zenith2/Docs/Architecture.md → "Library dependency graph"`, `Zenith2/Docs/Architecture.md → "Third-party dependencies"`, `Zenith2/Docs/Architecture.md → "Engine modules and game projects"` (table "Backend selection") and `Zenith2/Docs/Contracts.md → "API boundary"`, and never contradicts them; where it needs something another area has not yet specified it says so in an `<!-- assumption: pending <AREA> -->` block. The document family is this entry file and `Zenith2/Docs/Design/COMPRESS/PublicAPI.md` (every public declaration and contract block; its headings are listed under "Declarations in PublicAPI.md"). The requirement catalogue is `Zenith2/Docs/Requirements/COMPRESS.md`; every obligation below traces to a `Z2-R-COMPRESS-NNN` listed in "Requirement index".

<!-- informative -->
The reference engine at `C:/dev/Zenith/Zenith/**` has no compression layer: it loads uncompressed asset files and links neither zlib nor zstd, so there is no prior art to carry over. The shape here follows the Zenith2 backend pattern (one API library, one library per backend, a factory per backend, a shared conformance suite), which COMPRESS proves on its smallest instance (`Zenith2/Docs/DecisionLog.md → "Z2-D-004 One neutral Zenith_Compression API with zlib and zstd backends"`, its "Context" bullet).
<!-- /informative -->

## Purpose and scope

### What COMPRESS owns

COMPRESS owns lossless general-purpose byte compression for every Zenith2 library, tool and game:

| Capability | Where it lives |
|---|---|
| the neutral interface `Zenith_Compression`, its metadata, one-shot compress and decompress, frame inspection, dictionaries | `Zenith2/Engine/ZenithCompression/` |
| the streaming context `Zenith_CompressionContext` (compress and decompress, bounded output, bounded work per call, cancellation by destroy or reset) | `Zenith2/Engine/ZenithCompression/` |
| the backend-facing engine interface `Zenith_CompressionStreamEngine` and allocator adapter `Zenith_CompressionAllocator` | `Zenith2/Engine/ZenithCompression/` |
| the codec-ID registry `Zenith_CompressionCodecID` and the 24-byte on-disk compressed section descriptor that every cooked format embeds | `Zenith2/Engine/ZenithCompression/` |
| the codec set `Zenith_CompressionCodecSet` through which readers select a codec from the data | `Zenith2/Engine/ZenithCompression/` |
| the zlib backend (RFC 1950 streams, zlib 1.3.1) exporting `Zenith_CreateCompression_Zlib` | `Zenith2/Engine/ZenithCompression_Zlib/` |
| the zstd backend (RFC 8878 frames and dictionaries, zstd 1.5.7) exporting `Zenith_CreateCompression_Zstd` | `Zenith2/Engine/ZenithCompression_Zstd/` |
| deterministic output per codec, its definition, its parameter pins and the golden-vector test | all three libraries |
| the shared conformance suite `ZenithCompression_Conformance`, the fuzz targets, and the benchmark harness `Zenith_Bench_Compression` | `Tests/Conformance/`, `Tests/ConformanceAdapter/`, `Tests/Fuzz/` of the three libraries; `Zenith2/Benchmarks/Compression/` |
| the zlib and zstd entries of `Zenith2/ThirdParty/manifest.json` and the zlib wrapper `Zenith2/ThirdParty/zlib/CMakeLists.txt` | added by the COMPRESS backend tickets (the directory itself is owned by SHIP, `Zenith2/Docs/Architecture.md → "Repository layout"`) |

### Exclusions

| Not COMPRESS | Owner | Why |
|---|---|---|
| archive layout, table of contents, chunking into archive blocks, patch and DLC manifests, which asset class uses which codec, level and dictionary | PACK | COMPRESS compresses byte ranges; containers are packaging (Z2-I-PACK-03 is PACK's) |
| cooked asset headers and loaders (they embed COMPRESS's section descriptor) | ASSET | Z2-D-038 cooked formats; ASSET owns each asset type's format |
| decode queues, back-pressure, I/O throttling (Z2-I-ASSET-16, Z2-I-ASSET-17) | ASSET | COMPRESS is synchronous and holds no queue |
| texture block compression (BC7, ASTC), mesh quantisation, animation compression | GEOM, ANIM, ASSET cook (KTX-Software, `ZenithVertexFormat`) | lossy, domain-specific codecs, not byte compression |
| audio and video codecs (D-SEL-CODEC (open, Stage 2), D-SEL-VIDEO (open, Stage 2)) | AUDIO, CINE | different problem; different selections |
| save-file layout and checksums | SAVE | SAVE compresses its sections through this API |
| network message compression | NET (Stage 7) | no Stage 1–6 consumer (Z2-D-073) |
| any Luau-facing compression API | none | scripts never compress (see "Luau surface") |
| LZ4, Brotli, Oodle or any third codec | none | `Zenith2/Docs/DecisionLog.md → "Z2-D-004 One neutral Zenith_Compression API with zlib and zstd backends"`, its "Alternatives considered" bullet; a new codec needs a decision and its own backend library |
| multi-threaded compression inside a library (zstd `nbWorkers`) | none | parallelism is the caller's jobs; see "Determinism definition and parameter pinning" |

### Inventory items covered

| Item | Text (Inventory v1) | Covered by |
|---|---|---|
| Z2-I-COMPRESS-01 | the neutral `Zenith_Compression` API | "Public API", "Library and file layout" |
| Z2-I-COMPRESS-02 | zlib backend | "ZenithCompression_Zlib files", "Third-party integration of zlib", "One-shot decompression with zlib", "Error mapping for zlib" |
| Z2-I-COMPRESS-03 | zstd backend | "ZenithCompression_Zstd files", "Third-party integration of zstd", "One-shot decompression with zstd", "Dictionary creation and training", "Error mapping for zstd" |
| Z2-I-COMPRESS-04 | streaming mode | PublicAPI.md "Zenith_CompressionContext.h", "Streaming compression context state machine", "Streaming decompression context state machine" |
| Z2-I-COMPRESS-05 | one-shot mode | PublicAPI.md "Zenith_Compression.h", "One-shot compression", "Section compression and decompression" |
| Z2-I-COMPRESS-06 | deterministic output (per codec) | "Canonical chunking", "Determinism definition and parameter pinning", "Golden vectors" |
| Z2-I-COMPRESS-07 | codec IDs in cooked headers | "Codec ID registry", "Compressed section descriptor on disk", "How cooked formats embed the descriptor" |
| Z2-I-COMPRESS-08 | a compression conformance suite | "Conformance corpus", "Conformance cases" |
| Z2-I-COMPRESS-09 | compression benchmarks | "Benchmarks", "Rows pending allocation" |

### Design choices at a glance

<!-- elaboration -->
| # | Choice | Section |
|---|---|---|
| C1 | Compression output is made independent of how a caller slices its input and output **by construction**: every compress path (streaming and one-shot) feeds the library 128 KiB input chunks at fixed offsets and drains it through a fixed 128 KiB output staging buffer | "Canonical chunking" |
| C2 | One-shot compression is implemented once, in the API library, on the same engine and driver as streaming, so one-shot bytes equal streaming bytes with a pledged size, by construction | "One-shot compression" |
| C3 | Public entry points are non-virtual members of `Zenith_Compression` (validation, diagnostics, arena restore implemented once); backends implement private `Do*` hooks | PublicAPI.md "Zenith_Compression.h" |
| C4 | Level ranges: zlib 1–9 (default 6), zstd 1–19 (default 3); 0 means "the codec default" and is never stored | "Codec metadata table" |
| C5 | Integrity is always on: zlib streams carry Adler-32; every zstd frame Zenith2 writes carries its content checksum and (when known) its content size | "zstd frame format as written" |
| C6 | zstd dictionaries are supported (create, compress, decompress, deterministic training); zlib has none | "Dictionary creation and training" |
| C7 | Decoders refuse zstd windows above 8 MiB (`windowLog` 23), which bounds decode memory on the S24 and is why zstd levels stop at 19 | "zstd frame format as written" |
| C8 | Codec IDs are one byte: 0 stored, 1 zlib, 2 zstd, 3–127 reserved, 128–254 experimental (never cooked), 255 invalid | "Codec ID registry" |
| C9 | Every compressed byte range in a cooked file is described by one 24-byte little-endian `Zenith_CompressedSectionDesc` record | "Compressed section descriptor on disk" |
| C10 | zlib is integrated as a `zenith-wrapper` build with `Z_SOLO`; zstd as a `cmake-subproject` from `build/cmake` | "Third-party integration of zlib", "Third-party integration of zstd" |

## Dependencies

### Libraries and documents relied on

| Dependency | What COMPRESS uses | Cited heading |
|---|---|---|
| ZenithCore | `Zenith_Result`, `Zenith_Err`, `ZENITH_TRY`, `Zenith_Owned`, `Zenith_TryNew`, `Zenith_Heap` (`TryAllocate`, `Free`, `GetUsableSize`), `Zenith_GetRootHeap`, `Zenith_Arena` (`TryAllocate`, `GetMarker`, `ResetToMarker`), `Zenith_TryConstruct`, `Zenith_Destroy`, `Zenith_Span<T>`, `Zenith_FaultInjector`, `Zenith_NarrowCast`, `Zenith_TryNarrowCast`, `ZENITH_ASSERT`, `ZENITH_CHECK`, `Zenith_GetProfilerHooks`, `Zenith_BuildInfo.h` | `Zenith2/Docs/Contracts.md → "Ownership and allocation"`, `Zenith2/Docs/Contracts/Memory.md → "Zenith_Heap"`, `Zenith2/Docs/Contracts/Memory.md → "Zenith_Arena"`, `Zenith2/Docs/Contracts/Memory.md → "Fallible construction"`, `Zenith2/Docs/Contracts.md → "Errors and results"`, `Zenith2/Docs/Contracts.md → "No mutable global state"` |
| zlib 1.3.1 | `deflateInit2`, `deflate`, `deflateReset`, `deflateEnd`, `inflateInit2`, `inflate`, `inflateReset`, `inflateEnd`, `zlibVersion` | `Zenith2/Docs/Architecture.md → "Third-party dependencies"` |
| zstd 1.5.7 | the `ZSTD_CCtx`/`ZSTD_DCtx` advanced API with `ZSTD_customMem`, `ZSTD_compressStream2`, `ZSTD_decompressStream`, `ZSTD_decompressDCtx`, `ZSTD_getFrameHeader`, `ZSTD_findFrameCompressedSize`, CDict/DDict, `ZDICT_optimizeTrainFromBuffer_fastCover` | `Zenith2/Docs/Architecture.md → "Third-party dependencies"` |
| the build helpers | `zenith_add_library(… KIND BACKEND …)`, `THIRD_PARTY`, `TEST_DEPENDS`, `zenith_add_benchmark`, the glob partition of `Tests/Conformance/`, `Tests/ConformanceAdapter/`, `Tests/Fuzz/` | `Zenith2/Docs/Architecture/BuildSystem.md → "The zenith_add_library helper"`, `Zenith2/Docs/Architecture/BuildSystem.md → "Tools, benchmarks and integration suites"`, `Zenith2/Docs/Architecture/BuildSystem.md → "Third-party integration"` |
| doctest, nanobench | test and benchmark frameworks (test and benchmark targets only) | `Zenith2/Docs/Architecture.md → "Third-party dependencies"` |

COMPRESS depends on no other Zenith2 library: the declared graph gives `ZenithCompression → ZenithCore` and each backend `→ ZenithCompression, ZenithCore` (`Zenith2/Docs/Architecture.md → "Library dependency graph"`). In particular it depends on neither `ZenithProfiling` nor `ZenithJobs`: profiling goes through the ZenithCore profiler hooks, and parallelism is the caller's.

### Assumed CORE types

COMPRESS uses these ZenithCore names exactly as the CORE design declares them:

| Name | Declared in |
|---|---|
| `Zenith_Span<T>`, `Zenith_ByteSpan` (`Zenith_Span<const uint8>`), `Zenith_MutableByteSpan` (`Zenith_Span<uint8>`) | `Zenith2/Docs/Design/CORE/Containers.md → "Zenith_Span"` |
| `Zenith_StringView` | `Zenith2/Docs/Design/CORE/Text.md → "Zenith_StringView"` |
| `Zenith_SanitizerKind` (`SANITIZER_KIND_NONE = 1`, `SANITIZER_KIND_ASAN`, `SANITIZER_KIND_TSAN_UBSAN`, `SANITIZER_KIND_HWASAN`), `Zenith_GetSanitizerKind()`, `Zenith_IsSanitizerBuild()`, `Zenith_IsDebugBuild()`, and the single-preset test idiom (`if constexpr (Zenith_GetSanitizerKind() != SANITIZER_KIND_ASAN)` plus doctest's `MESSAGE`) | `Zenith2/Docs/Design/CORE/Diagnostics.md → "Build information"` |
| `Zenith_LogCategory`, `ZENITH_LOG_CATEGORY`, `ZENITH_LOG_ERROR(Category, …)`, the levels | `Zenith2/Docs/Design/CORE/Diagnostics.md → "Levels and categories"`, `Zenith2/Docs/Design/CORE/Diagnostics.md → "Log macros"` |
| `Zenith_TestFaultInjector`, `Zenith_RunFaultSweep`, `Zenith_Heap::SetFaultCountdown` | `Zenith2/Docs/Design/CORE/Memory.md → "Fault-injection support"` |
| `Zenith_CapturingHooks` (the test hook table recording zones, counters, lock acquire and release with the wait, blocking waits with the wait, and log records), `Zenith_TestMain.cpp` | `Zenith2/Docs/Design/CORE.md → "ZenithTestSupport target"` |
| `Zenith_Result`, `Zenith_Err`, `ZENITH_TRY`, `Zenith_Owned`, `Zenith_TryNew`, `Zenith_Heap`, `Zenith_Arena`, `Zenith_ArenaMarker`, `Zenith_TryConstruct`, `Zenith_Destroy`, `Zenith_FaultInjector` | `Zenith2/Docs/Contracts/Memory.md → "Zenith_Heap"`, `Zenith2/Docs/Contracts/Memory.md → "Zenith_Arena"`, `Zenith2/Docs/Contracts/Memory.md → "Fallible construction"` |
| `Zenith_Semaphore`, `Zenith_Mutex`, `Zenith_Thread` | `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` |
| `Zenith_PersistentID` | `Zenith2/Docs/Contracts.md → "Handle kinds"` |

<!-- assumption: pending CORE -->
One name is not declared by the CORE design: a scoped profiler zone. COMPRESS assumes `Zenith_ScopedZone`, an RAII object in `ZenithCore/Zenith_ProfilerHooks.h` taking a `constexpr Zenith_ZoneSite&` and calling `Zenith_DispatchZoneBegin` and `Zenith_DispatchZoneEnd` ([Z2-N-CONTRACT-143]); if CORE adds it under another name, the first COMPRESS ticket uses that name.
<!-- /assumption -->

### Areas that consume COMPRESS

| Area | Uses | Through |
|---|---|---|
| MOD (ZenithRuntime, the generated composition TU) | creates both codecs at startup step 11 and fills a `Zenith_CompressionCodecSet` | `Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`; the factories named in `Zenith2/Docs/Architecture/CompositionRoot.md → "The generated composition table"` |
| ASSET (ZenithAssets, ZenithCook) | cooked-format section decode on its decode threads; section encode in the cook; the importer compression setting | `Zenith_DecompressSection`, `Zenith_CompressSection`, `Zenith_ValidateCompressionSetting` |
| PACK | archive entries and archive-level dictionaries; codec, level and dictionary policy per asset class | `Zenith_CompressedSectionDesc`, `Zenith_Compression::TrainDictionary`, the benchmark report |
| SAVE (ZenithSave) | compressed save and replay sections, streaming replay writing | `Zenith_CompressionContext`, section helpers |
| EDIT (ZenithEditor) | the importer-settings inspector's codec and level fields | `Zenith_ParseCompressionCodecName`, `Zenith_ValidateCompressionSetting`, `Zenith_CompressionCodecInfo` |
| SHIP | the licence inventory entries for zlib and zstd; dependency upgrades | `Zenith2/ThirdParty/manifest.json` |
| MAINT | dependency-upgrade rehearsals | "Re-cook and migration" |

The composition root uses COMPRESS's names: `Zenith_CodecEntry` carries a `Zenith_PfnCreateCompression` and the table lists `{ "zlib", &Zenith_CreateCompression_Zlib }` and `{ "zstd", &Zenith_CreateCompression_Zstd }` (`Zenith2/Docs/Architecture/CompositionRoot.md → "The generated composition table"`); startup step 11 creates both codecs into a `Zenith_CompressionCodecSet` before the asset registry (`Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`), and shutdown destroys them in the reverse order ([Z2-N-ARCH-841]). The names follow [Z2-N-CONTRACT-037] and [Z2-N-CONTRACT-423] (`Zenith_Create<Interface noun>_<Backend>`, no `I` prefix).

The consuming areas own their use; COMPRESS owns the behaviour of the calls. The cross-area items this document needs from them are listed in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` under "COMPRESS".

### Upward interfaces

COMPRESS sits at layer 1 and 2 and calls nothing above ZenithCore. It declares two interfaces that its backends (one layer up) implement: `Zenith_Compression` and `Zenith_CompressionStreamEngine`, plus the dictionary interface `Zenith_CompressionDictionary`. It declares no callback interface for higher libraries: no call into COMPRESS ever calls back into its caller (every interface's `Reentrancy` is `NOT_APPLICABLE`), which keeps the layering DAG trivially satisfied (`Zenith2/Docs/Contracts.md → "Layering DAG"`).

## Library and file layout

### Target summary

| Target | Kind | Declared dependencies | `THIRD_PARTY` | `SUPPORTED` | Factory |
|---|---|---|---|---|---|
| `ZenithCompression` | `ENGINE` (the API library) | `ZenithCore` | — | `ZENITH_ALL` | none |
| `ZenithCompression_Zlib` | `BACKEND`, `BACKEND_OF ZenithCompression`, `BACKEND_NAME Zlib` | `ZenithCompression`, `ZenithCore` | `zlib` | `ZENITH_ALL` | `Zenith_CreateCompression_Zlib` |
| `ZenithCompression_Zstd` | `BACKEND`, `BACKEND_OF ZenithCompression`, `BACKEND_NAME Zstd` | `ZenithCompression`, `ZenithCore` | `zstd` | `ZENITH_ALL` | `Zenith_CreateCompression_Zstd` |
| `ZenithCompression_Tests` | test | `ZenithCompression`, `TEST_DEPENDS ZenithCompression_Zlib ZenithCompression_Zstd`, `ZenithTestSupport`, doctest | — | built wherever `ZenithCompression` is; CTest in `ZENITH_HOST` | — |
| `ZenithCompression_Zlib_Tests`, `ZenithCompression_Zstd_Tests` | test | the backend, `ZenithTestSupport`, doctest | — | as above | — |
| `ZenithCompression_Conformance` | conformance | `ZenithCompression`, both backends, `ZenithTestSupport`, doctest | — | as above | — |
| `Zenith_Fuzz_<Stem>` | fuzz | the owning library, `ZenithTestSupport`, libFuzzer | — | `win-asan` only | — |
| `Zenith_Bench_Compression` | benchmark | `ZenithCompression`, both backends, `ZenithCore`, nanobench, `ZenithTestSupport` | — | `win`, `android-arm64` | — |

The three `CMakeLists.txt` files are exactly:

```cmake
# Zenith2/Engine/ZenithCompression/CMakeLists.txt
zenith_add_library(ZenithCompression DEPENDS ZenithCore
	TEST_DEPENDS ZenithCompression_Zlib ZenithCompression_Zstd)
```

```cmake
# Zenith2/Engine/ZenithCompression_Zlib/CMakeLists.txt
zenith_add_library(ZenithCompression_Zlib KIND BACKEND BACKEND_OF ZenithCompression BACKEND_NAME Zlib
	FACTORY Zenith_CreateCompression_Zlib
	DEPENDS ZenithCompression ZenithCore
	THIRD_PARTY zlib)
```

```cmake
# Zenith2/Engine/ZenithCompression_Zstd/CMakeLists.txt
zenith_add_library(ZenithCompression_Zstd KIND BACKEND BACKEND_OF ZenithCompression BACKEND_NAME Zstd
	FACTORY Zenith_CreateCompression_Zstd
	DEPENDS ZenithCompression ZenithCore
	THIRD_PARTY zstd)
```

Backend headers are included by `ZenithCompression_Tests` (through `TEST_DEPENDS`), `ZenithCompression_Conformance`, `Zenith_Bench_Compression`, the fuzz targets and the mains of ZenithCook and ZenithEditor. [Z2-N-CONTRACT-042] admits exactly these ("the composition root and … test, benchmark, fuzz and tool executables"), and `Libgraph` recognises them through the target-kind predicate of [Z2-N-CONTRACT-050]: each is an `EXECUTABLE` whose sources lie under a `Tests/` directory, under `Zenith2/Benchmarks/**` or under `Zenith2/Tools/**` (`Zenith2/Docs/Contracts.md → "API boundary"`).

<!-- informative -->
`TEST_DEPENDS` from the API library to its own backends is the same shape as `ZenithFlux … TEST_DEPENDS ZenithPathTracer` (`Zenith2/Docs/Architecture/BuildSystem.md → "Signature"`): the edge exists only on the test executable, so the declared library graph stays acyclic. It lets `ZenithCompression_Tests` run the compiled public-API example and the section helpers against real codecs.
<!-- /informative -->

### ZenithCompression files

| Path | Content |
|---|---|
| `Zenith2/Engine/ZenithCompression/CMakeLists.txt` | the call above |
| `Zenith2/Engine/ZenithCompression/Zenith_CompressionTypes.h` | codec IDs, the error enumeration and its `Zenith_ToString`, modes, states, constants, fault points, descs, `Zenith_CompressionCodecInfo`, `Zenith_CompressionFrameInfo`, `Zenith_CompressionProgress`, the setting functions |
| `Zenith2/Engine/ZenithCompression/Zenith_Compression.h` | the interface `Zenith_Compression`, `Zenith_PfnCreateCompression` |
| `Zenith2/Engine/ZenithCompression/Zenith_CompressionEngine.h` | the backend-facing interface `Zenith_CompressionStreamEngine`, `Zenith_CompressionEngineDesc`, `Zenith_CompressionEngineStep` |
| `Zenith2/Engine/ZenithCompression/Zenith_CompressionAllocator.h` | `Zenith_CompressionAllocator` |
| `Zenith2/Engine/ZenithCompression/Zenith_CompressionContext.h` | `Zenith_CompressionContext` |
| `Zenith2/Engine/ZenithCompression/Zenith_CompressionDictionary.h` | the interface `Zenith_CompressionDictionary` |
| `Zenith2/Engine/ZenithCompression/Zenith_CompressionCodecSet.h` | `Zenith_CompressionCodecSet`, `Zenith_CompressionCodecSetDesc` |
| `Zenith2/Engine/ZenithCompression/Zenith_CompressedSection.h` | `Zenith_CompressedSectionDesc`, its encode, decode and validate functions, `Zenith_CompressSection`, `Zenith_StoreSection`, `Zenith_DecompressSection` |
| `Internal/Zenith_CompressionTypes.cpp` | `Zenith_ParseCompressionCodecName`, `Zenith_ValidateCompressionSetting` |
| `Internal/Zenith_CompressionFacade.cpp` | the non-virtual members of `Zenith_Compression` except `Compress` |
| `Internal/Zenith_CompressionOneShot.cpp` | `Zenith_Compression::Compress` |
| `Internal/Zenith_CompressionDriver.h`, `Internal/Zenith_CompressionDriver.cpp` | `Zenith_CompressionDriver`, the canonical chunk driver shared by the context and one-shot compression |
| `Internal/Zenith_CompressionContext.cpp` | `Zenith_CompressionContext` |
| `Internal/Zenith_CompressionAllocator.cpp` | `Zenith_CompressionAllocator` |
| `Internal/Zenith_CompressionCodecSet.cpp` | `Zenith_CompressionCodecSet` |
| `Internal/Zenith_CompressedSection.cpp` | the descriptor codec and the section helpers |
| `Internal/Zenith_CompressionDiagnostics.h`, `Internal/Zenith_CompressionDiagnostics.cpp` | counter names, zone sites, the logging helpers of "Diagnostics and profiling" |
| `Tests/Zenith_CompressionTypes_Tests.cpp`, `Tests/Zenith_CompressionCodecSet_Tests.cpp`, `Tests/Zenith_CompressedSection_Tests.cpp`, `Tests/Zenith_CompressionContext_Tests.cpp`, `Tests/Zenith_CompressionAllocator_Tests.cpp`, `Tests/Zenith_Compression_Tests.cpp`, `Tests/Zenith_CompressionContext_DefectTests.cpp` | the unit tests of "ZenithCompression unit tests" |
| `Tests/Zenith_FakeCompression.h`, `Tests/Zenith_FakeCompression.cpp` | a test-only codec with codec ID 200 and a recording engine (see "ZenithCompression unit tests") |
| `Tests/Examples/Zenith_Compression_Example.cpp` | the compiled example the `Zenith_Compression` contract block cites |
| `Tests/Conformance/Zenith_CompressionConformance.h` | the conformance case macro, backend getters, the test hash, shared helpers |
| `Tests/Conformance/Zenith_CompressionConformanceCorpus.h`, `.cpp` | the seeded corpus generator |
| `Tests/Conformance/Zenith_CompressionConformance_RoundTrip.cpp`, `_Streaming.cpp`, `_Determinism.cpp`, `_Failure.cpp`, `_Memory.cpp`, `_Threads.cpp`, `_Dictionary.cpp`, `_Large.cpp` | the conformance cases |
| `Tests/Conformance/Fixtures/golden_v1.txt` | the golden vectors |
| `Tests/Fuzz/CompressedSectionDesc.cpp` | fuzz target `Zenith_Fuzz_CompressedSectionDesc` |

There is no `Internal/Windows|Android|Linux/` directory in any COMPRESS library: nothing in COMPRESS is platform-specific.

### ZenithCompression_Zlib files

| Path | Content |
|---|---|
| `Zenith2/Engine/ZenithCompression_Zlib/CMakeLists.txt` | the call above |
| `Zenith2/Engine/ZenithCompression_Zlib/Zenith_Compression_Zlib.h` | `Zenith_CreateCompression_Zlib` and nothing else |
| `Internal/Zenith_ZlibInclude.h` | the only file that includes `<zlib.h>`; `static_assert`s that `Z_SOLO` is defined and that `ZLIB_VERNUM == 0x1310` |
| `Internal/Zenith_CompressionZlib.h`, `.cpp` | `class Zenith_CompressionZlib final : public Zenith_Compression`, the factory definition |
| `Internal/Zenith_CompressionZlibEngine.h`, `.cpp` | `Zenith_CompressionZlibDeflateEngine`, `Zenith_CompressionZlibInflateEngine` |
| `Internal/Zenith_CompressionZlibDecode.cpp` | one-shot inflate and header parsing |
| `Internal/Zenith_CompressionZlibErrors.h`, `.cpp` | `Zenith_ZlibCall`, `Zenith_MapZlibError` and the zlib allocation thunks ("Library call enumerations and allocation thunks") |
| `Tests/Zenith_CompressionZlib_Tests.cpp` | the zlib unit tests |
| `Tests/ConformanceAdapter/Zenith_CompressionConformanceAdapter_Zlib.cpp` | `Zenith_GetCompressionConformanceBackend_Zlib`, declared in "Conformance cases" |
| `Tests/Fuzz/ZlibDecode.cpp`, `Tests/Fuzz/ZlibRoundTrip.cpp` | fuzz targets `Zenith_Fuzz_ZlibDecode`, `Zenith_Fuzz_ZlibRoundTrip` |

### ZenithCompression_Zstd files

| Path | Content |
|---|---|
| `Zenith2/Engine/ZenithCompression_Zstd/CMakeLists.txt` | the call above |
| `Zenith2/Engine/ZenithCompression_Zstd/Zenith_Compression_Zstd.h` | `Zenith_CreateCompression_Zstd` and nothing else |
| `Internal/Zenith_ZstdInclude.h` | the only file that includes `<zstd.h>`, `<zstd_errors.h>` and `<zdict.h>`, after defining `ZSTD_STATIC_LINKING_ONLY` and `ZDICT_STATIC_LINKING_ONLY`; `static_assert(ZSTD_VERSION_NUMBER == 10507)` |
| `Internal/Zenith_CompressionZstd.h`, `.cpp` | `class Zenith_CompressionZstd final : public Zenith_Compression`, the factory definition |
| `Internal/Zenith_CompressionZstdEngine.h`, `.cpp` | `Zenith_CompressionZstdCompressEngine`, `Zenith_CompressionZstdDecompressEngine` |
| `Internal/Zenith_CompressionZstdDecode.cpp` | one-shot decompression and frame inspection |
| `Internal/Zenith_CompressionZstdDictionary.h`, `.cpp` | `class Zenith_CompressionZstdDictionary final : public Zenith_CompressionDictionary` |
| `Internal/Zenith_CompressionZstdTraining.cpp` | dictionary training |
| `Internal/Zenith_CompressionZstdErrors.h`, `.cpp` | `Zenith_ZstdCall`, `Zenith_MapZstdError` and the `ZSTD_customMem` thunks ("Library call enumerations and allocation thunks") |
| `Tests/Zenith_CompressionZstd_Tests.cpp` | the zstd unit tests |
| `Tests/ConformanceAdapter/Zenith_CompressionConformanceAdapter_Zstd.cpp` | `Zenith_GetCompressionConformanceBackend_Zstd`, declared in "Conformance cases" |
| `Tests/Fuzz/ZstdDecode.cpp`, `Tests/Fuzz/ZstdRoundTrip.cpp`, `Tests/Fuzz/ZstdDictionaryLoad.cpp` | fuzz targets `Zenith_Fuzz_ZstdDecode`, `Zenith_Fuzz_ZstdRoundTrip`, `Zenith_Fuzz_ZstdDictionaryLoad` |

### Third-party integration of zlib

The manifest entry, added to `Zenith2/ThirdParty/manifest.json` by the zlib backend ticket in the same diff as the backend (`Zenith2/Docs/Architecture/BuildSystem.md → "The manifest dependencies section"`):

```json
{
	"name": "zlib",
	"version": "1.3.1",
	"source": { "url": "https://zlib.net/fossils/zlib-1.3.1.tar.gz", "sha256": "<candidate: 9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23>", "stripPrefix": "zlib-1.3.1" },
	"licence": "Zlib",
	"integration": "zenith-wrapper",
	"owner": "ZenithCompression_Zlib",
	"platforms": ["android", "linux", "windows"],
	"language": "c",
	"licenceFile": "LICENSE",
	"licenceSha256": "<candidate: computed from LICENSE inside the verified archive>",
	"patches": [],
	"usage": "runtime"
}
```

Both digests in this entry (and in the zstd entry below) are **candidate digests**, transcribed from memory and not yet verified. The Stage 0 reproducibility-manifest ticket (Z2-D-008) downloads each archive, verifies the candidate against it, corrects it if it differs, computes the `licenceSha256` from the verified archive, and pins the verified values; the entry lands in `Zenith2/ThirdParty/manifest.json` only with verified values, and `Setup-Zenith2.ps1 -Check` re-verifies them on every machine (`Zenith2/Docs/Architecture/BuildSystem.md → "Toolchain pins and the setup script"`). The obligation is the verification, never these literal strings.

zlib is integrated as `zenith-wrapper`, as `Zenith2/Docs/Architecture.md → "Third-party dependencies"` lists it: zlib 1.3.1's own `CMakeLists.txt` renames `zconf.h` to `zconf.h.included` inside the source directory on every out-of-source configure, which would modify the shared, hash-verified dependency cache of `Zenith2/Docs/Architecture/BuildSystem.md → "The dependency cache"`. The wrapper builds the unmodified upstream sources, so "integrated as released" (Z2-D-082) holds.

`Zenith2/ThirdParty/zlib/CMakeLists.txt` (the wrapper, one static library target named `zlib`):

| Setting | Value |
|---|---|
| sources | exactly `adler32.c crc32.c deflate.c inffast.c inflate.c inftrees.c trees.c zutil.c` from the cache directory; `gz*.c`, `compress.c`, `uncompr.c`, `infback.c` and `contrib/` are not compiled |
| include directory | the cache directory itself (upstream `zconf.h` used unmodified), exposed `SYSTEM` to the owner |
| compile definitions | `Z_SOLO` and `ZLIB_CONST`, both `PUBLIC`, so zlib and the owner's `Internal/` see the same declarations: `Z_SOLO` compiles no default allocator (`zcalloc`) and no gz file API, so zlib can only allocate through the thunks COMPRESS installs (and `deflateInit2`/`inflateInit2` return `Z_STREAM_ERROR` when no `zalloc` is set); `ZLIB_CONST` makes `next_in` a `const Bytef*`, so the engine passes `Zenith_ByteSpan` data without a cast |
| forbidden definitions | `ZLIB_DEBUG`, `UNALIGNED_OK`, `DYNAMIC_CRC_TABLE`, `Z_PREFIX` — none is defined |
| flags | the third-party rows of `Zenith2/Docs/Architecture/BuildSystem.md → "Compile and link flags"` (sanitizer, runtime library, debug information), nothing else |

### Third-party integration of zstd

```json
{
	"name": "zstd",
	"version": "1.5.7",
	"source": { "url": "https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz", "sha256": "<candidate: eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3>", "stripPrefix": "zstd-1.5.7" },
	"licence": "BSD-3-Clause",
	"integration": "cmake-subproject",
	"cmakeSourceDir": "build/cmake",
	"owner": "ZenithCompression_Zstd",
	"platforms": ["android", "linux", "windows"],
	"language": "c",
	"options": {
		"common": {
			"ZSTD_BUILD_PROGRAMS": "OFF", "ZSTD_BUILD_SHARED": "OFF", "ZSTD_BUILD_STATIC": "ON",
			"ZSTD_BUILD_TESTS": "OFF", "ZSTD_BUILD_CONTRIB": "OFF", "ZSTD_LEGACY_SUPPORT": "OFF",
			"ZSTD_MULTITHREAD_SUPPORT": "OFF", "ZSTD_BUILD_COMPRESSION": "ON", "ZSTD_BUILD_DECOMPRESSION": "ON",
			"ZSTD_BUILD_DICTBUILDER": "ON", "ZSTD_BUILD_DEPRECATED": "OFF", "ZSTD_USE_STATIC_RUNTIME": "OFF"
		}
	},
	"targets": [ { "target": "libzstd_static", "owner": "ZenithCompression_Zstd", "usage": "runtime" } ],
	"licenceFile": "LICENSE",
	"licenceSha256": "<candidate: computed from LICENSE inside the verified archive>",
	"patches": [],
	"usage": "runtime"
}
```

The two digests are candidates, verified and pinned by the Stage 0 manifest ticket exactly as for zlib above.

| Option | Why |
|---|---|
| `ZSTD_LEGACY_SUPPORT OFF` | pre-v0.8 frame formats are never written by Zenith2; decoding them only widens the attack surface of an untrusted-input parser (`Zenith2/Docs/Architecture.md → "Security architecture"`) |
| `ZSTD_MULTITHREAD_SUPPORT OFF` | no library-internal threads: parallelism is the caller's jobs, and single-threaded zstd output does not depend on a worker count |
| `ZSTD_BUILD_DICTBUILDER ON` | `ZDICT_*` training is exposed through `Zenith_Compression::TrainDictionary` |
| `ZSTD_USE_STATIC_RUNTIME OFF` | the MSVC runtime row `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL` applies to third-party code too; a mismatch is a link error (LNK2038) |

zstd's CMake project lives in `build/cmake/` of the archive; the entry's `cmakeSourceDir` field passes that directory to `add_subdirectory` (`Zenith2/Docs/Architecture/BuildSystem.md → "The manifest dependencies section"`, `Zenith2/Docs/Architecture/BuildSystem.md → "Third-party integration"`), because the project refers to `../../lib` and `stripPrefix` cannot express it.

Exactly one copy of each library is linked into any target. `ZenithCook` links KTX-Software, whose source tree carries its own zstd for basisu supercompression; the ASSET cook ticket that integrates KTX-Software configures it against this manifest's zstd (or keeps its copy symbol-private) so that the `win-tools` link of `ZenithCook` reports no duplicate `ZSTD_`/`ZDICT_` symbol (`LNK2005`, `LNK4006`); the requirement is COMPRESS's, the mechanism ASSET's, and it is listed in the cross-area notes.

In `win-asan`, `linux-tsan` and the `Hwasan` configuration both libraries are compiled from the pinned source with the sanitizer flags (Z2-D-011; `Zenith2/Docs/Architecture/BuildSystem.md → "Third-party integration"` [Z2-N-ARCH-201]); neither has a `prebuilt` entry.

### Conformance benchmark and fuzz targets

| Target | Sources | Registered |
|---|---|---|
| `ZenithCompression_Conformance` | `ZenithCompression/Tests/Conformance/**`, `ZenithCompression_Zlib/Tests/ConformanceAdapter/**`, `ZenithCompression_Zstd/Tests/ConformanceAdapter/**` | CTest in `ZENITH_HOST` presets (`win` Debug and Release, `win-asan`, `linux-tsan`), one entry per case per backend |
| `Zenith_Fuzz_CompressedSectionDesc`, `Zenith_Fuzz_ZlibDecode`, `Zenith_Fuzz_ZlibRoundTrip`, `Zenith_Fuzz_ZstdDecode`, `Zenith_Fuzz_ZstdRoundTrip`, `Zenith_Fuzz_ZstdDictionaryLoad` | one `Tests/Fuzz/<Stem>.cpp` each | `win-asan`, 60-second smoke through CTest; full duration in stage acceptance |
| `Zenith_Bench_Compression` | `Zenith2/Benchmarks/Compression/Internal/**` | CTest smoke `--smoke` in `win`; the full run is human-run (see "Benchmarks") |

```cmake
# Zenith2/Benchmarks/Compression/CMakeLists.txt
zenith_add_benchmark(Compression DEPENDS ZenithCompression ZenithCompression_Zlib ZenithCompression_Zstd ZenithCore
	SUPPORTED win android-arm64)
```

| Benchmark file | Content |
|---|---|
| `Zenith2/Benchmarks/Compression/Internal/Zenith_CompressionBenchMain.cpp` | argument parsing, the run loop |
| `Zenith2/Benchmarks/Compression/Internal/Zenith_CompressionBenchCorpus.h`, `.cpp` | the benchmark corpus generator and the optional directory corpus |
| `Zenith2/Benchmarks/Compression/Internal/Zenith_CompressionBenchRunner.h`, `.cpp` | nanobench measurement per (codec, level, class, mode) |
| `Zenith2/Benchmarks/Compression/Internal/Zenith_CompressionBenchReport.h`, `.cpp` | statistics and the JSON report |

## Public API

### Header map

| Header | Declares | Consumers |
|---|---|---|
| `ZenithCompression/Zenith_CompressionTypes.h` | `Zenith_CompressionCodecID`, `Zenith_CompressionError`, `Zenith_CompressionMode`, `Zenith_CompressionContextState`, constants, fault points, `Zenith_CompressionDesc`, `Zenith_CompressionContextDesc`, `Zenith_CompressionTrainingDesc`, `Zenith_CompressionCodecInfo`, `Zenith_CompressionFrameInfo`, `Zenith_CompressionProgress`, `Zenith_ToString` overloads, `Zenith_ParseCompressionCodecName`, `Zenith_ValidateCompressionSetting` | everyone |
| `ZenithCompression/Zenith_Compression.h` | `Zenith_Compression`, `Zenith_PfnCreateCompression` | everyone |
| `ZenithCompression/Zenith_CompressionContext.h` | `Zenith_CompressionContext` | streaming users (SAVE, ASSET) |
| `ZenithCompression/Zenith_CompressionDictionary.h` | `Zenith_CompressionDictionary` | PACK, ASSET, the cook |
| `ZenithCompression/Zenith_CompressionCodecSet.h` | `Zenith_CompressionCodecSetDesc`, `Zenith_CompressionCodecSet` | readers of cooked data, the composition root |
| `ZenithCompression/Zenith_CompressedSection.h` | `Zenith_CompressedSectionDesc`, `Zenith_CompressedSectionLimits`, `Zenith_CompressSectionOptions`, the descriptor and section functions | every cooked-format reader and writer |
| `ZenithCompression/Zenith_CompressionEngine.h` | `Zenith_CompressionEngineDirective`, `Zenith_CompressionEngineDesc`, `Zenith_CompressionEngineStep`, `Zenith_CompressionStreamEngine` | the two backends alone |
| `ZenithCompression/Zenith_CompressionAllocator.h` | `Zenith_CompressionAllocator` | the two backends alone |
| `ZenithCompression_Zlib/Zenith_Compression_Zlib.h` | `Zenith_CreateCompression_Zlib` | the composition root, `ZenithCook`, `ZenithEditor`, conformance, benchmarks |
| `ZenithCompression_Zstd/Zenith_Compression_Zstd.h` | `Zenith_CreateCompression_Zstd` | as above |

Every fallible function declared in `Zenith2/Docs/Design/COMPRESS/PublicAPI.md` returns `Zenith_Result<…, Zenith_CompressionError>`, and every public type carries the contract block shown, in the grammar of `Zenith2/Docs/Contracts.md → "Contract blocks per type"`. No public header includes a zlib or zstd header, and no public declaration names a zlib or zstd type.

### Declarations in PublicAPI.md

The full declarations and contract blocks live in `Zenith2/Docs/Design/COMPRESS/PublicAPI.md` (split out for length), one `##` heading per header:

| Heading |
|---|
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionTypes.h"` |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_Compression.h"` |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionEngine.h"` |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionAllocator.h"` |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionContext.h"` |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionDictionary.h"` |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionCodecSet.h"` |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressedSection.h"` |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Backend factory headers"` |

### Codec metadata table

| Field | zlib | zstd |
|---|---|---|
| `m_eCodec` | `COMPRESSION_CODEC_ID_ZLIB` (1) | `COMPRESSION_CODEC_ID_ZSTD` (2) |
| `m_szName` | `"zlib"` | `"zstd"` |
| `m_szLibraryVersion` | `zlibVersion()`, `"1.3.1"` | `ZSTD_versionString()`, `"1.5.7"` |
| level range | 1 … 9 | 1 … 19 |
| default level | 6 | 3 |
| `m_bSupportsDictionaries` | `false` | `true` |
| stream format | RFC 1950: 2-byte header, deflate, Adler-32 | RFC 8878: one frame, content checksum, content size when known |
| decode window | 32 KiB (`windowBits` 15) | at most 8 MiB (`windowLog` 23) |
| typical use | interchange formats that require deflate; data a third-party reader must open | every cooked Zenith2 asset, saves, replays |

<!-- informative -->
Levels excluded on purpose: 0 (zlib's stored mode, whose block boundaries depend on the caller's output buffer, and zstd's alias for "default"); zstd's negative "fast" levels (their ratio on cooked content is below zlib level 1, and no consumer needs more than zstd level 1's encode speed); zstd 20–22 ("ultra"), whose 32–128 MiB windows break the 8 MiB decode cap that keeps a streaming zstd decoder under 9 MiB on the S24. Storing without compression is codec `NONE`, not a level.
<!-- /informative -->

## Data structures and formats

### Codec ID registry

| Value | Enumerator | Meaning | Written by the cook | Accepted by readers |
|---|---|---|---|---|
| 0 | `COMPRESSION_CODEC_ID_NONE` | stored: the section bytes are the payload | yes | yes, without a codec object |
| 1 | `COMPRESSION_CODEC_ID_ZLIB` | one RFC 1950 zlib stream written by `ZenithCompression_Zlib` | yes | yes, when the set holds the zlib codec |
| 2 | `COMPRESSION_CODEC_ID_ZSTD` | one RFC 8878 zstd frame written by `ZenithCompression_Zstd` | yes | yes, when the set holds the zstd codec |
| 3 – 127 | none | reserved for future Zenith2 codecs; each needs a `Z2-D-` decision, its own backend library and a conformance adapter | no | no: `COMPRESSION_ERROR_UNKNOWN_CODEC` |
| 128 – 254 | none | experimental and test codecs (`Zenith_FakeCompression` uses 200) | never | only when `m_bAllowExperimentalCodecs` is set, which no runtime or cook path sets |
| 255 | none | invalid; never written | no | no: `COMPRESSION_ERROR_UNKNOWN_CODEC` |

Rules: an enumerator's value never changes and a value is never reused for a different codec, including after a codec is retired (a retired codec's value stays reserved); the text names `"none"`, `"zlib"`, `"zstd"` are the only spellings in settings files; adding a codec adds a row here, an enumerator, a backend library, a conformance adapter and a `Z2-D-` decision in one diff.

### Compressed section descriptor on disk

Every compressed byte range in any Zenith2 cooked file is described by one 24-byte record, little-endian, with no padding and no alignment requirement (it is read byte by byte):

| Offset | Size | Field | Encoding | Rule |
|---|---|---|---|---|
| 0 | 1 | codec ID | `uint8` | a value of "Codec ID registry" |
| 1 | 1 | level | `uint8` | the resolved level the cook used (1–9 zlib, 1–19 zstd, 0 for NONE); informational for provenance, never needed to decode |
| 2 | 2 | flags | `uint16` LE | bit 0 `uCOMPRESSED_SECTION_FLAG_DICTIONARY`; bits 1–15 zero |
| 4 | 4 | dictionary ID | `uint32` LE | 0 without a dictionary; otherwise in [32,768, 2,147,483,647] |
| 8 | 8 | stored bytes | `uint64` LE | bytes of the section in the file; at most 4 GiB |
| 16 | 8 | decoded bytes | `uint64` LE | bytes after decoding; at most 4 GiB |

Worked example — a zstd level-19 section of 0x1234 stored bytes decoding to 65,536 bytes, no dictionary:

```text
02 13 00 00  00 00 00 00  34 12 00 00 00 00 00 00  00 00 01 00 00 00 00 00
```

`Zenith_EncodeCompressedSectionDesc` writes the bytes with shifts and `Zenith_DecodeCompressedSectionDesc` reads them the same way; neither copies a C++ object's bytes (Z2-D-038, `Zenith2/Docs/Contracts.md → "Reflection"` [Z2-N-CONTRACT-377]).

### How cooked formats embed the descriptor

`Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` requires every cooked file's header to record "the compression codec ID of each compressed section" ([Z2-N-CONTRACT-527]) and `Zenith2/Docs/Architecture.md → "Security architecture"` (table "Parser limits") bounds the `.zpak` archive's compressed and decompressed sizes. COMPRESS satisfies both through the descriptor:

| Rule | Owner of the rule | Owner of the container |
|---|---|---|
| each compressed section of a cooked file or archive entry is described by exactly one `Zenith_CompressedSectionDesc` record stored in that file's header or table of contents | COMPRESS | ASSET (asset files), PACK (archives), SAVE (saves and replays) |
| the section's stored bytes are exactly one zlib stream, one zstd frame, or the raw payload — nothing before, between or after | COMPRESS | the container places the bytes and records their offset in its own field |
| a reader decodes the descriptor with `Zenith_DecodeCompressedSectionDesc` and the section with `Zenith_DecompressSection`, which selects the codec from the descriptor through a `Zenith_CompressionCodecSet` — never from configuration (Z2-D-004, [Z2-N-ARCH-271]) | COMPRESS | — |
| a container that holds dictionaries stores each as a `NONE` section and passes the loaded dictionaries to `Zenith_DecompressSection` as a span; dictionary IDs are unique within one container | COMPRESS (uniqueness check) | PACK (the dictionary table) |
| the container's own header version covers the descriptor layout; a descriptor change is a container format version bump and a re-cook | COMPRESS and the container owner together | — |

The request that ASSET, PACK and SAVE embed the record is in the cross-area notes.

### zlib stream format as written

| Part | Bytes | Value written | Reader rule |
|---|---|---|---|
| CMF | 1 | `0x78`: CM 8 (deflate), CINFO 7 (32 KiB window) | CM ≠ 8 → `CORRUPT_INPUT`; CINFO > 7 → `CORRUPT_INPUT`; CINFO < 7 accepted (smaller windows are valid deflate) |
| FLG | 1 | FLEVEL as zlib derives it from the level, FDICT 0, FCHECK | `(CMF·256 + FLG) mod 31 ≠ 0` → `CORRUPT_INPUT`; FDICT 1 → `UNSUPPORTED` |
| deflate blocks | n | produced by `deflate` with the pinned parameters | `inflate` |
| Adler-32 | 4 | big-endian checksum of the decoded bytes | mismatch → `CORRUPT_INPUT` ("incorrect data check") |

A zero-length input compresses to an 8-byte stream (2 header, 2 empty final block, 4 Adler-32). gzip (RFC 1952) and raw deflate (RFC 1951) are neither written nor accepted.

### zstd frame format as written

| Part | Value written | Reader rule |
|---|---|---|
| magic | `0xFD2FB528` little-endian | anything else → `CORRUPT_INPUT` (legacy formats are compiled out) |
| Frame_Header_Descriptor | `Content_Checksum_flag` 1; `Frame_Content_Size_flag` set whenever the pledged size is known (always for one-shot and section output); `Dictionary_ID_flag` set when a dictionary was used; `Single_Segment_flag` as zstd derives it | a skippable frame (`0x184D2A50`–`0x184D2A5F`) → `CORRUPT_INPUT` |
| Window_Descriptor | derived from the level and pledged size; at most 8 MiB for levels 1–19 | Window_Size > 2^23 → `LIMIT_EXCEEDED` |
| why Zenith2 frames always pass the cap | zstd sets `Single_Segment_flag` only when the pledged size is at most the level's window, and a Single_Segment frame's Window_Size is its Frame_Content_Size, so it is ≤ 2^23 bytes; every other frame carries a Window_Descriptor, which is ≤ 2^23 for levels 1–19 | `Test_CompressionZstd_EveryLevel_WindowAtMost8MiB` |
| Dictionary_ID | the dictionary's ID when one was used | absent while a dictionary is supplied, present while none is, or different → `DICTIONARY_MISMATCH` |
| Frame_Content_Size | the pledged size when known | greater than the destination → `DESTINATION_TOO_SMALL` before decoding; different from the section descriptor → `SIZE_MISMATCH` |
| blocks | at most 128 KiB each | `ZSTD_decompress*` |
| Content_Checksum | low 32 bits of XXH64 of the decoded bytes | mismatch → `CORRUPT_INPUT` |

A Zenith2 section always holds exactly one frame: bytes after the frame → `TRAILING_DATA`. `Zenith_DecompressSection` additionally requires `Content_Checksum_flag`; the raw `Decompress` accepts frames without it (for data another encoder wrote). A zero-length input compresses to a frame of about 13 bytes.

### Dictionary format and identifiers

| Property | Rule |
|---|---|
| format | the zstd dictionary format: magic `0xEC30A437` little-endian, `Dictionary_ID` (`uint32` LE), entropy tables, content; `ZSTD_dct_fullDict` |
| raw-content dictionaries | refused by `CreateDictionary` with `INVALID_DICTIONARY`: they carry no ID, so a frame could not name them |
| ID range | [32,768, 2,147,483,647] (`uCOMPRESSION_DICTIONARY_ID_MIN`, `…_MAX`), zstd's non-reserved range; 0 means "none" |
| trained IDs | `ZDICT_*` with `dictID = 0` derives the ID from the XXH64 of the dictionary content: `(hash mod (2^31 − 32,768)) + 32,768`, so the same content always gets the same ID |
| size | 256 B to 1 MiB; the default training capacity is 112,640 bytes (zstd's default) |
| storage | a cooked `NONE` section inside the container that uses it (PACK); never inline in a frame |
| memory | a `Zenith_CompressionDictionary` holds one CDict (at its level) and one DDict, each a copy of the bytes (`ZSTD_dlm_byCopy`); `GetMemoryBytes` reports both |

<!-- informative -->
Why dictionaries (choice C6). A large share of cooked entries are small: material parameter blocks, prefab fragments, scene-cell records, script bytecode, UI documents, small meshes. Below about 16 KiB a zstd frame has little history to match against and pays its entropy-table and header cost per entry, so ratios collapse towards 1. A dictionary trained on a class's own samples supplies that history; zstd's documentation reports several-fold ratio gains on small JSON-like records with faster decoding. The alternative — grouping small entries into solid blocks — keeps the ratio but makes reading one entry decode its whole block, which fights streaming's random access. The API therefore supports dictionaries on zstd; whether an asset class uses one is PACK's policy, decided from the "dictionary" rows of the benchmark report. zlib's preset dictionary (`deflateSetDictionary`) is not offered: zlib is the interchange codec, and interchange readers do not share Zenith2's dictionaries.
<!-- /informative -->

### Staging buffers and context layout

| Object | Size | Allocated | Present in |
|---|---|---|---|
| `Zenith_CompressionContext` object | `sizeof` (under 256 bytes) | `Zenith_TryNew`, general heap | every context |
| `Zenith_CompressionStaging` | 262,144 bytes: 131,072 input + 131,072 output | `Zenith_TryNew`, general heap | compress contexts |
| engine object | backend `sizeof` (under 128 bytes) | general heap (`DoCreateEngine`) or the scratch arena (`DoConstructEngine`) | every context and one-shot call |
| library state | "Memory per context" | through `Zenith_CompressionAllocator`: root heap for contexts, the scratch arena for one-shot calls | every engine |
| one-shot output staging | 131,072 bytes | the scratch arena | every one-shot `Compress` |

### Memory per context

The bounds are what `GetContextMemoryBound`, `GetCompressScratchBound` and `GetDecompressScratchBound` return (formulas in "Scratch and memory bounds"); the approximate values are for orientation.

| Codec, mode | Library state | Plus | Approximate total |
|---|---|---|---|
| zlib compress (any level) | `(1 << (15 + 2)) + (1 << (8 + 9)) + 8,192` = 270,336 | 262,144 staging (context) or 131,072 (one-shot) | 530 KiB context, 400 KiB one-shot |
| zlib decompress | `(1 << 15) + 16,384` = 49,152 | — | 52 KiB |
| zstd compress, level 1, unknown size | `ZSTD_estimateCStreamSize_usingCParams` | staging as above | about 1.5 MiB |
| zstd compress, level 3, unknown size | as above | as above | about 3.5 MiB |
| zstd compress, level 19, unknown size | as above | as above | about 90 MiB (cook and tools only) |
| zstd compress, known size S | as above with `ZSTD_getCParams(level, S, dictSize)` | as above | shrinks with S: tables are capped near log2(S) |
| zstd decompress, streaming | `ZSTD_estimateDStreamSize(1 << 23)` | — | about 8.3 MiB (the 8 MiB window cap) |
| zstd decompress, one-shot | `ZSTD_estimateDCtxSize()` | — | about 160 KiB |

Runtime guidance that follows from the table (the consuming areas apply it): on the S24, streaming zstd compression uses level ≤ 3 (SAVE), decode of whole sections uses one-shot `Decompress` into the destination (ASSET), and streaming zstd decompression is reserved for sections too large to hold decoded, because only it pays the 8 MiB window.

### Golden vector fixture format

`Zenith2/Engine/ZenithCompression/Tests/Conformance/Fixtures/golden_v1.txt` is UTF-8 text, LF line endings:

```text
# zenith2.compression.golden 1
library zlib 1.3.1
library zstd 1.5.7
# codec level class seed bytes pledged dictionary compressedBytes fnv1a64
zlib 6 text 1 1048576 pledged none 309488 1f0c2a7e9b3d4c55
zstd 19 floats 3 1048576 pledged none 512211 6a01bb9d20e7f413
zstd 3 json-small 7 4096 pledged json-small-7 1203 0c77e1a39d5b6f02
```

(The values above show the syntax; the recorded file holds the measured values.) Fields: codec name; resolved level; corpus class and seed of "Conformance corpus"; input byte count; `pledged` or `unpledged` (zstd streaming without a pledged size; zlib rows are always `pledged` because zlib output does not depend on it); `none` or the dictionary's corpus key; the compressed byte count; FNV-1a 64 of the compressed bytes as 16 lower-case hex digits (offset basis `0xcbf29ce484222325`, prime `0x100000001b3`). The FNV-1a function is defined in `Zenith_CompressionConformance.h`; it is test code, chosen so the fixture depends on no open decision (D-SEL-HASH (open, Stage 1) does not govern it).

The vector set is the cross product of: zlib levels {1, 6, 9} and zstd levels {1, 3, 9, 19}; classes `empty` (0 bytes), `one` (1 byte), `chunk-plus-one` (131,073 bytes of `text`), `zeros` (1 MiB), `text` (1 MiB), `json` (256 KiB), `floats` (1 MiB), `random` (64 KiB), `mixed` (3 MiB); `pledged` for all, plus `unpledged` for the zstd rows at levels 1 and 3; the level-9 and level-19 zstd rows use `mixed` at 1 MiB instead of 3 MiB, so level 19 feeds about 4.4 MiB in total; plus four dictionary rows (zstd levels 3 and 19 × `json-small` inputs of 512 and 4,096 bytes with the dictionary trained from `json-small` seed 7).

Recording: the conformance executable writes the file instead of comparing when the environment variable `ZENITH2_COMPRESSION_RECORD_GOLDEN` names an output path; the recording run is made only in the diff that changes a library pin (see "Re-cook and migration"), and that diff commits the new file. No shipping target reads the variable (it lives in test code).

### Compression setting fragment

The importer `.meta` settings and the PACK archive recipe (both authoring documents owned by ASSET and PACK) carry compression as this JSON fragment; COMPRESS owns its value rules:

```json
"compression": { "codec": "zstd", "level": 19, "dictionary": null }
```

| `codec` | `level` | `dictionary` | `Zenith_ValidateCompressionSetting` |
|---|---|---|---|
| `"none"` | 0 | `null` | ok |
| `"none"` | ≠ 0 | any | `INVALID_LEVEL` |
| `"none"` | 0 | a GUID | `UNSUPPORTED` |
| `"zlib"` | 0 or 1–9 | `null` | ok |
| `"zlib"` | any other | any | `INVALID_LEVEL` |
| `"zlib"` | valid | a GUID | `UNSUPPORTED` |
| `"zstd"` | 0 or 1–19 | `null` or the `Zenith_PersistentID` of a dictionary asset | ok |
| `"zstd"` | any other | any | `INVALID_LEVEL` |
| anything else | — | — | `UNKNOWN_CODEC` (from `Zenith_ParseCompressionCodecName`) |

The settings file is untrusted input: a bad value is a recoverable cook error naming the file and field, never an assert (`Zenith2/Docs/Contracts.md → "Errors and results"`).

## Algorithms and state machines

### Canonical chunking

Definitions: `C` is `ulCOMPRESSION_STREAM_CHUNK_BYTES` (131,072). A compression stream's **segments** are delimited by its start and each completed `Flush`. A segment's **canonical chunks** are its consecutive `C`-byte ranges; its last chunk may be shorter.

Invariants every compression engine call obeys, in streaming and in one-shot compression alike:

1. The engine receives input only in `CONTINUE` steps, and the input of a step is always the not-yet-consumed tail of exactly one canonical chunk. A chunk shorter than `C` is given to the engine only when its segment ends (`Flush` or `End`).
2. `FLUSH` and `END` steps always carry empty input.
3. Every compression step's output span is the whole 131,072-byte output staging buffer, which is empty when the step starts; bytes are copied from it to the caller only afterwards.
4. The zstd engine's first call after creation or `Reset` is an empty `CONTINUE` made by the engine itself (it allocates the workspace with the pledged size already set), so zstd never derives a pledged size from an `END` call.

Consequence: the sequence of engine calls — input bytes, output capacity, directive — is a function of the input bytes and the flush offsets alone. How the caller slices its input across `Process` calls and how large its output spans are changes only when bytes are copied, never what the library is asked. That is the determinism mechanism of choice C1; the conformance cases `StreamingInputChunking_DoesNotChangeBytes` and `StreamingOutputChunking_DoesNotChangeBytes` test it rather than rely on it.

Cost: at most one extra copy of each input byte (only when a caller's slice does not cover a whole chunk, or when a chunk's step fills the output staging and its tail must be kept) and one copy of every output byte.

### Compression driver loop

`Zenith_CompressionDriver` (`ZenithCompression/Internal/Zenith_CompressionDriver.h`) implements the invariants for both callers. It holds the engine pointer, the input staging (absent for one-shot, whose source is contiguous), the output staging, `m_ulStagedInputBytes`, `m_ulChunkOffset`, `m_ulStagedOutputOffset` and `m_ulStagedOutputBytes`. Each public operation performs **at most one engine step** per call, which bounds the work (and so the cancellation latency) of one `Process`, `Flush` or `End` call to one chunk.

`Deliver(xOutput, ulOut)`: copy `min(staged − offset, xOutput.GetCount() − ulOut)` bytes from the output staging; when the staging is emptied set both counters to 0.

`StepChunk(xChunkTail, eDirective)`: precondition output staging empty; `Step(xChunkTail, outputStaging[0, C), eDirective)`; on error return it; record the produced count as the staging's content.

`ProcessOnce(xInput, xOutput)` for compression, returning `{consumed, produced}`:

1. `Deliver`. If staged output remains, return `{0, produced}`: the caller must supply more output before more input is taken.
2. If `m_ulChunkOffset > 0` (the staged chunk was partly consumed): `StepChunk(staging[m_ulChunkOffset, m_ulStagedInputBytes), CONTINUE)`; advance `m_ulChunkOffset`; when it reaches `m_ulStagedInputBytes` clear the staging; `Deliver`; return `{0, produced}`.
3. Else if the input staging is empty and `xInput.GetCount() ≥ C`: check the pledged size (step 5); `StepChunk(xInput[0, C), CONTINUE)`; if the engine consumed fewer than `C` bytes, copy the whole chunk `xInput[0, C)` into the input staging, set `m_ulStagedInputBytes = C` and `m_ulChunkOffset` to the consumed count; report `consumed = C`; `Deliver`; return.
4. Else: `n = min(C − m_ulStagedInputBytes, xInput.GetCount())`; check the pledged size; append `xInput[0, n)` to the staging; `consumed = n`; if the staging now holds `C` bytes, `StepChunk(staging[0, C), CONTINUE)` and set `m_ulChunkOffset`, clearing the staging when it was all consumed; `Deliver`; return.
5. Pledged-size check (steps 3 and 4): when `m_ulPledgedSourceBytes` is known and `total + consumed` would exceed it, fail with `COMPRESSION_ERROR_SIZE_MISMATCH` before touching the engine.

`FinishOnce(xOutput, eDirective)` for `FLUSH` and `END`, returning `{produced, complete}`:

1. `Deliver`; if staged output remains, return `{produced, false}`.
2. If `m_ulChunkOffset > 0`: continue the chunk as in step 2 above; return `{produced, false}`.
3. Else if `m_ulStagedInputBytes > 0` (the segment's partial last chunk): `StepChunk(staging[0, m_ulStagedInputBytes), CONTINUE)`; set `m_ulChunkOffset` (clear when fully consumed); `Deliver`; return `{produced, false}`.
4. Else if the directive has already reported complete: return `{produced, true}` (all of its output has now been delivered).
5. Else: `StepChunk(empty, eDirective)`; remember `bDirectiveComplete`; `Deliver`; return `{produced, bDirectiveComplete && staging empty}`.

Every size addition in the driver is overflow-checked; an overflow is `COMPRESSION_ERROR_SIZE_OVERFLOW` (which needs 2^64 bytes and exists for the check's completeness).

### Streaming compression context state machine

| State \ call | `Process(in, out)` | `Flush(out)` | `End(out)` | `Reset()` | error in a call |
|---|---|---|---|---|---|
| READY | → ACTIVE; `ProcessOnce` | → FLUSHING; `FinishOnce(FLUSH)`; → ACTIVE when complete | pledged check; → ENDING; `FinishOnce(END)`; → FINISHED when complete | stays READY | → FAILED |
| ACTIVE | `ProcessOnce` | as READY | as READY | → READY | → FAILED |
| FLUSHING | `ZENITH_CHECK` failure: finish the flush first | `FinishOnce(FLUSH)`; → ACTIVE when complete | `ZENITH_CHECK` failure | → READY | → FAILED |
| ENDING | `ZENITH_CHECK` failure | `ZENITH_CHECK` failure | `FinishOnce(END)`; → FINISHED when complete | → READY | → FAILED |
| FINISHED | `ZENITH_CHECK` failure | `ZENITH_CHECK` failure | returns `{0, 0, true}` | → READY | — |
| FAILED | returns the stored error | returns the stored error | returns the stored error | → READY | — |

"Pledged check" at `End`: when a pledged size was given and the total input differs from it, the call fails with `COMPRESSION_ERROR_SIZE_MISMATCH` before the `END` step, so no frame with a wrong `Frame_Content_Size` is ever completed. `m_bFinished` in the returned progress is true exactly on the call that makes the transition to ACTIVE (after `Flush`) or FINISHED (after `End`), and on every subsequent `End` in FINISHED.

`Reset` calls the engine's `Reset` (zlib `deflateReset`; zstd `ZSTD_CCtx_reset(ZSTD_reset_session_only)`, the pledged size and dictionary re-applied, then the invariant-4 empty step), clears the staging counters and totals, and keeps every allocation. An engine `Reset` failure (only possible on an invalid stream, never for a live context) leaves the context FAILED with `COMPRESSION_ERROR_INTERNAL`. A context reset and reused produces exactly the bytes a fresh context would.

### Streaming decompression context state machine

The decompression context has no staging: input goes to the engine directly and output is written to the caller's span, because decoded bytes are unique whatever the slicing.

| State \ call | `Process(in, out)` | `End(out)` | `Flush` | `Reset()` |
|---|---|---|---|---|
| READY | → ACTIVE; as ACTIVE | as ACTIVE | `ZENITH_CHECK` failure | stays READY |
| ACTIVE | step once (below) | drain once (below) | `ZENITH_CHECK` failure | → READY |
| FINISHED | non-empty input → FAILED `TRAILING_DATA`; empty input → `{0, 0, true}` | `{0, 0, true}` | `ZENITH_CHECK` failure | → READY |
| FAILED | the stored error | the stored error | `ZENITH_CHECK` failure | → READY |

Step once (`Process` in ACTIVE):

1. `in = xInput[0, min(count, C))`. The output span is `xOutput` narrowed to `cap + 1` bytes when a limit applies, where `cap` is the smaller of `m_ulMaxDecodedBytes − total` and `m_ulExpectedDecodedBytes − total`; one byte beyond the cap is what detects an over-long frame without writing past the caller's span.
2. `Step(in, out, CONTINUE)`; map an error and move to FAILED.
3. `total += produced`; beyond `m_ulExpectedDecodedBytes` → FAILED `SIZE_MISMATCH`; beyond `m_ulMaxDecodedBytes` → FAILED `LIMIT_EXCEEDED`.
4. If the engine reports the frame's end: when fewer bytes were consumed than this call's `xInput` holds → FAILED `TRAILING_DATA`; when an expected size was declared and `total` differs → FAILED `SIZE_MISMATCH`; else → FINISHED.
5. Return `{consumed, produced, state == FINISHED}`.

Drain once (`End` in ACTIVE): `Step(empty, xOutput, CONTINUE)`; the frame's end → FINISHED (expected-size check as above) returning `{0, produced, true}`; bytes produced without the end → `{0, produced, false}` (call `End` again with more output); nothing produced and no end → FAILED `TRUNCATED_INPUT`.

A stuck engine (non-empty input and output, nothing consumed or produced, no end) is FAILED `COMPRESSION_ERROR_INTERNAL`, logged with the library's message; no correct library reaches it.

### One-shot compression

`Zenith_Compression::Compress` (implemented once in `Internal/Zenith_CompressionOneShot.cpp`):

1. `ZENITH_ASSERT` that no span has a null data pointer with a non-zero count. Resolve the level. With a dictionary: codec mismatch → `DICTIONARY_MISMATCH`; codec without dictionaries → `UNSUPPORTED`; level 0 resolves to the dictionary's `GetLevel()`, and any other level that differs from it → `INVALID_LEVEL`.
2. Take `xMarker = xScratch.GetMarker()`; build a `Zenith_CompressionAllocator` in arena mode on `xScratch` with the codec's fault injector.
3. `DoConstructEngine({COMPRESS, level, m_ulPledgedSourceBytes = xSource.GetCount(), dictionary, 23}, allocator, xScratch)`; allocate the 131,072-byte output staging from `xScratch` (failure → `OUT_OF_MEMORY`).
4. Run the driver: every canonical chunk of `xSource` with `CONTINUE` (repeating a chunk's tail while the engine leaves it partly consumed), the last partial chunk with `CONTINUE`, then `END` until complete. After every step copy the staged bytes to `xDestination` at the running offset; if they do not fit, stop with `DESTINATION_TOO_SMALL`, having written nothing beyond the destination.
5. On every exit path: `Zenith_Destroy(xScratch, pxEngine)`, then `xScratch.ResetToMarker(xMarker)`; emit the zone and counters; log a failure once.
6. Return the compressed byte count.

Because steps 3–4 issue the same engine calls as a `Zenith_CompressionContext` created with `m_ulPledgedSourceBytes` equal to the source size and never flushed, one-shot output equals that context's output byte for byte (the conformance case `OneShotEqualsStreamingWithPledgedSize`). A zlib stream is the same whether or not a size was pledged; a zstd frame differs (its header records the content size only when pledged).

### One-shot decompression with zlib

`Zenith_CompressionZlib::DoDecompress` with an allocator the facade built on `xScratch`:

1. Fewer than 2 source bytes → `TRUNCATED_INPUT`. Parse CMF and FLG per "zlib stream format as written" (a dictionary argument is refused earlier by the facade with `UNSUPPORTED`).
2. `inflateInit2(&xStream, 15)` with `zalloc`/`zfree` thunks bound to the allocator (the fault injector's `uFAULT_POINT_COMPRESSION_LIBRARY_INIT` is consulted first) → `Z_MEM_ERROR` → `OUT_OF_MEMORY`; any other failure → `INTERNAL`.
3. Loop: `avail_in = min(remaining input, 2^30)`, `avail_out = min(remaining output, 2^30)`, `inflate(Z_NO_FLUSH)`; advance 64-bit counters kept by the wrapper (zlib's `total_in`/`total_out` are `uLong`, 32 bits on Windows, and are never read). Results: `Z_STREAM_END` ends the loop; `Z_OK` continues; `Z_BUF_ERROR` with no input left → `TRUNCATED_INPUT`, with no output left → `DESTINATION_TOO_SMALL`, otherwise `INTERNAL`; the rest per "Error mapping for zlib".
4. After `Z_STREAM_END`: unconsumed source bytes → `TRAILING_DATA`.
5. `inflateEnd` on every path; return the decoded byte count.

### One-shot decompression with zstd

`Zenith_CompressionZstd::DoDecompress`:

1. Frame header: `ZSTD_getFrameHeader_advanced(&xHeader, src, size, ZSTD_f_zstd1)`; a positive return (more bytes needed) → `TRUNCATED_INPUT`; an error per "Error mapping for zstd"; a skippable frame → `CORRUPT_INPUT`.
2. `windowSize > 2^23` → `LIMIT_EXCEEDED`.
3. Dictionary: the frame's `dictID` and the supplied dictionary must agree (both absent, or equal IDs) else `DICTIONARY_MISMATCH`.
4. A known `frameContentSize` larger than the destination → `DESTINATION_TOO_SMALL`, before any decoding.
5. `ZSTD_findFrameCompressedSize(src, size)`: `srcSize_wrong` → `TRUNCATED_INPUT`; another error → `CORRUPT_INPUT`; a frame shorter than the source → `TRAILING_DATA`.
6. `ZSTD_createDCtx_advanced(customMem)` bound to the allocator → null → `OUT_OF_MEMORY`; `ZSTD_DCtx_setParameter(ZSTD_d_windowLogMax, 23)` and `ZSTD_d_format = ZSTD_f_zstd1`; with a dictionary `ZSTD_DCtx_refDDict`.
7. `ZSTD_decompressDCtx(dctx, dst, capacity, src, frameSize)`; an error is mapped; zstd itself checks the content size and the checksum.
8. `ZSTD_freeDCtx` on every path; return the decoded count.

One-shot decoding writes straight into the destination and allocates no window buffer, which is why it needs only about 160 KiB of scratch.

### Section compression and decompression

`Zenith_CompressSection(xCodec, xSource, iLevel, pxDictionary, xDestination, xScratch, xOptions)`:

1. `xSource.GetCount() > ulCOMPRESSION_SECTION_MAX_DECODED_BYTES` → `LIMIT_EXCEEDED`.
2. `ulNeed = GetCompressBound(source)`, raised to the source size when `m_bStoreIfNotSmaller`; `xDestination.GetCount() < ulNeed` → `DESTINATION_TOO_SMALL`. (Requiring the full bound makes the store-or-compress decision independent of the caller's buffer size.)
3. Resolve the level; `ulStored = xCodec.Compress(…)`.
4. When `m_bStoreIfNotSmaller` and `ulStored ≥ source size`: copy the source over the destination and return `{NONE, 0, 0, 0, size, size}`.
5. `ulStored > ulCOMPRESSION_SECTION_MAX_STORED_BYTES` → `LIMIT_EXCEEDED`.
6. Return `{codec, level, dictionary ? 1 : 0, dictionary ? its ID : 0, ulStored, size}`.

`Zenith_StoreSection(xSource, xDestination)` is the path for the setting `"codec": "none"`: a source above `ulCOMPRESSION_SECTION_MAX_DECODED_BYTES` → `LIMIT_EXCEEDED`; a destination smaller than the source → `DESTINATION_TOO_SMALL`; otherwise it copies the source and returns `{NONE, 0, 0, 0, size, size}`, which descriptor rule 6 validates.

`Zenith_DecompressSection(xCodecs, xDesc, apxDictionaries, xStored, xDestination, xScratch, xLimits)`:

1. `Zenith_ValidateCompressedSectionDesc(xDesc, xLimits)`.
2. `xStored.GetCount() ≠ m_ulStoredBytes` → `SIZE_MISMATCH`; `xDestination.GetCount() < m_ulDecodedBytes` → `DESTINATION_TOO_SMALL`.
3. `NONE`: copy `m_ulDecodedBytes` bytes; done.
4. `xCodecs.Find(m_eCodec)` → `UNKNOWN_CODEC`.
5. More than `uCOMPRESSION_MAX_DICTIONARIES_PER_LOOKUP` (64) dictionaries, or two with one ID → `INVALID_DESC`. With the dictionary flag: the dictionary with the descriptor's ID (linear search) → absent → `DICTIONARY_MISMATCH`; its codec differs → `DICTIONARY_MISMATCH`.
6. `ReadFrameInfo(xStored)`: zstd without `Content_Checksum_flag` → `CORRUPT_INPUT`; a recorded content size ≠ `m_ulDecodedBytes` → `SIZE_MISMATCH`.
7. `Decompress(xStored, xDestination[0, m_ulDecodedBytes), dictionary, xScratch)`; `DESTINATION_TOO_SMALL` from it becomes `SIZE_MISMATCH` (the frame is longer than declared); a decoded count ≠ `m_ulDecodedBytes` → `SIZE_MISMATCH`.

### Descriptor validation

`Zenith_ValidateCompressedSectionDesc` applies these rules in order and returns the first failure. `Zenith_DecodeCompressedSectionDesc` returns `TRUNCATED_INPUT` for fewer than 24 bytes, then decodes and validates; `Zenith_EncodeCompressedSectionDesc` validates against the default limits, returns `DESTINATION_TOO_SMALL` for a destination under 24 bytes, then writes.

| # | Rule | Error |
|---|---|---|
| 1 | codec is 0, 1 or 2, or in 128–254 with `m_bAllowExperimentalCodecs` | `UNKNOWN_CODEC` |
| 2 | no flag bit outside `uCOMPRESSED_SECTION_FLAGS_DEFINED` | `CORRUPT_INPUT` |
| 3 | the dictionary flag is set exactly when the dictionary ID is non-zero | `CORRUPT_INPUT` |
| 4 | a non-zero dictionary ID lies in [32,768, 2,147,483,647] | `CORRUPT_INPUT` |
| 5 | the dictionary flag only with codec 2 (or an experimental codec) | `CORRUPT_INPUT` |
| 6 | codec 0: level 0, flags 0, stored bytes = decoded bytes | `CORRUPT_INPUT` |
| 7 | codec 1: level 1–9; codec 2: level 1–19 | `CORRUPT_INPUT` |
| 8 | codecs 1 and 2: stored bytes ≥ 1 | `CORRUPT_INPUT` |
| 9 | stored bytes ≤ `min(m_ulMaxStoredBytes, 4 GiB)` | `LIMIT_EXCEEDED` |
| 10 | decoded bytes ≤ `min(m_ulMaxDecodedBytes, 4 GiB)` | `LIMIT_EXCEEDED` |

### Dictionary creation and training

`CreateDictionary(xBytes, iLevel)` on zstd:

1. Resolve the level. Fewer than 8 bytes, or a magic other than `0xEC30A437` → `INVALID_DICTIONARY`; more than 1 MiB → `LIMIT_EXCEEDED`.
2. `ZDICT_getDictID` outside [32,768, 2,147,483,647] → `INVALID_DICTIONARY`.
3. `Zenith_TryNew` the dictionary object (fault point `uFAULT_POINT_COMPRESSION_DICTIONARY_CREATE`) with its own heap-mode allocator; its constructor calls `m_xCodec.AdjustLiveObjects(+1)` first and its destructor calls `AdjustLiveObjects(−1)` last, so a failure in steps 4–5 destroys a counted object and the count balances.
4. `ZSTD_createCDict_advanced(bytes, size, ZSTD_dlm_byCopy, ZSTD_dct_fullDict, ZSTD_getCParams(level, ZSTD_CONTENTSIZE_UNKNOWN, size), customMem)` and `ZSTD_createDDict_advanced(bytes, size, ZSTD_dlm_byCopy, ZSTD_dct_fullDict, customMem)`; a null result is `OUT_OF_MEMORY` when the allocator's failed-allocation count rose during the call and `INVALID_DICTIONARY` otherwise.
5. `ZSTD_getDictID_fromDDict` must equal step 2's ID, else `INVALID_DICTIONARY`.

On zlib, `CreateDictionary` and `TrainDictionary` return `UNSUPPORTED` without allocating.

`TrainDictionary(axSamples, xDestination, xDesc, xScratch)` on zstd:

1. Resolve the level; `m_ulDictionaryBytes` outside [256, 1 MiB] → `INVALID_DESC`; `xDestination.GetCount() < m_ulDictionaryBytes` → `DESTINATION_TOO_SMALL`.
2. Fewer than 16 samples → `INVALID_DESC`; more than 1,048,576, or more than 256 MiB in total (overflow-checked) → `LIMIT_EXCEEDED`.
3. Copy the samples, in the given order, into one scratch buffer and their sizes into a scratch `size_t` array (both from `xScratch`).
4. `ZDICT_optimizeTrainFromBuffer_fastCover` with the pinned parameters: `k = 0`, `d = 0` (search), `f = 20`, `steps = 40`, `nbThreads = 1`, `splitPoint = 0.75`, `accel = 1`, `shrinkDict = 0`, `shrinkDictMaxRegression = 0`, `zParams.compressionLevel = level`, `zParams.notificationLevel = 0`, `zParams.dictID = 0`.
5. `ZDICT_isError`: `srcSize_wrong` → `INVALID_DESC` (the samples cannot train a dictionary); `memory_allocation` → `OUT_OF_MEMORY`; others per "Error mapping for zstd". Return the dictionary size.

Training is deterministic: the same samples in the same order with the same capacity and level give the same bytes and the same ID. The fast-cover API sits in `zdict.h`'s `ZDICT_STATIC_LINKING_ONLY` block, which zstd documents as unstable across versions, so a zstd pin bump may need a change to `Zenith_CompressionZstdTraining.cpp` as well as a re-record. Sample order is an input, so the cook passes samples sorted by their assets' `Zenith_PersistentID`. `ZDICT_*` allocates its working memory with the C runtime allocator, because zstd's dictionary builder has no `ZSTD_customMem` hook; that is admitted by [Z2-N-CONTRACT-240] ("where the library offers a hook") and is why training allocations are not fault-injectable (the conformance case covers the scratch-arena side only).

### Scratch and memory bounds

| Bound | zlib | zstd |
|---|---|---|
| `GetCompressBound(n)` | `n + (n >> 12) + (n >> 14) + (n >> 25) + 13` | `ZSTD_compressBound(n)`; `ZSTD_isError` → `SIZE_OVERFLOW` |
| engine library state, compress | 270,336 | `ZSTD_estimateCStreamSize_usingCParams(ZSTD_getCParams(level, pledged or ZSTD_CONTENTSIZE_UNKNOWN, dictionary size))` |
| engine library state, decompress (streaming) | 49,152 | `ZSTD_estimateDStreamSize(1 << 23)` |
| `GetCompressScratchBound(n, level, dict)` | 270,336 + 131,072 + 4,096 | the compress state for `n` + 131,072 + 4,096 |
| `GetDecompressScratchBound(n, dict)` | 49,152 + 4,096 | `ZSTD_estimateDCtxSize()` + 4,096 |
| `GetContextMemoryBound(desc)` | `sizeof` context + engine state + `sizeof` engine + 262,144 (compress) + 4,096 | as zlib, with zstd's state |

Every bound is computed in `uint64` with overflow checks and returns `SIZE_OVERFLOW` on overflow. The 4,096-byte slack covers the 16-byte alignment padding of each arena allocation (zlib makes 5 allocations, zstd at most 12), the engine object, and the usable-size rounding of the context, staging and engine objects and of each library block. In the `GetContextMemoryBound` row, engine state + `sizeof` engine is exactly what the backend's `DoGetEngineMemoryBound` returns (the backend owns the `sizeof` of its engine object); the facade adds `sizeof` context, the staging and the slack. The usable-size term of the slack assumes the system backing's reporting (usable size = request within alignment padding); see "Open decisions", D-SEL-HEAP. Bounds are upper bounds: the conformance case `ScratchBoundIsSufficient` compresses and decompresses the sweep set and 3 MiB of `mixed` at the minimum, default and maximum levels (every level in `ScratchBoundIsSufficient_AllLevels`) with an arena whose cap is exactly the bound, and `ContextMemoryBoundHolds` checks `GetMemoryBytes() ≤ GetContextMemoryBound()` after every call of a full round trip.

### Error mapping for zlib

`Zenith_MapZlibError(int iResult, Zenith_ZlibCall eCall, uint64 ulInputLeft, uint64 ulOutputLeft)` in `ZenithCompression_Zlib/Internal/Zenith_CompressionZlibErrors.cpp`. `eCall` selects the `Z_BUF_ERROR` row: a streaming step (`COMPRESSION_ZLIB_CALL_DEFLATE`, `COMPRESSION_ZLIB_CALL_INFLATE_STREAM`) never maps `Z_BUF_ERROR` to an error — the context's `End` drain produces `TRUNCATED_INPUT` — and the one-shot call (`COMPRESSION_ZLIB_CALL_INFLATE_ONESHOT`) checks input exhaustion before output exhaustion, so a stream that ends early into a full destination is `TRUNCATED_INPUT`:

| zlib result | Meaning in context | `Zenith_CompressionError` |
|---|---|---|
| `Z_OK` (0), `Z_STREAM_END` (1) | progress; frame end | success |
| `Z_NEED_DICT` (2) | the stream names a preset dictionary | `UNSUPPORTED` |
| `Z_ERRNO` (−1) | never produced by the compiled subset | `INTERNAL` |
| `Z_STREAM_ERROR` (−2) | invalid stream state or parameter: a wrapper defect | `INTERNAL` |
| `Z_DATA_ERROR` (−3) | invalid deflate data, header, or Adler-32 | `CORRUPT_INPUT` |
| `Z_MEM_ERROR` (−4) | an allocation through the thunks failed | `OUT_OF_MEMORY` |
| `Z_BUF_ERROR` (−5), `COMPRESSION_ZLIB_CALL_INFLATE_ONESHOT`, `ulInputLeft == 0` | the stream ended early | `TRUNCATED_INPUT` |
| `Z_BUF_ERROR`, `COMPRESSION_ZLIB_CALL_INFLATE_ONESHOT`, input left, `ulOutputLeft == 0` | the destination is full | `DESTINATION_TOO_SMALL` |
| `Z_BUF_ERROR`, `COMPRESSION_ZLIB_CALL_INFLATE_ONESHOT`, input and output left | impossible for a correct library | `INTERNAL` |
| `Z_BUF_ERROR`, `COMPRESSION_ZLIB_CALL_DEFLATE` or `COMPRESSION_ZLIB_CALL_INFLATE_STREAM` | no progress possible this step (normal) | success with zero progress |
| any result, `COMPRESSION_ZLIB_CALL_DEFLATE_INIT` or `COMPRESSION_ZLIB_CALL_INFLATE_INIT` | `Z_MEM_ERROR` → `OUT_OF_MEMORY`; any other non-`Z_OK` → `INTERNAL` | as stated |
| any non-`Z_OK` result, `COMPRESSION_ZLIB_CALL_DEFLATE_RESET` or `COMPRESSION_ZLIB_CALL_INFLATE_RESET` | an invalid stream | `INTERNAL` |
| `Z_VERSION_ERROR` (−6) | header/library mismatch | `INTERNAL` |
| any other value | — | `INTERNAL` |

Each `INTERNAL` and `CORRUPT_INPUT` is logged with `xStream.msg` when non-null.

### Error mapping for zstd

`Zenith_MapZstdError(size_t ulResult, Zenith_ZstdCall eCall)` in `ZenithCompression_Zstd/Internal/Zenith_CompressionZstdErrors.cpp`, over `ZSTD_getErrorCode(ulResult)`; `eCall` selects the `srcSize_wrong` row (decode calls: `…_DECOMPRESS_STREAM`, `…_DECOMPRESS_ONESHOT`, `…_FRAME_HEADER`, `…_FRAME_SIZE`; the compress call `…_COMPRESS_STREAM`; the training call `…_TRAIN`):

| `ZSTD_ErrorCode` | `Zenith_CompressionError` |
|---|---|
| `ZSTD_error_no_error` | success |
| `ZSTD_error_memory_allocation`, `ZSTD_error_workSpace_tooSmall` | `OUT_OF_MEMORY` |
| `ZSTD_error_dstSize_tooSmall` | `DESTINATION_TOO_SMALL` |
| `ZSTD_error_srcSize_wrong` | decode: `TRUNCATED_INPUT`; compress (pledged size): `SIZE_MISMATCH`; training: `INVALID_DESC` |
| `ZSTD_error_corruption_detected`, `ZSTD_error_checksum_wrong`, `ZSTD_error_literals_headerWrong`, `ZSTD_error_prefix_unknown`, `ZSTD_error_version_unsupported`, `ZSTD_error_frameParameter_unsupported`, `ZSTD_error_tableLog_tooLarge`, `ZSTD_error_maxSymbolValue_tooLarge`, `ZSTD_error_maxSymbolValue_tooSmall` | `CORRUPT_INPUT` |
| `ZSTD_error_frameParameter_windowTooLarge` | `LIMIT_EXCEEDED` |
| `ZSTD_error_dictionary_wrong` | `DICTIONARY_MISMATCH` |
| `ZSTD_error_dictionary_corrupted`, `ZSTD_error_dictionaryCreation_failed` | `INVALID_DICTIONARY` |
| `ZSTD_error_parameter_unsupported`, `ZSTD_error_parameter_combination_unsupported`, `ZSTD_error_parameter_outOfBound`, `ZSTD_error_stage_wrong`, `ZSTD_error_init_missing`, `ZSTD_error_dstBuffer_null`, `ZSTD_error_noForwardProgress_destFull`, `ZSTD_error_noForwardProgress_inputEmpty`, `ZSTD_error_stabilityCondition_notRespected`, `ZSTD_error_GENERIC` | `INTERNAL` |
| any other code | `INTERNAL` |

Each `INTERNAL`, `CORRUPT_INPUT` and `INVALID_DICTIONARY` is logged with `ZSTD_getErrorName(ulResult)`.

### Determinism definition and parameter pinning

**Definition.** For a codec at a resolved level, with an optional dictionary, the compressed bytes are a function of: the input bytes; the flush offsets (streaming only); whether a pledged size was declared (zstd only); and the **library build** — the pinned library version compiled from the manifest's verified source with the wrapper's definitions for one target. Given equal values of those, the bytes are identical for any slicing of input and output across calls, on any thread, in any process, on any repetition, from any codec instance, and whether produced by `Compress`, by `Zenith_CompressSection` or by a context created with the equivalent desc. Decompressed bytes are unique by construction and are always identical.

**Tested beyond the definition.** The golden vectors are asserted on the MSVC x64 and clang x64 builds (`win` presets and `linux-tsan`) and in the S24 device acceptance run; a divergence between targets is a determinism defect handled by a ruling, because the reproducible cook (Windows) and runtime compression (Windows and Android) both rely on it.

**Not promised.** Equal bytes across library versions, across codecs, between a pledged and an unpledged zstd stream, or with any parameter other than those below; and equal bytes between Zenith2's output and the libraries' own one-shot entry points (`compress2`, `ZSTD_compress2`) or the `zstd`/`gzip` command-line tools at a nominally equal level — their call patterns and defaults differ, so a comparison against them is not a determinism test.

zlib parameters, set once in the deflate engine and never changed:

| Parameter | Value |
|---|---|
| call | `deflateInit2(&xStream, level, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY)` |
| never called | `deflateParams`, `deflateTune`, `deflateSetDictionary`, `deflatePrime`, `deflateSetHeader`, `deflateCopy` |
| flush | `Z_NO_FLUSH` for `CONTINUE`, `Z_SYNC_FLUSH` for `FLUSH`, `Z_FINISH` for `END` |
| build | the 8 source files and definitions of "Third-party integration of zlib"; no zlib-ng, Chromium zlib or other fork |

zstd parameters, set in the compress engine in this order after `ZSTD_createCCtx_advanced(customMem)`:

| Parameter | Value | Why |
|---|---|---|
| `ZSTD_c_compressionLevel` | the resolved level | |
| `ZSTD_c_checksumFlag` | 1 | integrity (C5) |
| `ZSTD_c_contentSizeFlag` | 1 | size validation on decode |
| `ZSTD_c_dictIDFlag` | 1 | a frame names its dictionary |
| `ZSTD_c_nbWorkers` | 0 | single-threaded; the library is built without `ZSTD_MULTITHREAD` |
| `ZSTD_c_enableLongDistanceMatching` | `ZSTD_ps_disable` | explicit, never auto |
| `ZSTD_c_useRowMatchFinder` | `ZSTD_ps_enable` | explicit instead of `auto`, whose resolution depends on whether the build target has 128-bit SIMD; takes effect for the greedy, lazy and lazy2 strategies and is ignored by the others |
| `ZSTD_c_format` | `ZSTD_f_zstd1` | magic always present |
| pledged size | `ZSTD_CCtx_setPledgedSrcSize` when declared | frame content size |
| dictionary | `ZSTD_CCtx_refCDict` when given, after the parameters | |
| left at their level-derived values | window, hash, chain and search logs, minimum match, target length, strategy, block splitting, dictionary attach mode | each is a deterministic function of the level, the pledged size and the dictionary size |

A pin change (library version or any parameter above) changes compressed bytes, so it re-records the golden vectors and bumps the engine cook version in the same diff (see "Re-cook and migration").


### Library call enumerations and allocation thunks

Declared in `ZenithCompression_Zlib/Internal/Zenith_CompressionZlibErrors.h` and `ZenithCompression_Zstd/Internal/Zenith_CompressionZstdErrors.h`, after `Zenith_ZlibInclude.h` / `Zenith_ZstdInclude.h`:

```cpp
enum Zenith_ZlibCall : uint8
{
	COMPRESSION_ZLIB_CALL_DEFLATE_INIT = 1,   // deflateInit2
	COMPRESSION_ZLIB_CALL_DEFLATE,            // deflate in a streaming step
	COMPRESSION_ZLIB_CALL_DEFLATE_RESET,      // deflateReset
	COMPRESSION_ZLIB_CALL_INFLATE_INIT,       // inflateInit2
	COMPRESSION_ZLIB_CALL_INFLATE_STREAM,     // inflate in a streaming step
	COMPRESSION_ZLIB_CALL_INFLATE_ONESHOT,    // inflate in DoDecompress
	COMPRESSION_ZLIB_CALL_INFLATE_RESET,      // inflateReset
};

enum Zenith_ZstdCall : uint8
{
	COMPRESSION_ZSTD_CALL_CCTX_PARAMETER = 1,  // ZSTD_CCtx_setParameter, setPledgedSrcSize, refCDict
	COMPRESSION_ZSTD_CALL_COMPRESS_STREAM,     // ZSTD_compressStream2
	COMPRESSION_ZSTD_CALL_CCTX_RESET,          // ZSTD_CCtx_reset
	COMPRESSION_ZSTD_CALL_DCTX_PARAMETER,      // ZSTD_DCtx_setParameter, refDDict
	COMPRESSION_ZSTD_CALL_DECOMPRESS_STREAM,   // ZSTD_decompressStream
	COMPRESSION_ZSTD_CALL_DECOMPRESS_ONESHOT,  // ZSTD_decompressDCtx
	COMPRESSION_ZSTD_CALL_DCTX_RESET,          // ZSTD_DCtx_reset
	COMPRESSION_ZSTD_CALL_FRAME_HEADER,        // ZSTD_getFrameHeader_advanced
	COMPRESSION_ZSTD_CALL_FRAME_SIZE,          // ZSTD_findFrameCompressedSize
	COMPRESSION_ZSTD_CALL_DICTIONARY,          // CDict and DDict creation
	COMPRESSION_ZSTD_CALL_TRAIN,               // ZDICT_optimizeTrainFromBuffer_fastCover
};

// zlib: alloc_func / free_func, with opaque = the Zenith_CompressionAllocator*.
voidpf Zenith_ZlibAllocThunk(voidpf pOpaque, uInt uItems, uInt uSize);
	// uint64 product of uItems and uSize (cannot overflow: both < 2^32); returns
	// static_cast<Zenith_CompressionAllocator*>(pOpaque)->TryAllocate(product), or Z_NULL.
void Zenith_ZlibFreeThunk(voidpf pOpaque, voidpf pAddress);
	// static_cast<Zenith_CompressionAllocator*>(pOpaque)->Free(pAddress).

// zstd: ZSTD_allocFunction / ZSTD_freeFunction, with opaque = the Zenith_CompressionAllocator*.
void* Zenith_ZstdAllocThunk(void* pOpaque, size_t ulSize);
void Zenith_ZstdFreeThunk(void* pOpaque, void* pAddress);
ZSTD_customMem Zenith_MakeZstdCustomMem(Zenith_CompressionAllocator& xAllocator);
	// { &Zenith_ZstdAllocThunk, &Zenith_ZstdFreeThunk, &xAllocator }
```

A zlib stream is bound to its allocator by setting `xStream.zalloc = &Zenith_ZlibAllocThunk`, `xStream.zfree = &Zenith_ZlibFreeThunk` and `xStream.opaque = &xAllocator` before `deflateInit2`/`inflateInit2`. The allocator outlives the stream: a context owns both (the allocator is a member declared before the engine, so it is destroyed after it), and a one-shot call keeps its allocator on the stack for the call. Each thunk is a C++ function with exactly the library's C function-pointer signature. Before creating a stream the engine consults `uFAULT_POINT_COMPRESSION_LIBRARY_INIT`; a failure injected there returns `OUT_OF_MEMORY` without calling the library.

### Zlib engine step

`Zenith_CompressionZlibDeflateEngine` (compression) and `Zenith_CompressionZlibInflateEngine` (decompression), each holding one `z_stream m_xStream`, `const Zenith_CompressionZlib& m_xCodec`, its `Zenith_CompressionEngineDesc` and `bool m_bInitialised`. Each constructor calls `m_xCodec.AdjustLiveObjects(+1)` first and each destructor calls `m_xCodec.AdjustLiveObjects(−1)` last (PublicAPI.md "Zenith_Compression.h", "Live-object mechanism"); the creation steps below run in `DoCreateEngine`/`DoConstructEngine` after the object exists.

Deflate, creation: bind the thunks; `deflateInit2(&m_xStream, level, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY)`; a non-`Z_OK` result maps with `COMPRESSION_ZLIB_CALL_DEFLATE_INIT` and the engine object is destroyed before the error is returned — heap path (`DoCreateEngine`): the `Zenith_Owned` returned by `Zenith_TryNew` is released; arena path (`DoConstructEngine`): the function calls `Zenith_Destroy(xArena, pxEngine)` itself, because its caller never receives the pointer; `Z_OK` sets `m_bInitialised`.

Deflate, `Step(xIn, xOut, eDirective)`:

1. `ZENITH_ASSERT(xIn.GetCount() ≤ ulCOMPRESSION_STREAM_CHUNK_BYTES)`; `FLUSH` and `END` assert empty input.
2. `next_in = xIn.GetData()`, `avail_in = Zenith_NarrowCast<uInt>(xIn.GetCount())`; `next_out = xOut.GetData()`, `avail_out = Zenith_NarrowCast<uInt>(min(xOut.GetCount(), 2^30))`.
3. `iFlush` = `Z_NO_FLUSH` for `CONTINUE`, `Z_SYNC_FLUSH` for `FLUSH`, `Z_FINISH` for `END`; `iResult = deflate(&m_xStream, iFlush)`.
4. `consumed = xIn.GetCount() − avail_in`; `produced = offered avail_out − avail_out`.
5. `Z_OK`, `Z_STREAM_END` and `Z_BUF_ERROR` (no progress possible, for example a repeated flush with nothing pending) are successes; any other result maps with `COMPRESSION_ZLIB_CALL_DEFLATE` (`Z_STREAM_ERROR` → `INTERNAL`).
6. `m_bDirectiveComplete`: `CONTINUE` ⇔ `avail_in == 0`; `FLUSH` ⇔ `avail_out != 0` after the call (zlib's rule: a `Z_SYNC_FLUSH` that returns with output space left has emitted everything); `END` ⇔ `iResult == Z_STREAM_END`.

Deflate, `Reset()`: `deflateReset`; non-`Z_OK` → `INTERNAL` (`COMPRESSION_ZLIB_CALL_DEFLATE_RESET`). Destructor: `deflateEnd` when `m_bInitialised`, then `m_xCodec.AdjustLiveObjects(−1)`. `GetMemoryBytes()`: `sizeof(*this)`; the library's blocks are counted by the allocator, which the context adds.

Inflate, creation: bind the thunks; `inflateInit2(&m_xStream, 15)` (15 accepts only the zlib wrapper, so gzip and raw deflate fail as `Z_DATA_ERROR`); non-`Z_OK` maps with `COMPRESSION_ZLIB_CALL_INFLATE_INIT` and the engine object is destroyed on the same two paths as deflate; `Z_OK` sets `m_bInitialised`.

Inflate, `Step(xIn, xOut, CONTINUE)` (the only directive a decompression engine accepts; others `ZENITH_ASSERT`):

1. Set the four stream fields as in deflate step 2.
2. `iResult = inflate(&m_xStream, Z_NO_FLUSH)`.
3. Compute consumed and produced as in deflate step 4.
4. `Z_STREAM_END`: success, `m_bDirectiveComplete = true` (the frame's end, after the Adler-32 check); bytes of `xIn` not consumed stay unconsumed, and the context turns a non-zero remainder into `TRAILING_DATA`. `Z_OK`: success, not complete. `Z_BUF_ERROR`: success with zero progress, not complete. `Z_NEED_DICT` → `UNSUPPORTED`; `Z_DATA_ERROR` → `CORRUPT_INPUT` (a bad header, bitstream or Adler-32); `Z_MEM_ERROR` → `OUT_OF_MEMORY` (the lazily allocated 32 KiB window); others → `INTERNAL`; all with `COMPRESSION_ZLIB_CALL_INFLATE_STREAM`.

Inflate, `Reset()`: `inflateReset` (`COMPRESSION_ZLIB_CALL_INFLATE_RESET`). Destructor: `inflateEnd` when `m_bInitialised`, then `m_xCodec.AdjustLiveObjects(−1)`.

### Zstd engine step

`Zenith_CompressionZstdCompressEngine` holds `ZSTD_CCtx* m_pxCCtx`; `Zenith_CompressionZstdDecompressEngine` holds `ZSTD_DCtx* m_pxDCtx` and the header probe `uint8 m_auHeaderProbe[18]` (`ZSTD_FRAMEHEADERSIZE_MAX`), `uint32 m_uHeaderProbeFill` and `bool m_bHeaderChecked`. Both hold `const Zenith_CompressionZstd& m_xCodec`; each constructor calls `m_xCodec.AdjustLiveObjects(+1)` first and each destructor calls `m_xCodec.AdjustLiveObjects(−1)` last, and the creation steps run in `DoCreateEngine`/`DoConstructEngine` after the object exists. A failed step destroys the object before the error is returned, which balances the count — heap path (`DoCreateEngine`): the `Zenith_Owned` returned by `Zenith_TryNew` is released; arena path (`DoConstructEngine`): the function calls `Zenith_Destroy(xArena, pxEngine)` itself, because its caller never receives the pointer.

Compress, creation:

1. `m_pxCCtx = ZSTD_createCCtx_advanced(Zenith_MakeZstdCustomMem(xAllocator))`; null → `OUT_OF_MEMORY`.
2. Set the parameters of the zstd table of "Determinism definition and parameter pinning" in its order; `ZSTD_CCtx_setPledgedSrcSize(pxCCtx, pledged or ZSTD_CONTENTSIZE_UNKNOWN)`; with a dictionary `ZSTD_CCtx_refCDict`. An error maps with `COMPRESSION_ZSTD_CALL_CCTX_PARAMETER`.
3. Invariant 4: `ZSTD_compressStream2(pxCCtx, &{nullptr, 0, 0}, &{nullptr, 0, 0}, ZSTD_e_continue)` — output buffer `{dst, size, pos}` and input buffer `{src, size, pos}` both empty — which initialises the session and allocates the workspace; an error maps with `COMPRESSION_ZSTD_CALL_COMPRESS_STREAM` (`memory_allocation` → `OUT_OF_MEMORY`).

Compress, `Step(xIn, xOut, eDirective)`:

1. The asserts of deflate step 1.
2. `ZSTD_inBuffer xInBuf{ xIn.GetData(), xIn.GetCount(), 0 }`; `ZSTD_outBuffer xOutBuf{ xOut.GetData(), xOut.GetCount(), 0 }`; `eEnd` = `ZSTD_e_continue`, `ZSTD_e_flush` or `ZSTD_e_end`.
3. `ulRemaining = ZSTD_compressStream2(pxCCtx, &xOutBuf, &xInBuf, eEnd)`; `ZSTD_isError` → map with `COMPRESSION_ZSTD_CALL_COMPRESS_STREAM`.
4. `consumed = xInBuf.pos`; `produced = xOutBuf.pos`.
5. `m_bDirectiveComplete`: `CONTINUE` ⇔ `xInBuf.pos == xInBuf.size`; `FLUSH` and `END` ⇔ `ulRemaining == 0` (nothing left to flush; for `END`, the frame and its checksum are written).

Compress, `Reset()`: `ZSTD_CCtx_reset(pxCCtx, ZSTD_reset_session_only)` (parameters and the referenced CDict are kept), re-apply `ZSTD_CCtx_setPledgedSrcSize`, then the invariant-4 empty call; any error → `INTERNAL` (`COMPRESSION_ZSTD_CALL_CCTX_RESET`). Destructor: `ZSTD_freeCCtx(m_pxCCtx)` (null-safe), then `m_xCodec.AdjustLiveObjects(−1)`.

Decompress, creation: `m_pxDCtx = ZSTD_createDCtx_advanced(customMem)` (null → `OUT_OF_MEMORY`); `ZSTD_DCtx_setParameter(ZSTD_d_windowLogMax, 23)`, `ZSTD_d_format = ZSTD_f_zstd1`; with a dictionary `ZSTD_DCtx_refDDict`; errors map with `COMPRESSION_ZSTD_CALL_DCTX_PARAMETER`. The window buffer is allocated by zstd on the first step that decodes the frame header.

Decompress, `Step(xIn, xOut, CONTINUE)`:

1. Header probe, while `m_bHeaderChecked` is false: before calling zstd, copy `min(18 − m_uHeaderProbeFill, xIn.GetCount())` bytes from the start of `xIn` into `m_auHeaderProbe` (a copy only; the same bytes also go to zstd in step 2) and call `ZSTD_getFrameHeader_advanced(&xHeader, m_auHeaderProbe, m_uHeaderProbeFill, ZSTD_f_zstd1)`. A positive result (more bytes needed) leaves the check for the next step. Otherwise apply, in order, the reader rules of "zstd frame format as written" that the streaming path owns: an error maps with `COMPRESSION_ZSTD_CALL_FRAME_HEADER`; a skippable frame → `CORRUPT_INPUT`; `windowSize > 2^23` → `LIMIT_EXCEEDED`; the frame's `dictID` and the supplied dictionary disagree → `DICTIONARY_MISMATCH`; then `m_bHeaderChecked = true`. These are what zstd itself would not refuse (it decodes a frame without a dictionary ID using a referenced dictionary). Invariant: a positive result means every byte of this step's input was header (the input was shorter than the header's remainder), and zstd's header-loading stage consumes every byte of a partial header whatever the output space, so the probe and the decoder stay aligned and no byte is copied twice.
2. `ulHint = ZSTD_decompressStream(pxDCtx, &xOutBuf, &xInBuf)` with buffers built as in compress step 2; `ZSTD_isError` → map with `COMPRESSION_ZSTD_CALL_DECOMPRESS_STREAM` (`frameParameter_windowTooLarge` → `LIMIT_EXCEEDED`, `dictionary_wrong` → `DICTIONARY_MISMATCH`, `checksum_wrong` → `CORRUPT_INPUT`).
3. `consumed = xInBuf.pos`; `produced = xOutBuf.pos`.
4. `m_bDirectiveComplete` ⇔ `ulHint == 0`: the frame is fully decoded, checksum-verified and flushed. zstd returns at the frame's end without consuming input beyond it, so a remainder of `xIn` reaches the context's `TRAILING_DATA` rule.

Decompress, `Reset()`: `ZSTD_DCtx_reset(pxDCtx, ZSTD_reset_session_only)` (parameters and DDict kept), clear the probe (`COMPRESSION_ZSTD_CALL_DCTX_RESET`). Destructor: `ZSTD_freeDCtx(m_pxDCtx)` (null-safe), then `m_xCodec.AdjustLiveObjects(−1)`.

`GetMemoryBytes()` of both zstd engines is `sizeof(*this)`; `ZSTD_sizeof_CCtx`/`ZSTD_sizeof_DCtx` equal the allocator's live bytes and are not added twice.

### Context desc validation

`Zenith_CompressionContext::Create(xCodec, xDesc)` validates the desc in this order and returns the first failure:

| # | Rule | Error |
|---|---|---|
| 1 | `m_eMode` is `COMPRESSION_MODE_COMPRESS` or `COMPRESSION_MODE_DECOMPRESS`; `m_szName` is not null | `INVALID_DESC` |
| 2 | decompress: `m_iLevel` is 0 and `m_ulPledgedSourceBytes` is unknown | `INVALID_LEVEL`, `INVALID_DESC` |
| 3 | compress: `m_ulExpectedDecodedBytes` and `m_ulMaxDecodedBytes` are unknown | `INVALID_DESC` |
| 4 | decompress: when both are known, `m_ulExpectedDecodedBytes ≤ m_ulMaxDecodedBytes` | `INVALID_DESC` |
| 5 | a dictionary on a codec without dictionaries | `UNSUPPORTED` |
| 6 | a dictionary of another codec | `DICTIONARY_MISMATCH` |
| 7 | compress: the level resolves (0 → the dictionary's level when one is given, else the codec default; otherwise within the codec's range and, with a dictionary, equal to its level) | `INVALID_LEVEL` |

Then it builds the context in these steps; a failing step returns its error and the partially built context's owners release what exists (`Zenith2/Docs/Contracts/Memory.md → "Fallible construction"`):

| Step | Fault point consulted first | Allocation | Error on failure |
|---|---|---|---|
| 1. `Zenith_TryNew<Zenith_CompressionContext>(PrivateTag{}, …)` | `uFAULT_POINT_COMPRESSION_CONTEXT_CREATE` | the context object | `OUT_OF_MEMORY` |
| 2. compress only: `Zenith_TryNew<Zenith_CompressionStaging>()` | `uFAULT_POINT_COMPRESSION_STAGING_CREATE` | 256 KiB staging | `OUT_OF_MEMORY` |
| 3. `DoCreateEngine(xEngineDesc, m_xAllocator)`: the engine object | `uFAULT_POINT_COMPRESSION_ENGINE_CREATE` | the engine object | `OUT_OF_MEMORY` |
| 4. inside step 3: `deflateInit2` / `inflateInit2` / `ZSTD_createCCtx_advanced` / `ZSTD_createDCtx_advanced` and the parameter calls | `uFAULT_POINT_COMPRESSION_LIBRARY_INIT` | library state | `OUT_OF_MEMORY` for a failed allocation, else `INTERNAL` |
| 5. inside step 4, every library allocation | `uFAULT_POINT_COMPRESSION_LIBRARY_ALLOCATE` | per request | as step 4 |
| 6. zstd compress: the invariant-4 empty step | — | the zstd workspace | `OUT_OF_MEMORY` |

zlib allocates its inflate window, and zstd its decode window buffer, lazily on the first step that needs them, so an allocation failure can also surface mid-stream from `Process`; the context then moves to FAILED with `OUT_OF_MEMORY` and its destructor releases everything.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity keyword | Rule |
|---|---|---|
| `Zenith_Compression` (both backends) | `ANY_THREAD` | every public member is `const` and callable concurrently; each call's mutable state lives in the caller's scratch arena or context; the interface has no data member; the backend's final class owns the only mutable state, `mutable std::atomic<uint32> m_uLiveObjects`, adjusted by its Internal-only `AdjustLiveObjects` from the constructors and destructors of its engines and dictionaries (relaxed increments, release decrements, an acquire load in its destructor; `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_Compression.h"`) |
| `Zenith_CompressionContext` | `OWNER_THREAD` | one thread at a time; a hand-off between threads happens at a frame-graph edge, a sync point, or a hand-off the caller synchronises (a semaphore or queue whose release-acquire orders the context's memory) |
| `Zenith_CompressionStreamEngine` | `OWNER_THREAD` | owned by exactly one context or one-shot call, used on its thread |
| `Zenith_CompressionAllocator` | `OWNER_THREAD` | as its owner; arena mode additionally follows the arena's `OWNER_THREAD` rule ([Z2-N-CONTRACT-233]) |
| `Zenith_CompressionDictionary` | `IMMUTABLE` | read-only after creation; zstd's CDict and DDict are documented as shareable read-only by concurrent contexts |
| `Zenith_CompressionCodecSet` | `ANY_THREAD_EXCEPT(Add)` | filled by its owner before it is shared; read-only afterwards |
| descs, infos, progress, section descriptors, enums | `IMMUTABLE` | plain values |
| free functions of `Zenith_CompressedSection.h` and `Zenith_CompressionTypes.h` | `ANY_THREAD` | state only in arguments |

The scratch arena passed to a one-shot call is the calling thread's own arena: parallel callers (cook workers, decode threads) each pass their own, as [Z2-N-CONTRACT-233] requires.

### Frame-graph placement

COMPRESS owns no frame-graph node, declares no ECS or world-resource access and runs in no phase of its own (`Zenith2/Docs/Architecture.md → "Frame phases and authority"`). Its calls run inside the work of their callers:

| Caller | Thread | Typical call |
|---|---|---|
| ZenithRuntime startup step 11 | main thread | the two factories, `Zenith_CompressionCodecSet::Add` |
| ASSET decode (Z2-I-ASSET-16, Z2-I-ASSET-17) | the asset system's decode threads or jobs | `Zenith_DecompressSection` with a per-thread scratch arena sized by `GetDecompressScratchBound` |
| SAVE | its asynchronous save job; replay writing | `Zenith_CompressSection`; a streaming compress context |
| ZenithCook, ZenithEditor | tool worker threads | `Zenith_CompressSection`, `TrainDictionary` |

Every COMPRESS call is CPU-bound, allocates only through its arena or the root heap's `TryAllocate`, and never waits, so it may run on a job worker (`Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` [Z2-N-CONTRACT-315]). A streaming call does at most one engine step, so a caller that time-slices a long stream across jobs or frames gets a bounded slice: one 128 KiB chunk (at zstd level 19 on the reference desktop, tens of milliseconds; the benchmark reports the p99 per-call time).

### Locks and blocking

COMPRESS creates no `Zenith_Mutex`, adds no row to the lock ordering table, calls no blocking primitive and starts no thread. The only lock it can reach is inside `Zenith_Heap` (its OOM lock is never reached, because every COMPRESS allocation uses `TryAllocate`, which never invokes the OOM handler — [Z2-N-CONTRACT-197]). Library allocation failure therefore surfaces as `COMPRESSION_ERROR_OUT_OF_MEMORY`, never as a fatal OOM.

### Lifetime and shutdown order

| Object | Created by | Destroyed by | Must outlive |
|---|---|---|---|
| each codec | the composition root (startup step 11), each tool's main, each test | the same owner, after everything below | every context, engine and dictionary created from it (`ZENITH_CHECK` on `m_uLiveObjects` in the backend final class's destructor) |
| the codec set | the composition root | the composition root, before the codecs | every reader holding a pointer from `Find` |
| a dictionary | its container's loader (PACK), the cook | the same | every context and call using it; its codec outlives it |
| a context | its user | its user | — (it holds a reference to its codec and a pointer to its dictionary) |

In the Player, ZenithRuntime destroys the asset registry and the save system before the codec set and the codecs, which is the reverse of startup step 11 (`Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`, `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` table "Shutdown ownership order"). Destroying a context or a dictionary is always safe on its owner thread; neither has a callback or a deferred release.

### Cancellation

COMPRESS calls are synchronous and have no operation handle. Cancellation of a stream is the owner's decision between calls:

| Point | How | What remains |
|---|---|---|
| between `Process`, `Flush` or `End` calls | destroy the context, or `Reset` it for reuse | nothing: the destructor releases the staging, the engine and every library allocation on the owner's thread; `Reset` keeps allocations for reuse and forgets the frame |
| during one call | not possible; each call is bounded to one engine step | — |
| during a one-shot call | not possible; a caller that must cancel within a large buffer uses a context | — |

Output already delivered is the caller's to discard; it is never a complete frame (no `END` step ran), so any decoder refuses it with `TRUNCATED_INPUT`, and a section descriptor is only written after a successful end. An asynchronous operation built on COMPRESS (an ASSET decode request) reports cancellation through its own operation handle at the boundary between two COMPRESS calls ([Z2-N-CONTRACT-335]).

### Error enumeration

| Enumerator | Returned when |
|---|---|
| `COMPRESSION_ERROR_INVALID_DESC` (1) | a desc field or argument combination breaks "Context desc validation", "Section compression and decompression" (dictionary span rules) or "Dictionary creation and training" (capacity, sample count below 16) |
| `COMPRESSION_ERROR_INVALID_LEVEL` (2) | a level is outside the codec's range, a decompression desc names a level, or a level differs from the dictionary's |
| `COMPRESSION_ERROR_OUT_OF_MEMORY` (3) | a context, staging, engine, dictionary, scratch or library allocation failed (including injected failures) |
| `COMPRESSION_ERROR_DESTINATION_TOO_SMALL` (4) | one-shot output does not fit; a section destination is below the required size; a descriptor destination is under 24 bytes |
| `COMPRESSION_ERROR_CORRUPT_INPUT` (5) | a malformed header, bitstream or checksum; a descriptor breaking rules 2–8 of "Descriptor validation"; a section zstd frame without a checksum |
| `COMPRESSION_ERROR_TRUNCATED_INPUT` (6) | the input ended before the frame did; a descriptor source under 24 bytes |
| `COMPRESSION_ERROR_TRAILING_DATA` (7) | input bytes follow the end of the frame |
| `COMPRESSION_ERROR_SIZE_MISMATCH` (8) | a pledged, expected or declared size differs from the real one |
| `COMPRESSION_ERROR_LIMIT_EXCEEDED` (9) | a zstd window above 8 MiB; a decoded-size cap; a section or dictionary size limit; the training sample limits |
| `COMPRESSION_ERROR_UNKNOWN_CODEC` (10) | a codec ID that is reserved, invalid, experimental where not allowed, or absent from the codec set; an unknown codec name |
| `COMPRESSION_ERROR_UNSUPPORTED` (11) | a dictionary operation on zlib; a zlib stream with FDICT; a `none` setting with a dictionary |
| `COMPRESSION_ERROR_INVALID_DICTIONARY` (12) | dictionary bytes with a bad magic, a raw-content dictionary, an ID outside the range, or bytes zstd cannot load |
| `COMPRESSION_ERROR_DICTIONARY_MISMATCH` (13) | the frame or descriptor names a dictionary that was not supplied, a different one, or a dictionary of another codec |
| `COMPRESSION_ERROR_SIZE_OVERFLOW` (14) | 64-bit size arithmetic would overflow |
| `COMPRESSION_ERROR_INTERNAL` (15) | the library returned a code outside the mapping tables, a linked-library version mismatch, or a stuck engine; always logged |

Errors crossing into a consumer's enumeration are mapped explicitly at that boundary ([Z2-N-CONTRACT-463]): for example ASSET maps every decode failure to its own corrupt-asset error, keeping the COMPRESS enumerator in its log line.

### Programmer errors

These are asserts, because only engine code can cause them; no input bytes can:

| Condition | Macro |
|---|---|
| a span with a null data pointer and a non-zero count | `ZENITH_ASSERT` |
| an engine step with more than 131,072 input bytes, or `FLUSH`/`END` with input | `ZENITH_ASSERT` |
| a context call outside its state machine (the `ZENITH_CHECK` cells of the two state tables) | `ZENITH_CHECK` |
| `GetError` on a context that is not FAILED | `ZENITH_CHECK` |
| destroying a codec with live contexts, engines or dictionaries (the backend destructor's check of `m_uLiveObjects`) | `ZENITH_CHECK` |
| destroying a heap-mode allocator with live bytes | `ZENITH_CHECK` |
| a scratch arena not at its entry marker when a one-shot call returns (a defect in the facade) | `ZENITH_ASSERT` |

Every other failure — every property of input bytes, settings files, descriptors, dictionaries and sizes — is a `Zenith_Result` error and never an assert ([Z2-N-CONTRACT-472]).

### Luau surface

COMPRESS exposes nothing to Luau: no binding table for ZenithCompression is registered in `ZenithRuntime/Internal/ScriptBindings/` (`Zenith2/Docs/Architecture/CompositionRoot.md → "Module interfaces"` [Z2-N-ARCH-861] lists where such bindings would live). Scripts reach compressed data only through the engine systems that decode it. So no COMPRESS condition raises a Luau error, and no script input reaches a COMPRESS assert.

## Diagnostics and profiling

### Counters

Emitted through `Zenith_DispatchCounter` (`Zenith2/Docs/Contracts.md → "No mutable global state"`, `Zenith_ProfilerHooks`), with `<codec>` = `zlib` or `zstd`:

| Counter | Value passed | Emitted |
|---|---|---|
| `compression.<codec>.compress_bytes_in_delta` | source bytes of this call | after each successful `Compress` and each context step that consumed input |
| `compression.<codec>.compress_bytes_out_delta` | compressed bytes delivered | same |
| `compression.<codec>.decompress_bytes_in_delta` | compressed bytes consumed | after each successful `Decompress` and each decompress step |
| `compression.<codec>.decompress_bytes_out_delta` | decoded bytes produced | same |
| `compression.<codec>.errors_delta` | 1 | after each failed public call |
| `compression.<codec>.contexts_live_delta` | +1 on creation, −1 on destruction | `Zenith_CompressionContext::Create` success, and the context's destructor when `m_xEngine` is non-null (a context destroyed inside a failed `Create` never emitted +1) |
| `compression.<codec>.context_memory_bytes_delta` | `GetMemoryBytes() − m_ulLastReportedMemoryBytes`, then `m_ulLastReportedMemoryBytes` is updated (the destructor emits `−m_ulLastReportedMemoryBytes`) | context creation, destruction and every call that changes an allocator's live bytes |

Every COMPRESS counter's `iValue` is a delta, and every COMPRESS counter name ends in `_delta` so the convention is visible in the name: `Zenith_PfnOnCounter` (`Zenith2/Docs/Contracts.md → "No mutable global state"`) carries no semantics marker, and CORE's producers on the same hook publish cumulative values (a queue's `<name>.full` count, an arena's `<name>.used_bytes`, the memory governor's totals). A sink that shows a live total sums a `_delta` counter's values. No COMPRESS component keeps a readable total for the counters, so no shared state beyond the backend's live-object count exists.

<!-- assumption: pending CONTRACT -->
A marker for counter semantics on `Zenith_PfnOnCounter` (a delta-or-cumulative flag or a naming rule stated in Contracts) is requested from CONTRACT and DEVT (cross-area note CM12). Until one exists, the `_delta` suffix is the marker; if CONTRACT adopts a flag, COMPRESS passes it and keeps the names. COMPRESS asks DEVT for a "Compression" counter group showing per-frame decode MB/s per codec, the live context count and the context memory.
<!-- /assumption -->

### Profiler zones

Each public call opens one zone through the CORE scoped-zone helper, with a `constexpr Zenith_ZoneSite` per zone:

| Zone | Scope |
|---|---|
| `Compression/Compress/<codec>` | `Zenith_Compression::Compress` |
| `Compression/Decompress/<codec>` | `Zenith_Compression::Decompress` |
| `Compression/Context/<codec>` | one `Process`, `Flush` or `End` call |
| `Compression/TrainDictionary/zstd` | `TrainDictionary` |
| `Compression/CreateDictionary/zstd` | `CreateDictionary` |

The zones feed Tracy and the on-screen profiler's CPU timeline of whichever node made the call (`Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`).

### Log category and messages

Three categories, declared with `ZENITH_LOG_CATEGORY` (`Zenith2/Docs/Design/CORE/Diagnostics.md → "Levels and categories"`) with compiled minimum `LOG_LEVEL_INFO`: `xLOG_CATEGORY_COMPRESSION_API` (`compression.api`) in `ZenithCompression/Internal/Zenith_CompressionDiagnostics.h`, `xLOG_CATEGORY_COMPRESSION_ZLIB` (`compression.zlib`) in `ZenithCompression_Zlib/Internal/Zenith_CompressionZlibErrors.h` and `xLOG_CATEGORY_COMPRESSION_ZSTD` (`compression.zstd`) in `ZenithCompression_Zstd/Internal/Zenith_CompressionZstdErrors.h`. Each failed public call is logged exactly once, with `ZENITH_LOG_ERROR` (a recoverable failure at its point of failure, [Z2-N-CONTRACT-464]), by the code that detects it: the backend for a library result (it alone holds the library's message), the API library for a validation, size, arena or state failure. The API library never logs an error a `Do*` hook returned. The message is:

```text
<codec> <operation> failed: <COMPRESSION_ERROR_…> (<library message or rule>) name=<context name> in=<bytes> out=<bytes> level=<n> dict=<id>
```

which the default sink prints as `[ERROR] compression.zstd: …` (`Zenith2/Docs/Design/CORE/Diagnostics.md → "Log macros"`).

`<operation>` is `compress`, `decompress`, `context.process`, `context.flush`, `context.end`, `frame-info`, `create-dictionary`, `train-dictionary`, `section.encode`, `section.decode`, `section.compress` or `section.decompress`. No COMPRESS log call allocates from the caller's arena, and none runs on a success path.

### On-screen profiler and frame dumps

COMPRESS contributes counters and zones only: it has no debug view, no frame-dump entry (it owns no node), and no telemetry stream of its own. Its measured evidence is the benchmark report (see "Benchmarks").

## Authoring and cooking

### Editor surfaces

COMPRESS has no editor panel of its own. The importer-settings inspector (owned by EDIT and ASSET) shows the "Compression setting fragment" as a codec dropdown fed by the three names, a level field whose range comes from `Zenith_CompressionCodecInfo`, and a dictionary picker enabled only when `m_bSupportsDictionaries`; it validates with `Zenith_ValidateCompressionSetting` before saving.

### Cook integration

| Step | Owner | COMPRESS call |
|---|---|---|
| create both codecs at tool start; fill a codec set | ZenithCook's main (its own composition root) | the two factories; `Add` |
| read each asset's compression setting from its importer `.meta` | ASSET | `Zenith_ParseCompressionCodecName`, `Zenith_ValidateCompressionSetting` |
| compress each cooked section | ASSET, PACK | `Zenith_CompressSection` with a per-worker scratch arena of `GetCompressScratchBound` bytes |
| write the descriptor into the cooked header or archive table | ASSET, PACK | `Zenith_EncodeCompressedSectionDesc` |
| key the derived-data cache of every compressed output on codec, level, dictionary ID and `m_szLibraryVersion` | ASSET | `GetInfo()` |

<!-- informative -->
COMPRESS's recommended defaults, for PACK and ASSET to adopt or override in their own tables: shipping archives zstd 19; development cooks zstd 3 (fast iteration, same format); saves and replays zstd 3; formats whose readers require deflate (for example interchange exports) zlib 6; already-compressed payloads (audio, video, block-compressed textures that do not shrink) `none`, which `m_bStoreIfNotSmaller` selects automatically.
<!-- /informative -->

### Dictionary training in the cook

The cook step that trains dictionaries (owned by PACK) runs `TrainDictionary` per asset class over that class's cooked sections, passing the samples sorted by their assets' `Zenith_PersistentID`, the class's zstd level and the default 112,640-byte capacity; it stores the result as a `NONE` section and passes the created dictionary to `Zenith_CompressSection` for the class's entries. Because training is deterministic ("Dictionary creation and training"), a clean cook and an incremental cook produce identical dictionaries and identical archives.

### Re-cook and migration

| Change | Consequence | Mechanism |
|---|---|---|
| a zlib or zstd version pin, or any parameter of "Determinism definition and parameter pinning" | compressed bytes change; old data still decodes | one diff: the manifest entry (version, URL, SHA-256, licence hash), the `static_assert` pins in `Zenith_ZlibInclude.h` / `Zenith_ZstdInclude.h`, the re-recorded golden vectors, the benchmark re-run, and the engine cook version bump that forces a re-cook ([Z2-N-CONTRACT-529], [Z2-N-ARCH-126]) |
| a new codec | new codec ID, backend and adapter | a `Z2-D-` decision and the registry row |
| a descriptor layout change | every container format version bumps | re-cook; never a runtime migration |
| a codec-name or setting-rule change | none permitted: the names and rules are stable | — |

Cooked data is never migrated at runtime (Z2-D-048). The compression setting fragment is a field of ASSET's and PACK's authoring documents; its schema version and migration steps are theirs, and COMPRESS never requires one because its value rules only ever widen.

## Failure handling

### Failure table

| Failure | Detected by | Behaviour | Leaves |
|---|---|---|---|
| corrupt compressed bytes | zlib `Z_DATA_ERROR`, zstd corruption and checksum errors, header rules | `CORRUPT_INPUT`; logged once; the context is FAILED | no bytes written past the destination; every allocation released or reusable |
| truncated bytes | zlib `Z_BUF_ERROR` with no input, zstd `srcSize_wrong`, the decompress `End` drain | `TRUNCATED_INPUT` | as above |
| trailing bytes after a frame | the frame-end position | `TRAILING_DATA` | as above |
| declared size wrong (descriptor, pledged, expected) | size comparisons at the points of "Section compression and decompression" and the state tables | `SIZE_MISMATCH` | as above |
| decompression bomb (decoded far larger than declared) | the destination cap (one-shot) and the `cap + 1` narrowing (streaming) | `SIZE_MISMATCH` or `LIMIT_EXCEEDED` before any byte beyond the cap is kept | as above |
| zstd window too large | the frame header | `LIMIT_EXCEEDED` before any allocation for the window | as above |
| unknown or reserved codec in data | descriptor rule 1, `Find` | `UNKNOWN_CODEC`; the consumer refuses the asset (ASSET) | nothing allocated |
| missing or wrong dictionary | frame and descriptor dictionary IDs | `DICTIONARY_MISMATCH` | nothing decoded |
| allocation failure at creation | `TryNew`, the allocator adapter, fault points | `OUT_OF_MEMORY`; the factory unwinds exactly what it built | no leak, no half-built object |
| allocation failure mid-stream (zlib inflate window, zstd decode window) | the allocator adapter's null return mapped from the library | `OUT_OF_MEMORY`; the context is FAILED | destroy or `Reset` releases or reuses everything |
| scratch arena too small (one-shot) | arena `TryAllocate` | `OUT_OF_MEMORY` | the arena at its entry marker |
| destination too small (one-shot compress or decompress) | copy bound, zlib `Z_BUF_ERROR`, zstd `dstSize_tooSmall`, content size check | `DESTINATION_TOO_SMALL`; nothing written beyond the destination | the destination's contents are unspecified |
| linked library differs from the pinned version | the factory's runtime version check | `INTERNAL` from the factory; the Player stops at startup step 11 with its startup-failure exit code | no codec |
| cancellation | the owner destroys or resets a context between calls | see "Cancellation" | nothing |
| device loss, memory pressure | not applicable: COMPRESS holds no GPU resource and no cache to shed | — | — |
| budget breach | Z2-B-COMPRESS-001 and Z2-B-COMPRESS-002 (see "Rows pending allocation") | BR-ACCEPT at stage acceptance: a regression BUG on this epic | — |

### Untrusted input rules

Compressed bytes, section descriptors, dictionary bytes, frame headers and compression settings are untrusted ([Z2-N-ARCH-951]):

1. Every size, count and offset is validated against the input length with overflow-checked 64-bit arithmetic before use, and every failure is a `Zenith_Result` error.
2. No decoder writes outside its destination span, whatever the input; the `cap + 1` narrowing and the one-shot destination bounds are the mechanism.
3. Decode memory is bounded before it is allocated: the zlib window is 32 KiB; the zstd window is checked against 8 MiB from the frame header before the decoder allocates it (`ZSTD_d_windowLogMax`).
4. The limits are the constants of `Zenith_CompressionTypes.h` and `Zenith_CompressedSection.h`, each tested at the limit and at limit + 1 ([Z2-N-ARCH-953]).
5. Every decoder has a libFuzzer target (see "Fuzz targets") ([Z2-N-ARCH-952]).

## Test strategy

### Test targets and gate steps

| Target | Gate step that builds and runs it | Presets |
|---|---|---|
| `ZenithCompression_Tests`, `ZenithCompression_Zlib_Tests`, `ZenithCompression_Zstd_Tests` | `Build`, `Test` (base category, every ticket) | `win-debug`, `win-release`; compiled in `android-arm64` |
| `ZenithCompression_Conformance` | `Build`, `Test` | `win-debug`, `win-release`; compiled in `android-arm64` |
| the same four executables | `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan` | `win-asan`, `linux-tsan` (change-triggered through `Zenith2Memory`, and in every stage acceptance) |
| `Zenith_Fuzz_*` (six) | `Sanitizers -Preset win-asan` (60-second smoke each); full duration in stage acceptance | `win-asan` |
| `Zenith_Bench_Compression --smoke` | `Test` | `win-release` |
| the S24 HWASan run of the conformance suite and the golden vectors | stage acceptance, a `needs-human` device step | `android-arm64-hwasan`, `android-arm64` |
| header and contract checks of the eight public headers and two factory headers | `HeaderSelfContained`, `ApiContractCheck` | `win-debug` |
| confinement of zlib and zstd headers, swap contract | `Libgraph` | `win-debug` |
| no `malloc`, no exceptions, no STL in public headers | `ForbiddenCheck` | all |

Every test below follows `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"` (subsections "Test layout", "Test naming", "Death and sanitizer-defect tests"): the test ID `Test_<Type>_<Behaviour>` maps to the doctest name `"<Library>/<Type>/<Behaviour>"`, each case is its own CTest entry, and every randomised case prints its seed.

### ZenithCompression unit tests

`Tests/Zenith_FakeCompression.h/.cpp` defines `Zenith_FakeCompression`, a `Zenith_Compression` with codec ID 200, levels 1–3 and dictionaries, whose engine records every `Step` call (input length, input FNV-1a, output capacity, directive) and "compresses" by emitting each consumed byte as-is followed by one marker byte per step, with configurable behaviours: consume at most `k` bytes per step, report the frame end after `n` output bytes, emit more than declared, fail the `m`-th step with a chosen error, never progress. It is registered in codec sets with `m_bAllowExperimentalCodecs`, so no runtime path can see it.

| Test ID | File | Asserts |
|---|---|---|
| `Test_CompressionTypes_ErrorToString_EveryEnumerator` | `Zenith_CompressionTypes_Tests.cpp` | enumerators 1–15 each return their own spelling; no two strings are equal |
| `Test_CompressionTypes_ErrorValues_AreStable` | same | `static_assert`s of every enumerator's value (1 … 15) and of the codec IDs 0, 1, 2 and the range constants |
| `Test_CompressionTypes_ParseCodecName_AcceptsExactNames` | same | `"none"`, `"zlib"`, `"zstd"` parse and round-trip through `Zenith_ToString`; `"ZSTD"`, `"zstd "`, `""`, `"lz4"` return `UNKNOWN_CODEC` |
| `Test_CompressionTypes_ValidateSetting_EveryRow` | same | every row of the table in "Compression setting fragment", including the boundary levels 0, 1, 9, 10, 19, 20, −1 |
| `Test_CompressionTypes_LevelConstants_MatchCodecInfo` | same | both real codecs' `GetInfo()` equals the constants and "Codec metadata table" |
| `Test_CompressionCodecSet_AddFind_ReturnsCodec` | `Zenith_CompressionCodecSet_Tests.cpp` | after adding both codecs, `Find(1)` and `Find(2)` return them; `GetCount() == 2` |
| `Test_CompressionCodecSet_AddDuplicate_IsRejected` | same | a second zstd codec → `INVALID_DESC`; the set is unchanged |
| `Test_CompressionCodecSet_AddNoneReservedInvalid_IsRejected` | same | fakes with IDs 0, 3, 127 and 255 → `INVALID_DESC` |
| `Test_CompressionCodecSet_Experimental_RequiresOptIn` | same | ID 200 → `UNKNOWN_CODEC` without the opt-in, accepted with it |
| `Test_CompressionCodecSet_FindAbsent_ReturnsUnknownCodec` | same | `Find(0)`, `Find(2)` on an empty set, `Find(3)` → `UNKNOWN_CODEC` |
| `Test_CompressedSectionDesc_EncodeDecode_RoundTrip` | `Zenith_CompressedSection_Tests.cpp` | 10,000 seeded valid descriptors round-trip exactly |
| `Test_CompressedSectionDesc_Encode_MatchesWorkedExample` | same | the 24 bytes of the worked example |
| `Test_CompressedSectionDesc_ValidationRules_EachRejects` | same | one case per rule 1–10 of "Descriptor validation", each returning the table's error |
| `Test_CompressedSectionDesc_DecodeShort_IsTruncated` | same | 0 and 23 bytes → `TRUNCATED_INPUT` |
| `Test_CompressedSectionDesc_EncodeShort_IsTooSmall` | same | a 23-byte destination → `DESTINATION_TOO_SMALL`, nothing written |
| `Test_CompressedSectionDesc_SizeLimits_AtAndAboveLimit` | same | 4 GiB accepted, 4 GiB + 1 → `LIMIT_EXCEEDED`, for stored and decoded bytes; a tighter reader limit is applied |
| `Test_StoreSection_CopiesAndDescribesNone` | same | the destination holds the source and the descriptor is `{NONE, 0, 0, 0, n, n}`; a destination one byte short → `DESTINATION_TOO_SMALL` |
| `Test_CompressSection_Incompressible_StoresRaw` | same | 64 KiB of `random` with zstd 19 yields a `NONE` descriptor and the source bytes |
| `Test_CompressSection_StoreDisabled_KeepsCompressed` | same | with `m_bStoreIfNotSmaller = false` the descriptor names zstd |
| `Test_CompressSection_DestinationBelowBound_IsRejected` | same | a destination one byte under the required size → `DESTINATION_TOO_SMALL` |
| `Test_DecompressSection_None_CopiesExactly` | same | the decoded bytes equal the stored bytes |
| `Test_DecompressSection_StoredSizeMismatch_IsRejected` | same | a stored span one byte short → `SIZE_MISMATCH` |
| `Test_DecompressSection_DictionaryMissing_IsMismatch` | same | a dictionary-flagged descriptor with an empty dictionary span → `DICTIONARY_MISMATCH` |
| `Test_DecompressSection_DictionarySpanRules_AreEnforced` | same | 65 dictionaries, or two with one ID → `INVALID_DESC` |
| `Test_DecompressSection_UnknownCodec_IsRejected` | same | a descriptor with codec 2 against a set holding only zlib → `UNKNOWN_CODEC` |
| `Test_DecompressSection_ZstdWithoutChecksum_IsCorrupt` | same | a real zstd frame with its checksum flag cleared and trailer removed → `CORRUPT_INPUT`; the raw `Decompress` of the same bytes succeeds |
| `Test_DecompressSection_ContentSizeDiffers_IsSizeMismatch` | same | a descriptor whose decoded size differs from the frame's content size |
| `Test_CompressionContext_Create_FaultSweep_Unwinds` | `Zenith_CompressionContext_Tests.cpp` | `Zenith_RunFaultSweep` (CORE test support) over `Create` in both modes on both real codecs: every visited fault point and every heap allocation, failed one at a time, gives `OUT_OF_MEMORY` (or `INTERNAL` for `LIBRARY_INIT` non-allocation failures), zero leaked allocations, no live context |
| `Test_CompressionContext_DescValidation_EachRule` | same | one case per rule of "Context desc validation" |
| `Test_CompressionContext_InputSlicing_DoesNotChangeEngineCalls` | same | on the fake, 3 MiB fed in slices of 1, 7, 4,096, 131,071, 131,072, 131,073 and 1 MiB bytes and in seeded random slices records one identical step sequence |
| `Test_CompressionContext_OutputSlicing_DoesNotChangeEngineCalls` | same | output spans of 1, 13, 4,096 and 1 MiB bytes record the same sequence; every compress step's output capacity is 131,072 |
| `Test_CompressionContext_Process_AtMostOneStep` | same | every `Process`, `Flush` and `End` call records zero or one step |
| `Test_CompressionContext_Output_NeverPastSpan` | same | guard bytes after every output span are unchanged |
| `Test_CompressionContext_StateTable_EveryNonCheckCell` | same | every cell of both state tables that is not a `ZENITH_CHECK` gives the tabled transition and result |
| `Test_CompressionContext_Flush_RestartsSegment` | same | after a flush at offset 70,000 the next chunk boundaries are 70,000 + k·131,072 |
| `Test_CompressionContext_PledgedSize_Mismatch_Fails` | same | more input than pledged fails in `Process`; less fails at `End`; both `SIZE_MISMATCH`, no `END` step recorded |
| `Test_CompressionContext_StickyError_RepeatsFirstError` | same | after an injected step failure every call returns the same error |
| `Test_CompressionContext_Reset_FromEveryState_ReturnsReady` | same | `Reset` from each of the six states gives READY with zero totals |
| `Test_CompressionContext_ResetReuse_MatchesFreshContext` | same | with both real codecs, a reset context's output equals a fresh context's |
| `Test_CompressionContext_Destroy_FromEveryState_ReleasesAll` | same | the root heap's live allocation count returns to its value before `Create` |
| `Test_CompressionContext_TotalsPast4GiB_AreExact` | same | 5 GiB through the fake (a 1 MiB buffer reused) gives exact 64-bit totals |
| `Test_CompressionContext_DecodeCap_DetectsOverLongFrame` | same | a fake frame one byte longer than expected → `SIZE_MISMATCH` (expected) and `LIMIT_EXCEEDED` (max), no byte kept past the cap |
| `Test_CompressionContext_DecodeTrailingData_Fails` | same | input after the frame end → `TRAILING_DATA` |
| `Test_CompressionContext_DecodeEndBeforeFrameEnd_IsTruncated` | same | `End` with the frame incomplete → `TRUNCATED_INPUT` |
| `Test_CompressionContext_StuckEngine_IsInternal` | same | the never-progressing fake → `INTERNAL` |
| `Test_CompressionContext_ProcessAfterEnd_Checks` | `Zenith_CompressionContext_DefectTests.cpp` | `z2-death-check`: `Process` in ENDING and in FINISHED |
| `Test_CompressionContext_FlushOnDecompress_Checks` | same | `z2-death-check` |
| `Test_CompressionContext_ProcessDuringFlush_Checks` | same | `z2-death-check` |
| `Test_CompressionContext_GetErrorWhenNotFailed_Checks` | same | `z2-death-check` |
| `Test_Compression_DestroyCodecWithLiveContext_Checks` | same | `z2-death-check`: destroying the real zstd codec while one context lives fails the backend destructor's `ZENITH_CHECK`; a companion default-suite case asserts that the codec destroys cleanly once the context is gone |
| `Test_Compression_NullSpanWithCount_Asserts` | same | `z2-death-assert` |
| `Test_CompressionAllocator_HeapMode_TracksLiveAndHighWater` | `Zenith_CompressionAllocator_Tests.cpp` | live and high-water bytes follow `GetUsableSize` over a scripted sequence |
| `Test_CompressionAllocator_ArenaMode_FreeReleasesNothing` | same | arena used bytes never decrease on `Free` |
| `Test_CompressionAllocator_Alignment_Is16` | same | 1,000 seeded sizes, every block 16-aligned in both modes |
| `Test_CompressionAllocator_FaultPoint_ReturnsNull` | same | the injector failing `LIBRARY_ALLOCATE` gives nullptr and increments the failed count |
| `Test_CompressionAllocator_HeapFailure_ReturnsNull` | same | a heap fault countdown gives nullptr, never the OOM handler |
| `Test_CompressionAllocator_DestroyWithLiveBytes_Checks` | `Zenith_CompressionContext_DefectTests.cpp` | `z2-death-check` |
| `Test_Compression_ResolveLevel_EveryBoundary` | `Zenith_Compression_Tests.cpp` | 0 → default; min, max accepted; min − 1, max + 1 → `INVALID_LEVEL`, on both codecs |
| `Test_Compression_OneShot_RestoresArenaMarker` | same | after a successful and after each failing `Compress` and `Decompress` the arena's used bytes equal their entry value |
| `Test_Compression_OneShot_ArenaTooSmall_IsOutOfMemory` | same | an arena capped one allocation short → `OUT_OF_MEMORY` |
| `Test_Compression_Counters_EmittedPerCall` | `Zenith_Compression_Tests.cpp` | with CORE's `Zenith_CapturingHooks` (`Zenith2/Docs/Design/CORE.md → "ZenithTestSupport target"`) as the hook table, one `Compress`, one `Decompress`, one failed call and a context's life emit exactly the `_delta` counters of "Counters" with the stated values, and the `contexts_live_delta` and `context_memory_bytes_delta` values sum to 0 after the context is destroyed |
| `Test_Compression_Zones_OnePerCall` | same | with `Zenith_CapturingHooks`, the same calls open exactly one zone each, with the names of "Profiler zones" |
| `Test_Compression_FailureLog_OneLinePerFailure` | same | with `Zenith_CapturingHooks` recording log records, one failure of each operation (one backend-detected and one API-detected failure each) records exactly one `LOG_LEVEL_ERROR` record in the category of "Log category and messages", with the stated message, and a success records none |
| `Test_Compression_Example_RoundTrips` | `Tests/Examples/Zenith_Compression_Example.cpp` | the compiled example compresses a text with zstd, decompresses it, and streams it through a context |

### Backend unit tests

| Test ID | Library | Asserts |
|---|---|---|
| `Test_CompressionZlib_ErrorMapping_EveryCode` | zlib | every row of "Error mapping for zlib", including an unknown value |
| `Test_CompressionZlib_HeaderRules_EachRejects` | zlib | CM 7, CINFO 8, a bad FCHECK and FDICT set give the tabled errors; CINFO 5 (a 2 KiB window stream) decodes |
| `Test_CompressionZlib_GzipAndRawDeflate_AreRejected` | zlib | a gzip member and a raw deflate stream → `CORRUPT_INPUT` |
| `Test_CompressionZlib_LinkedVersion_MatchesPin` | zlib | `zlibVersion()` is `"1.3.1"` and equals the `version` of the `zlib` entry of `Zenith2/ThirdParty/manifest.json` (read by the test with the STL) |
| `Test_CompressionZlib_Allocations_RouteThroughAllocator` | zlib | a full round trip's library allocations are all counted by the adapter and the root heap's live bytes return to their baseline |
| `Test_CompressionZlib_FrameInfo_WindowFromCinfo` | zlib | `ReadFrameInfo` gives 32,768 for CINFO 7 and 2 header bytes |
| `Test_CompressionZlib_ZeroLength_IsEightBytes` | zlib | the 8-byte stream `78 9C 03 00 00 00 00 01` at level 6 |
| `Test_CompressionZstd_ErrorMapping_EveryCode` | zstd | every row of "Error mapping for zstd", including an unknown code |
| `Test_CompressionZstd_FrameHeader_ChecksumSizeDictionary` | zstd | one-shot frames carry the checksum and content-size flags; dictionary frames carry the dictionary ID; unpledged streaming frames carry no content size |
| `Test_CompressionZstd_TwoFrames_AreTrailingData` | zstd | two concatenated frames → `TRAILING_DATA` |
| `Test_CompressionZstd_SkippableFrame_IsCorrupt` | zstd | a skippable frame → `CORRUPT_INPUT`, one-shot and streaming (the header probe) |
| `Test_CompressionZstd_LegacyMagic_IsCorrupt` | zstd | magic `0xFD2FB527` → `CORRUPT_INPUT` |
| `Test_CompressionZstd_EveryLevel_WindowAtMost8MiB` | zstd | `ZSTD_getCParams(level, ZSTD_CONTENTSIZE_UNKNOWN, 0).windowLog ≤ 23` for levels 1–19; a pledged 9 MiB `text` frame at level 19 decodes (the Single_Segment case: its window is not above 2^23) |
| `Test_CompressionZstd_WindowTooLarge_IsLimitExceeded` | zstd | a crafted header with a 16 MiB window → `LIMIT_EXCEEDED`, one-shot and streaming, before any window allocation |
| `Test_CompressionZstd_LinkedVersion_MatchesPin` | zstd | `ZSTD_versionString()` is `"1.5.7"` and equals the manifest entry |
| `Test_CompressionZstd_Parameters_ArePinned` | zstd | `ZSTD_CCtx_getParameter` on a created engine returns every value of the zstd parameter table |
| `Test_CompressionZstd_DictionaryID_RangeEnforced` | zstd | dictionaries with IDs 0, 32,767 and 2^31 → `INVALID_DICTIONARY`; 32,768 and 2^31 − 1 accepted |
| `Test_CompressionZstd_RawContentDictionary_IsRejected` | zstd | bytes without the dictionary magic → `INVALID_DICTIONARY` |
| `Test_CompressionZstd_Allocations_RouteThroughAllocator` | zstd | as the zlib case, for compress, decompress, CDict and DDict |

### Conformance corpus

`Tests/Conformance/Zenith_CompressionConformanceCorpus.cpp` generates every input from `(class, seed, bytes)` with platform-independent integer arithmetic (splitmix64 seeding an xorshift64* stream), so the same triple gives the same bytes on every host:

| Class | Content |
|---|---|
| `empty`, `one` | 0 bytes; 1 byte `0x5A` |
| `zeros` | all zero |
| `random` | uniform bytes (incompressible) |
| `text` | words drawn from a fixed embedded 512-word English list with a Zipf(1.1) rank distribution, separated by spaces and newlines |
| `json` | objects with keys from a fixed 64-key list and integer, float and string values, one object per line, as authoring documents look |
| `json-small` | 10,000 separate `json` records of 64 B to 16 KiB (log-uniform sizes), the dictionary class |
| `floats` | little-endian `float` triples of a smooth random walk (vertex-like positions) |
| `runs` | runs of 1–64 repeats of random byte strings of 1–32 bytes |
| `far-repeat` | a 1 MiB random block repeated at a 4 MiB distance (inside the zstd window) and at a 12 MiB distance (outside it) |
| `mixed` | the concatenation `text`, `floats`, `random`, `json`, `zeros`, with sizes crossing chunk boundaries |
| `chunk-plus-one` | 131,073 bytes of `text` |

`Zenith_CompressionConformanceCorpusStream` is the block-resumable form used by the large-stream case: block *i* (64 KiB) of class `runs` is generated from seed `Zenith_CompressionTestMix64(seed, i)` (splitmix64 of the pair, defined in `Zenith_CompressionConformance.h`), so any block can be produced on demand and the whole stream never exists in memory.

### Conformance cases

Each case is written once with `ZENITH_COMPRESSION_CONFORMANCE_CASE("<Behaviour>")`, which expands to one doctest case per backend named `"ZenithCompression/Conformance/<Behaviour>[Zlib]"` and `"…[Zstd]"`, each obtaining its codec from the backend's adapter getter, declared in `Zenith_CompressionConformance.h` and defined in each backend's `Tests/ConformanceAdapter/`:

```cpp
Zenith_Result<Zenith_Owned<Zenith_Compression>, Zenith_CompressionError> Zenith_GetCompressionConformanceBackend_Zlib(
	const Zenith_CompressionDesc& xDesc);   // returns Zenith_CreateCompression_Zlib(xDesc)
Zenith_Result<Zenith_Owned<Zenith_Compression>, Zenith_CompressionError> Zenith_GetCompressionConformanceBackend_Zstd(
	const Zenith_CompressionDesc& xDesc);   // returns Zenith_CreateCompression_Zstd(xDesc)
```

The macro's per-backend body receives the created codec as `const Zenith_Compression& xCodec` and the backend name as `const char* szBackend`; a case needing a fault injector passes it in the desc. Test IDs are `Test_CompressionConformance_<Behaviour>`.

| Behaviour | File | Asserts, per backend |
|---|---|---|
| `RoundTrip_CorpusThreeLevels` | `_RoundTrip.cpp` | at the minimum, default and maximum levels (the every-level form is `RoundTrip_CorpusEveryLevel_AllLevels`): the sweep set (every corpus class except `far-repeat` and `json-small`, each class sliced to 64 KiB; `empty`, `one` and `chunk-plus-one` at their own sizes; 7 × 64 KiB + 131,073 + 1 bytes, about 576 KiB; every class generated with seed 1, which the case prints) round-trips one-shot, section and streaming, and 3 MiB of `mixed` round-trips one-shot; at the maximum level that is about 4.9 MiB of input |
| `RoundTrip_CorpusEveryLevel_AllLevels` | same | `z2-stress`: every level of the codec, the sweep set round-trips one-shot, section and streaming (about 1.9 MiB of input per level, below the 8 MiB slice cap for levels ≥ 15) |
| `RoundTrip_ZeroLength` | same | empty input gives a valid frame through every path; decoding yields 0 bytes |
| `RoundTrip_ChunkBoundaries` | same | sizes 1, 131,071, 131,072, 131,073, 262,144 and 262,145 round-trip |
| `CompressBound_Holds` | same | `random` at 1, 2, 3, 4,095, 4,096, 65,536, 131,071, 131,072 and 131,073 bytes at every level, and at 1 MiB and 3 MiB at the minimum, default and maximum levels (at most 4.4 MiB per level): compressed size ≤ `GetCompressBound` |
| `FrameInfo_ReportsHeader` | same | window, header size, checksum flag and (zstd) content size and dictionary ID |
| `Levels_OutOfRange_AreRejected` | same | minimum − 1 and maximum + 1 → `INVALID_LEVEL` in `Compress`, `Create` and `Zenith_CompressSection` |
| `Metadata_MatchesTable` | same | `GetInfo()` equals its "Codec metadata table" column |
| `OneShotEqualsStreamingWithPledgedSize` | `_Streaming.cpp` | at the default and maximum levels the sweep set's one-shot bytes equal a pledged, unflushed context's bytes, and 3 MiB of `mixed` at the default level |
| `StreamingInputChunking_DoesNotChangeBytes` | same | 3 MiB `mixed` at the default level sliced 1, 7, 4,096, 65,536, 131,072, 1 MiB and seeded-random gives identical bytes |
| `StreamingOutputChunking_DoesNotChangeBytes` | same | 3 MiB `mixed` at the default level with output spans of 1 (first 64 KiB only), 13, 4,096 and 1 MiB bytes gives identical bytes |
| `StreamingDecompress_AnySlicing_RoundTrips` | same | the same slicings on the decompression side reproduce the input |
| `Flush_ProducesDecodablePrefix` | same | after each `Flush`, a decompression context fed the bytes so far has produced exactly the input so far |
| `Flush_SameOffsets_SameBytes` | same | two streams flushed at the same offsets are identical |
| `PledgedSize_Mismatch_IsRejected` | same | over and under the pledge → `SIZE_MISMATCH`; no complete frame is produced |
| `Deterministic_RepeatedRunsAndInstances` | `_Determinism.cpp` | at the minimum, default and maximum levels: three runs on one codec and one run on each of two fresh codec instances give identical bytes for the sweep set (about 3.1 MiB of input per level) |
| `Deterministic_RepeatedRunsAndInstances_AllLevels` | same | `z2-stress`: the same assertion at every level of the codec (about 3.1 MiB per level) |
| `GoldenVectors_Match` | same | every fixture row's compressed size and FNV-1a 64 match |
| `GoldenVectors_LibraryVersionsMatch` | same | the fixture's `library` lines equal `GetInfo().m_szLibraryVersion`; a mismatch names the re-recording procedure |
| `Truncated_EveryPrefix_IsTruncated` | `_Failure.cpp` | at the default level: every prefix of the `text` frames of 512 B and 4 KiB input, and 100 seeded prefixes of the 1 MiB `mixed` frame, → `TRUNCATED_INPUT` one-shot and at streaming `End` |
| `Corrupt_BitFlips_DetectedOrHarmless` | same | 2,000 seeded single-bit flips over the header, body and trailer of the `text` 16 KiB frame and every bit of the `one` frame, both at the default level: each decode is an error, or succeeds with byte-identical output (a flip in unused padding bits); never success with different bytes, never a crash |
| `Corrupt_RandomBytes_AreRejected` | same | 1,000 seeded random buffers of 1 B – 64 KiB never decode successfully |
| `TrailingData_IsRejected` | same | one appended byte → `TRAILING_DATA` |
| `DestinationTooSmall_NoWritePastEnd` | same | destinations one byte short of the decoded and compressed sizes → `DESTINATION_TOO_SMALL`; 64 guard bytes unchanged |
| `DecodedSizeCap_IsEnforced` | same | a context with `m_ulMaxDecodedBytes` one below the size → `LIMIT_EXCEEDED` |
| `AllocationFailure_CreateFaultSweep` | same | `Zenith_RunFaultSweep` over context creation and over `CreateDictionary` (zstd) |
| `AllocationFailure_MidStream_IsOutOfMemory` | same | failing the first allocation of the first decode step → `OUT_OF_MEMORY`, FAILED, no leak after destroy |
| `AllocationFailure_OneShotArena_IsOutOfMemory` | same | arenas capped at each of the call's allocation points → `OUT_OF_MEMORY`, arena back at its marker |
| `Cancellation_DestroyAtEveryState_ReleasesAll` | same | a context destroyed in each state after 0, 1 and 7 chunks leaves the root heap's live allocations at their baseline |
| `Cancellation_ResetThenReuse_IdenticalOutput` | same | a context reset mid-stream then used again gives a fresh context's bytes |
| `Cancellation_PartialOutput_NeverDecodesComplete` | same | the bytes delivered before a cancellation fail to decode with `TRUNCATED_INPUT` |
| `Cancellation_StressThousandCycles` | same | `z2-stress`: 1,000 cycles of create, feed a seeded random amount of 0–256 KiB of `mixed` in seeded random slices, destroy or reset at a seeded random point, in both modes at the default level; live allocations and live contexts return to zero |
| `ScratchBoundIsSufficient` | `_Memory.cpp` | at the minimum, default and maximum levels: the sweep set and 3 MiB of `mixed` compress and decompress with an arena capped at exactly `GetCompressScratchBound`/`GetDecompressScratchBound` (about 3.6 MiB per level) |
| `ScratchBoundIsSufficient_AllLevels` | same | `z2-stress`: the same assertion at every level of the codec (about 3.6 MiB per level) |
| `ContextMemoryBoundHolds` | same | at the minimum, default and maximum levels: `GetMemoryBytes() ≤ GetContextMemoryBound()` after every call of a streaming round trip of the sweep set, and of 3 MiB `mixed` at the default level |
| `LibraryAllocations_AreAccounted` | same | the root heap's growth during a streaming round trip equals the context's reported memory |
| `ThreadSafety_SharedCodecConcurrentOneShot` | `_Threads.cpp` | 2, 8 and hardware-concurrency threads each run 50 seeded one-shot round trips of 16 KiB `mixed` slices at the default level on one shared codec with their own arenas; results equal a single-threaded run; TSan clean in `linux-tsan` |
| `ThreadSafety_ContextPerThread` | same | one context per thread, concurrent streams, identical bytes to a serial run |
| `ThreadSafety_ContextHandOff` | same | a context alternated between two threads through a `Zenith_Semaphore` hand-off per call; TSan clean |
| `ThreadSafety_SharedDictionary` | same | zstd: 8 threads compress and decompress with one dictionary; zlib: `CreateDictionary` is `UNSUPPORTED` |
| `Dictionary_CapabilityMatchesInfo` | `_Dictionary.cpp` | zstd: `json-small` round-trips with a trained dictionary in one-shot, section and streaming paths; zlib: `CreateDictionary` and `TrainDictionary` → `UNSUPPORTED` without allocating |
| `Dictionary_WrongOrMissing_IsMismatch` | same | zstd: decoding without the dictionary, or with another one → `DICTIONARY_MISMATCH` |
| `Dictionary_LevelMustMatch` | same | zstd: compressing with a dictionary at another level → `INVALID_LEVEL`; level 0 uses the dictionary's |
| `Dictionary_TrainingIsDeterministic` | same | zstd: two trainings over the same samples give identical bytes and ID |
| `Dictionary_TrainingLimits` | same | zstd: 15 samples → `INVALID_DESC`; capacity 255 and 1 MiB + 1 → `INVALID_DESC` |
| `LargeStream_Past4GiB_RoundTrips` | `_Large.cpp` | `z2-stress`, level 1 on both codecs (the 64-bit counting it proves does not depend on the level): `Zenith_CompressionConformanceCorpusStream` (see "Conformance corpus") generates 4 GiB + 1 MiB of `runs` data in 64 KiB blocks on the fly, each block compressed through one context and the output fed at once into a decompression context, so no more than one block of input and one of output exists at a time and nothing is written to disk; totals are exact, `GetTotalInputBytes() > 2^32`, and a rolling FNV-1a of the decoded bytes equals the generated one's. It runs only in `win-asan` — the `Sanitizers -Preset win-asan` step, change-triggered through `Zenith2Memory` and run in every stage acceptance — through CORE's idiom `if constexpr (Zenith_GetSanitizerKind() != SANITIZER_KIND_ASAN) { MESSAGE("skipped: runs in win-asan only"); return; }` (`Zenith2/Docs/Design/CORE/Diagnostics.md → "Build information"`): the base `Test` step never runs its body. Time budget per CTest entry, at the assumed `win-asan` throughputs (zlib level 1 about 60 MB/s compress and 120 MB/s inflate on `runs`, zstd level 1 about 150 MB/s and 400 MB/s, plus generation and hashing): `[Zlib]` about 140 s, `[Zstd]` about 70 s; each entry measures itself and fails with `exceeded its 480-second budget` beyond 480 s, inside its own 600-second `z2-stress` timeout |

### Conformance time budget

Every default-suite case fits the 60-second CTest cap of [Z2-N-CONTRACT-409] in `win-debug` (`/Od`, third-party code unoptimised too), the slowest configuration of the base `Test` step. Sizing rule: a default-suite case runs its sweep set at the minimum, default and maximum levels only, with two stated exceptions — `CompressBound_Holds` runs its nine sizes of at most 131,073 bytes at every level (under 0.4 MiB per level, seconds even at level 19) and `GoldenVectors_Match` runs every fixture row at its recorded level (zlib {1, 6, 9}, zstd {1, 3, 9, 19}); every other every-level sweep uses the 64 KiB sweep set and is labelled `z2-stress` (600 seconds); and no case feeds more than 8 MiB of input per level at levels ≥ 15 (zstd levels 15–19 run at about 0.7–1 MB/s in `win-debug`, 3–5 MB/s in `win-release`). Estimates per CTest entry, from those throughputs and zlib level 9 at about 5 MB/s, other levels at 10–60 MB/s:

| Case | Input at the slowest level | `win-debug` estimate `[Zstd]` / `[Zlib]` |
|---|---|---|
| `RoundTrip_CorpusThreeLevels` | 4.9 MiB at level 19 / level 9 | 10 s / 4 s |
| `RoundTrip_CorpusEveryLevel_AllLevels` (`z2-stress`) | 1.9 MiB per level; 9.5 MiB over levels 15–19 | 25 s / 6 s |
| `CompressBound_Holds` | 4.4 MiB of `random` at level 19 | 8 s / 2 s |
| `OneShotEqualsStreamingWithPledgedSize` | 1.3 MiB at level 19 | 3 s / 2 s |
| `StreamingInputChunking_DoesNotChangeBytes`, `StreamingOutputChunking_DoesNotChangeBytes` | 21 MiB and 12 MiB at the default level | 2 s / 5 s each |
| `Deterministic_RepeatedRunsAndInstances` | 3.1 MiB at level 19 | 5 s / 2 s |
| `Deterministic_RepeatedRunsAndInstances_AllLevels` (`z2-stress`) | 3.1 MiB per level | 25 s / 8 s |
| `ScratchBoundIsSufficient` | 3.6 MiB at level 19 | 6 s / 2 s |
| `ScratchBoundIsSufficient_AllLevels` (`z2-stress`) | 3.6 MiB per level | 30 s / 8 s |
| `ContextMemoryBoundHolds` | 0.6 MiB at level 19 | 1 s / 1 s |
| `GoldenVectors_Match` | 4.4 MiB at level 19 (see "Golden vector fixture format") | 7 s / 3 s |
| `Truncated_EveryPrefix_IsTruncated`, `Corrupt_BitFlips_DetectedOrHarmless` | about 50 MiB and 32 MiB of decoding | 2 s / 4 s each |
| `ThreadSafety_*` | 50 × 16 KiB per thread at the default level | under 5 s |
| `Cancellation_StressThousandCycles` (`z2-stress`) | about 128 MiB in total at the default level | 6 s / 16 s (about 10× under TSan) |
| `LargeStream_Past4GiB_RoundTrips` (`z2-stress`, `win-asan` only) | 4 GiB + 1 MiB at level 1 | 70 s / 140 s in `win-asan` |

Under `linux-tsan` (TSan adds roughly 5–10× to single-threaded compression), exactly these cases skip their maximum-level leg: `RoundTrip_CorpusThreeLevels`, the 1 MiB and 3 MiB legs of `CompressBound_Holds`, `OneShotEqualsStreamingWithPledgedSize`, `Deterministic_RepeatedRunsAndInstances`, `ScratchBoundIsSufficient` and `ContextMemoryBoundHolds`. The mechanism is CORE's single-preset idiom applied to the level list: each of these cases builds its level list as `{min, default}` instead of `{min, default, max}` when `if constexpr (Zenith_GetSanitizerKind() == SANITIZER_KIND_TSAN_UBSAN)` holds, and emits `MESSAGE("maximum-level leg skipped under TSan")` naming the skipped leg, so the leg's loop iteration never runs (the analogue of the `return;` in the large-stream row). `GoldenVectors_Match` runs in full under `linux-tsan`, level-19 rows included, because R-061 requires the golden vectors there; so do the `ThreadSafety_*` cases, which are TSan's purpose here. `win-asan` (`/O1`) runs every leg.

### Golden vectors

`GoldenVectors_Match` is the determinism evidence of Z2-I-COMPRESS-06: it runs in `win-debug`, `win-release`, `win-asan` and `linux-tsan` (so on MSVC and clang, instrumented and not), and on the S24 in the Stage 2 acceptance device run. A mismatch fails with the codec, level, class, the expected and actual size and hash, and the sentence `re-record only in the diff that changes a library pin (COMPRESS "Re-cook and migration")`.

### Fuzz targets

| Target | Input interpretation | Asserts |
|---|---|---|
| `Zenith_Fuzz_CompressedSectionDesc` | the first 24 bytes as a descriptor, the rest as its section | decode and validate, with the default `Zenith_CompressedSectionLimits` (experimental codecs refused, as `Zenith_EncodeCompressedSectionDesc` validates), never crash; a validated descriptor encodes back to the same 24 bytes; `Zenith_DecompressSection` never writes past a destination of the declared size |
| `Zenith_Fuzz_ZlibDecode` | the bytes as a zlib stream; one byte selects one-shot or streaming and an output slice size | no crash, no write past the destination, every failure a `Zenith_CompressionError` |
| `Zenith_Fuzz_ZlibRoundTrip` | a level byte then the payload | compress then decompress reproduces the payload |
| `Zenith_Fuzz_ZstdDecode` | as zlib, for zstd frames, with and without a fixed dictionary | as zlib |
| `Zenith_Fuzz_ZstdRoundTrip` | as zlib | as zlib |
| `Zenith_Fuzz_ZstdDictionaryLoad` | the bytes as dictionary bytes | `CreateDictionary` never crashes; a created dictionary round-trips a fixed input |

Each target creates its codec per input (no static state, [Z2-N-CONTRACT-141]) and runs a 60-second smoke in `win-asan`; stage acceptance runs each for its full duration ([Z2-N-ARCH-952]).

### Sanitizer coverage

| Sanitizer | What it covers here |
|---|---|
| ASan (`win-asan`) | zlib and zstd compiled from source with ASan: out-of-bounds reads of malformed input, destination overruns, use after `Reset` or destroy; arena scratch poisoned between one-shot calls (`Zenith2/Docs/Contracts/Memory.md → "ASan poisoning rules"`) |
| TSan + UBSan (`linux-tsan`) | the four thread-safety cases; the shared codec's atomic count; UBSan over both libraries' integer arithmetic |
| HWASan (S24) | the conformance suite on the device in stage acceptance |

The sanitizer steps run when a diff touches a `Zenith2Memory` path and in every stage acceptance. COMPRESS asks PROC to add `Zenith2/Engine/ZenithCompression*/**` to the `Zenith2Memory` paths, because COMPRESS parses untrusted input and routes third-party allocation hooks (cross-area notes).

### Libgraph swap-contract evidence

The `Libgraph` step writes `${ZENITH_OUT}/gates/libgraph/swap_compression.json` on every run (`Zenith2/Docs/Architecture.md → "Library dependency graph"`, [Z2-N-ARCH-080]). For the compression family the expected content is:

| Consumer | Symbols it may use |
|---|---|
| ZenithAssets, ZenithSave, ZenithRuntime | only `ZenithCompression` symbols and types |
| each game's generated composition TU, ZenithCook's and ZenithEditor's mains, the conformance executable, the benchmark, the test executables | additionally `Zenith_CreateCompression_Zlib` and `Zenith_CreateCompression_Zstd`, the only public symbols of the backends |
| any target | no zlib or zstd symbol outside the two backend libraries; no `zlib.h`, `zstd.h` or `zdict.h` reachable from any public header |

A clean report — every consumer depending on the API alone — is the swappability evidence the Stage 3 acceptance collects (`Zenith2/Docs/Roadmap.md → "Acceptance evidence"`).

### Benchmarks

`Zenith_Bench_Compression` (`Zenith2/Benchmarks/Compression/`) measures, with nanobench, for every codec × every level × every benchmark corpus class × mode:

| Metric | Definition |
|---|---|
| ratio | compressed bytes ÷ original bytes |
| one-shot compress throughput | original bytes ÷ median call time, MB/s (10^6 bytes) |
| one-shot decompress throughput | decoded bytes ÷ median call time, MB/s |
| streaming decompress throughput | decoded bytes ÷ time, with 64 KiB input and output slices |
| per-call p99 | the 99th percentile of `Process` call time at 128 KiB chunks (the cancellation-latency bound) |
| context memory | `GetMemoryBytes()` after the first call, compress and decompress |
| scratch bound | `GetCompressScratchBound` and `GetDecompressScratchBound` |
| dictionary gain | for `json-small` at zstd levels 3 and 19: ratio and decompress throughput with and without a dictionary trained on a disjoint half of the samples |

Benchmark corpus classes (`Internal/Zenith_CompressionBenchCorpus.cpp`, generated like the conformance corpus at 16 MiB per class): `text`, `json`, `json-small`, `floats`, `runs`, `random`, `mixed`; plus `--corpus-dir <dir>`, which adds every file under a directory as its own class (the human run uses the reference engine's asset directories at `C:/dev/Zenith` for cooked-content realism and records the directory's file list and hash).

Command line: `Zenith_Bench_Compression [--smoke] [--codec zlib|zstd|all] [--levels <list>|all] [--classes <list>|all] [--repetitions <n>] [--corpus-dir <dir>] [--out <file.json>] [--label <platform>]`. `--smoke` runs one repetition over 64 KiB of each class at each codec's minimum, default and maximum level, checks every round trip, and exits 0; it is the CTest smoke registered by `zenith_add_benchmark`.

Procedure for a reported run, per `Zenith2/Docs/Budgets.md → "Frame-rate definitions and measurement procedure"` (subsections "Repetitions and statistics" and "Controlled and ordinary conditions"): a Release build; controlled conditions (the reference desktop's locked clocks, the S24's fixed-performance mode); one discarded warm-up repetition; at least 10 repetitions, each a nanobench run of at least 11 epochs; per metric the median of the repetitions with the order-statistic 95% interval (`[x(2), x(9)]` for n = 10) and the run-to-run variance. The S24 run copies the Android build of the executable and the corpus with `adb push` to `/data/local/tmp/` and runs it with `adb shell`.

Report: `--out` writes JSON with `schema: "zenith2.bench.compression.v1"`, the build (tree SHA, preset, configuration), the platform label, the conditions, the library versions, and one record per (codec, level, class, metric) holding every repetition's value, the median, the interval and the variance. The human-run reports are committed under `Zenith2/Docs/Research/Compression/Stage2/<platform>/` with a `report.md` summary, per `Zenith2/Docs/Budgets.md → "Frame-rate definitions and measurement procedure"` ([Z2-N-BUDGET-079]).

## Budget rows

### Rows COMPRESS is measured against

COMPRESS owns two rows (`Zenith2/Docs/Budgets.md → "Row index"`, section "Decompression throughput floors" of `Zenith2/Docs/Budgets.md → "Per-system rows"`) and is measured inside eight rows other areas own:

| Row | Name | How COMPRESS contributes |
|---|---|---|
| Z2-B-COMPRESS-001 | zstd decompression throughput floor | owned; MB/s, MP-DECOMP, Win and S24, tolerance 5%, BR-ACCEPT; value pending allocation (see "Rows pending allocation") |
| Z2-B-COMPRESS-002 | zlib decompression throughput floor | owned; as Z2-B-COMPRESS-001 for zlib |
| Z2-B-ASSET-001 | Asset system resident data and decode buffers | decode scratch arenas and decompression contexts are allocated by ASSET and counted in this row; "Memory per context" gives their size |
| Z2-B-ASSET-002 | Decode queue depth | decode throughput sets how fast ASSET's bounded decode queue drains |
| Z2-B-SHIP-013 | Cold boot Windows | boot-time content is decompressed |
| Z2-B-SHIP-014 | Cold boot S24 | as above, at the S24's decode throughput |
| Z2-B-PACK-001 | AAB base module download size | the compression ratio of base-module content |
| Z2-B-PACK-002, Z2-B-PACK-003, Z2-B-PACK-004 | Windows install size per flagship slice | the compression ratio of each slice's archives |

### Rows pending allocation

Z2-B-COMPRESS-001 and Z2-B-COMPRESS-002 are pending until their producer, the Stage 1 per-system budget ticket for COMPRESS, allocates them. The measurement procedure is MP-DECOMP: `Zenith_Bench_Compression` in a Release build under controlled conditions, one discarded warm-up, at least 10 repetitions of at least 11 nanobench epochs, the statistic being the median one-shot decompress throughput at the codec's default level, judged on the `mixed` class of the benchmark corpus ([Z2-N-BUDGET-232], [Z2-N-BUDGET-233], [Z2-N-BUDGET-234]). The producer's harness half is the "Stage 1 per-system budget harness for COMPRESS", which is the ticket delivering `Zenith_Bench_Compression` under `Zenith2/Benchmarks/Compression/` with its `--smoke` run ([Z2-N-BUDGET-236]; `Zenith2/Docs/Budgets.md → "Purpose versioning and change rule"`, table "Per-system producer shape"); the pull-forward puts it in Stage 1 (see "Stage plan"). The run's report and evidence record follow [Z2-N-BUDGET-235].

## Stage plan

### Stage table

| Stage | Sprint | COMPRESS work | Fix version | Gated by |
|---|---|---|---|---|
| S0 | `Stage 0 — Tooling bring-up` | none; the scaffold, the gate steps and the toolchain manifest land here and are prerequisites | — | — |
| S1 | `Stage 1 — Architecture validation` (pulled forward, `Zenith2/Docs/Roadmap.md → "Stage 1 Architecture validation"` subsection "Stage 1 pull-forward") | every Stage 2 COMPRESS ticket below, because none depends on an unresolved decision; the benchmark harness, which is also the Stage 1 per-system budget harness for COMPRESS; the needs-human budget ruling that allocates Z2-B-COMPRESS-001 and Z2-B-COMPRESS-002 | `Z2 Stage 2` (the benchmark harness and the budget ruling: `Z2 Stage 1`, because they produce Stage 1 budget rows from a pulled-forward library, as `Zenith2/Docs/Budgets.md → "Purpose versioning and change rule"` (table "Per-system producer shape") allows) | the scaffold; the pulled-forward Core memory, span, result and fault-injector code; the pinned toolchain |
| S2 | `Stage 2 — Foundation` | whatever of the above did not finish in S1; the Stage 2 acceptance evidence: conformance and golden vectors green on every host preset, fuzz targets and allocation-failure injection, the S24 device run | `Z2 Stage 2` | — |
| S3 | `Stage 3 — Playable authoring slice` | the swap-contract report for the compression family collected in the Stage 3 acceptance | `Z2 Stage 3` | the consumers (ASSET, SAVE, MOD) existing |
| S4 | `Stage 4 — Production content and worlds` | none of its own; PACK's dictionary policy uses the benchmark's dictionary rows | — | — |
| S5 | `Stage 5 — Rendering and simulation completion` | none | — | — |
| S6 | `Stage 6 — Shipping qualification` | the zlib and zstd licence entries verified in the SHIP licence inventory; the fuzz targets' full-duration runs in the security evidence ([Z2-N-ARCH-960]) | `Z2 Stage 6` | SHIP's inventory |
| S7 | `Stage 7 — Later expansion` | none | — | — |

### Ticket slices and ordering

The interface lands first as an API-only slice, so ASSET, SAVE and MOD can write against it while the backends are built:

| # | Slice (kind) | Content | Blocked by |
|---|---|---|---|
| 1 | API-only (`api`) | the eight public headers of `ZenithCompression` with their contract blocks; `Zenith_ToString`, the setting functions, the descriptor codec, the codec set, the allocator adapter, `Zenith_CompressionDriver`, `Zenith_CompressionContext` and the facade; `Zenith_FakeCompression`; every `ZenithCompression_Tests` case that needs no real codec | the scaffold; CORE memory, span, result and fault-injector slices |
| 2 | zlib backend (`implementation`) | the manifest entry, the `ThirdParty/zlib` wrapper, `ZenithCompression_Zlib`, its unit tests, its conformance adapter, its two fuzz targets, and the conformance suite's first cases | 1 |
| 3 | zstd backend (`implementation`) | the manifest entry (with `cmakeSourceDir`), `ZenithCompression_Zstd`, dictionaries and training, its unit tests, its adapter, its three fuzz targets, the dictionary conformance cases | 1, and the manifest field of "Third-party integration of zstd" |
| 4 | conformance completion and golden vectors (`implementation`) | the remaining conformance cases, the golden fixture recorded, the `ZenithCompression_Tests` cases that use the real codecs, the example | 2, 3 |
| 5 | benchmark harness (`harness`) | `Zenith_Bench_Compression` and its CTest smoke; the Stage 1 per-system budget harness for COMPRESS | 2, 3 |
| 6 | budget ruling (`budget`, `needs-human`) | the reference-desktop and S24 benchmark runs, the report under `Zenith2/Docs/Research/`, the two rows allocated | 5, the Budgets rows existing |
| 7 | Stage 2 acceptance evidence (`acceptance` input) | the S24 conformance and golden run, full fuzz durations | 4 |
| 8 | swap-contract evidence (`integration`, Stage 3) | the clean `swap_compression.json` with the consumers present | 4 and the consumers |

## Decisions

### Settled decisions

| ID | What it settles for COMPRESS |
|---|---|
| Z2-D-004 | one neutral `Zenith_Compression` API; zlib and zstd backends as separate libraries, both required and both always linked; deterministic output per codec; codec IDs in cooked headers; streaming and one-shot modes, a conformance suite and benchmarks, in Stage 2 |
| Z2-D-001 | every backend is its own library target selected by linking; no preprocessor symbol selects one |
| Z2-D-011 | zlib and zstd are built from pinned source with the sanitizer flags in `win-asan`, `linux-tsan` and the HWASan build; suppressions only with a decision ID |
| Z2-D-038 | cooked data is versioned binary and never depends on C++ layout: the descriptor is encoded field by field |
| Z2-D-082 | zlib and zstd are pinned in `ThirdParty/manifest.json`, integrated unmodified, and confined to the backends' `Internal/` |
| Z2-D-040 | contexts, codecs and dictionaries are `Zenith_Owned` on the general heap; no STL smart pointer |
| Z2-D-045 | recoverable errors are `Zenith_Result`; asserts for programmer errors; no exceptions |
| Z2-D-042, Z2-D-007 | `Libgraph` (confinement, swap contract), `ForbiddenCheck`, `ApiContractCheck` and `TestSeparationCheck` are hard gates over these libraries |
| Z2-D-048 | a codec or pin change re-cooks; cooked data is never migrated at runtime |
| Z2-D-065 | the manifest hash is part of every evidence fingerprint, so a zlib or zstd bump invalidates COMPRESS evidence |
| Z2-D-084 | the budget measurement is split into the benchmark harness ticket and a `needs-human` ruling ticket |
| Z2-D-097 | a ruling a COMPRESS ticket records is appended to `Zenith2/Docs/DecisionLog.md` in that ticket's diff |

### Open decisions

No open decision governs COMPRESS, which is why its Stage 2 tickets are pulled forward into Stage 1. Three open decisions touch it; none blocks the design, and one of them, D-SEL-HEAP, makes exactly one ticket wait — the memory conformance family waits for the (COMPRESS, Stage 1, D-SEL-HEAP) revalidation pass, as its row records:

| Decision | Relationship |
|---|---|
| D-SEL-HEAP (open, Stage 1) | chooses the root heap's backing; COMPRESS allocates only through `Zenith_Heap`, and one number depends on the choice: the 4,096-byte slack of the `GetContextMemoryBound` row ("Scratch and memory bounds") covers usable-size rounding only on a backing whose usable size exceeds the request by at most its alignment padding, which the system backing reports in every current preset; a size-class backing (mimalloc rounds blocks above 128 KiB to 64 KiB–4 MiB granularity and reports that as usable size) needs a per-block rounding term in that row, so the (COMPRESS, Stage 1, D-SEL-HEAP) revalidation pass re-opens the row and `ContextMemoryBoundHolds` for any ruling other than the system allocator (sanitizer builds always use the system allocator) |
| D-SEL-HASH (open, Stage 1) | not used: the golden vectors hash with test-local FNV-1a, and zstd's checksums and dictionary IDs use zstd's internal XXH64 |
| D-SEL-JSON (open, Stage 1) | not used: the compression setting is parsed by its owning document's reader; COMPRESS validates values only |

### Design rulings owned by COMPRESS

These are the design choices C1–C10 of "Design choices at a glance", recorded here with their alternatives. They elaborate Z2-D-004 and need no new decision ID; a ticket that changes one edits this section in its diff (Z2-D-085).

<!-- informative -->
| Choice | Alternatives considered | Why this one |
|---|---|---|
| C1 canonical chunking with staging | (a) pass the caller's buffers straight to the library and rely on its output being slicing-independent; (b) forbid streaming for cooked data | (a) rests on undocumented library internals that a version bump may change — zstd, for instance, derives a pledged size from an `ZSTD_e_end` call that carries input; (b) removes SAVE's replay streaming. Staging costs 256 KiB per compress context and one copy |
| C2 one-shot on the streaming engine | the libraries' own one-shot calls (`compress2`, `ZSTD_compress2`) | their call pattern differs from streaming's, so equality would again rest on library internals |
| C3 non-virtual public members | pure-virtual public members per backend | validation, counters, zones, logging and arena restore would be duplicated in every backend |
| C4 level ranges | exposing zstd's negative levels and 20–22; zlib 0 | see the note under "Codec metadata table" |
| C5 checksums always | optional checksums | a 4-byte trailer per section is negligible, and it turns silent corruption into `CORRUPT_INPUT` |
| C6 zstd dictionaries | none; solid blocks; zlib preset dictionaries | see "Dictionary format and identifiers" |
| C7 8 MiB window cap | zstd's 128 MiB default cap | bounds the S24 streaming decoder at about 8.3 MiB and rejects hostile windows before allocation |
| C8 one-byte codec IDs with ranges | a 4-byte FourCC | one byte covers every plausible codec; ranges keep test codecs out of cooked data by construction |
| C9 a common 24-byte descriptor | each format inventing its own fields | one validated parser and one fuzz target for every container |
| C10 zlib as a wrapper with `Z_SOLO` | zlib's own CMake | zlib 1.3.1's CMake renames `zconf.h` inside the shared cache; `Z_SOLO` guarantees no allocation bypasses the adapter |
<!-- /informative -->

## Requirement index

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-COMPRESS-001 | API library exports the interface and no factory | "Target summary", "ZenithCompression files", PublicAPI.md "Zenith_Compression.h" |
| Z2-R-COMPRESS-002 | Interface contract blocks and results | "Header map", PublicAPI.md "Zenith_Compression.h", PublicAPI.md "Backend factory headers" |
| Z2-R-COMPRESS-003 | Interface shape: virtual destructor and private hooks | PublicAPI.md "Zenith_Compression.h", PublicAPI.md "Zenith_CompressionEngine.h", PublicAPI.md "Zenith_CompressionDictionary.h", "Thread affinity per type" |
| Z2-R-COMPRESS-004 | Codec metadata matches the table | "Codec metadata table", PublicAPI.md "Zenith_CompressionTypes.h" |
| Z2-R-COMPRESS-005 | Level resolution | PublicAPI.md "Zenith_Compression.h", "Codec metadata table", "Context desc validation" |
| Z2-R-COMPRESS-006 | Error enumeration values and names | PublicAPI.md "Zenith_CompressionTypes.h", "Error enumeration" |
| Z2-R-COMPRESS-007 | Factory signature and pointer type | PublicAPI.md "Backend factory headers", PublicAPI.md "Zenith_Compression.h", "Target summary" |
| Z2-R-COMPRESS-008 | Factory failure unwinds | PublicAPI.md "Backend factory headers", "Failure table" |
| Z2-R-COMPRESS-009 | Codec set selects codecs by ID | PublicAPI.md "Zenith_CompressionCodecSet.h", "Codec ID registry" |
| Z2-R-COMPRESS-010 | Third-party headers confined to backend Internal | "ZenithCompression_Zlib files", "ZenithCompression_Zstd files", "Header map" |
| Z2-R-COMPRESS-011 | Consumers depend on the API alone | "Libgraph swap-contract evidence", "Areas that consume COMPRESS" |
| Z2-R-COMPRESS-012 | Both codecs created at startup | "Areas that consume COMPRESS", "Lifetime and shutdown order" |
| Z2-R-COMPRESS-013 | No Luau compression surface | "Luau surface" |
| Z2-R-COMPRESS-014 | zlib backend library target | "Target summary", "ZenithCompression_Zlib files" |
| Z2-R-COMPRESS-015 | zstd backend library target | "Target summary", "ZenithCompression_Zstd files" |
| Z2-R-COMPRESS-016 | zlib pinned at 1.3.1 | "Third-party integration of zlib" |
| Z2-R-COMPRESS-017 | zlib built as a wrapper with Z_SOLO | "Third-party integration of zlib", "Design rulings owned by COMPRESS" |
| Z2-R-COMPRESS-018 | zstd pinned at 1.5.7 with fixed options | "Third-party integration of zstd" |
| Z2-R-COMPRESS-019 | Libraries built from source in sanitizer presets | "Third-party integration of zstd", "Sanitizer coverage" |
| Z2-R-COMPRESS-020 | One copy of each library per link | "Third-party integration of zstd" |
| Z2-R-COMPRESS-021 | Library allocations routed through the adapter | PublicAPI.md "Zenith_CompressionAllocator.h", "Dictionary creation and training" |
| Z2-R-COMPRESS-022 | Allocation adapter behaviour | PublicAPI.md "Zenith_CompressionAllocator.h", "Locks and blocking" |
| Z2-R-COMPRESS-023 | zlib stream format | "zlib stream format as written", "One-shot decompression with zlib" |
| Z2-R-COMPRESS-024 | zstd frame format | "zstd frame format as written", "One-shot decompression with zstd" |
| Z2-R-COMPRESS-025 | zstd decode window cap | "zstd frame format as written", "Untrusted input rules" |
| Z2-R-COMPRESS-026 | zlib error mapping | "Error mapping for zlib" |
| Z2-R-COMPRESS-027 | zstd error mapping | "Error mapping for zstd" |
| Z2-R-COMPRESS-028 | zlib 64-bit counting on LLP64 | "One-shot decompression with zlib", "Conformance cases" |
| Z2-R-COMPRESS-029 | Streaming context creation | "Context desc validation", PublicAPI.md "Zenith_CompressionContext.h" |
| Z2-R-COMPRESS-030 | Context creation failure unwinds | "Context desc validation", "Failure table" |
| Z2-R-COMPRESS-031 | Bounded work per streaming call | "Compression driver loop", "Frame-graph placement" |
| Z2-R-COMPRESS-032 | Bounded output | "Compression driver loop", "Streaming decompression context state machine" |
| Z2-R-COMPRESS-033 | Canonical input chunking | "Canonical chunking", "Compression driver loop" |
| Z2-R-COMPRESS-034 | Fixed output staging | "Canonical chunking", "Staging buffers and context layout" |
| Z2-R-COMPRESS-035 | zstd pledged size never derived from end | "Canonical chunking", "Context desc validation" |
| Z2-R-COMPRESS-036 | Streaming compression state machine | "Streaming compression context state machine" |
| Z2-R-COMPRESS-037 | Streaming decompression state machine | "Streaming decompression context state machine" |
| Z2-R-COMPRESS-038 | Flush yields a decodable prefix | "Compression driver loop", "Streaming compression context state machine" |
| Z2-R-COMPRESS-039 | Pledged size enforced | "Compression driver loop", "Streaming compression context state machine" |
| Z2-R-COMPRESS-040 | Decoded size cap and expected size | "Streaming decompression context state machine", "Untrusted input rules" |
| Z2-R-COMPRESS-041 | Sticky errors | "Streaming compression context state machine", "Streaming decompression context state machine" |
| Z2-R-COMPRESS-042 | Reset reuses a context | "Streaming compression context state machine", "Cancellation" |
| Z2-R-COMPRESS-043 | Totals exact beyond 4 GiB | "Conformance cases", PublicAPI.md "Zenith_CompressionContext.h" |
| Z2-R-COMPRESS-044 | Context memory bound | "Memory per context", "Scratch and memory bounds" |
| Z2-R-COMPRESS-045 | Stuck engine detection | "Streaming decompression context state machine" |
| Z2-R-COMPRESS-046 | One-shot compression | "One-shot compression", PublicAPI.md "Zenith_Compression.h" |
| Z2-R-COMPRESS-047 | One-shot decompression | "One-shot decompression with zlib", "One-shot decompression with zstd" |
| Z2-R-COMPRESS-048 | One-shot calls restore the scratch arena | "One-shot compression", "Programmer errors" |
| Z2-R-COMPRESS-049 | Scratch bounds are sufficient | "Scratch and memory bounds", "One-shot compression" |
| Z2-R-COMPRESS-050 | Compress bound | "Scratch and memory bounds" |
| Z2-R-COMPRESS-051 | One-shot equals pledged streaming | "One-shot compression", "Canonical chunking" |
| Z2-R-COMPRESS-052 | Zero-length input | "zlib stream format as written", "zstd frame format as written" |
| Z2-R-COMPRESS-053 | Frame inspection | PublicAPI.md "Zenith_CompressionTypes.h", "One-shot decompression with zstd" |
| Z2-R-COMPRESS-054 | Section compression | "Section compression and decompression", PublicAPI.md "Zenith_CompressedSection.h" |
| Z2-R-COMPRESS-055 | Section decompression | "Section compression and decompression" |
| Z2-R-COMPRESS-056 | Determinism definition | "Determinism definition and parameter pinning" |
| Z2-R-COMPRESS-057 | Output independent of caller slicing | "Canonical chunking", "Determinism definition and parameter pinning" |
| Z2-R-COMPRESS-058 | Flush offsets determine bytes | "Determinism definition and parameter pinning" |
| Z2-R-COMPRESS-059 | zlib parameters pinned | "Determinism definition and parameter pinning" |
| Z2-R-COMPRESS-060 | zstd parameters pinned | "Determinism definition and parameter pinning" |
| Z2-R-COMPRESS-061 | Golden vectors on host builds | "Golden vectors", "Golden vector fixture format" |
| Z2-R-COMPRESS-062 | Golden vectors on the S24 | "Golden vectors", "Stage table" |
| Z2-R-COMPRESS-063 | Pin changes re-record and re-cook | "Re-cook and migration", "Golden vector fixture format" |
| Z2-R-COMPRESS-064 | No library-internal threads or clocks | "Third-party integration of zstd", "Determinism definition and parameter pinning" |
| Z2-R-COMPRESS-065 | Codec ID registry values | "Codec ID registry" |
| Z2-R-COMPRESS-066 | Descriptor on-disk layout | "Compressed section descriptor on disk" |
| Z2-R-COMPRESS-067 | Descriptor validation | "Descriptor validation" |
| Z2-R-COMPRESS-068 | Cooked formats embed the descriptor | "How cooked formats embed the descriptor" |
| Z2-R-COMPRESS-069 | Readers select the codec from the data | "How cooked formats embed the descriptor", "Section compression and decompression" |
| Z2-R-COMPRESS-070 | Codec names and setting validation | "Compression setting fragment", "Editor surfaces" |
| Z2-R-COMPRESS-071 | Library version exposed for cache keys | "Cook integration", "Codec metadata table" |
| Z2-R-COMPRESS-072 | Dictionary creation | "Dictionary creation and training", "Dictionary format and identifiers" |
| Z2-R-COMPRESS-073 | Dictionaries unsupported on zlib | "Dictionary creation and training", "Codec metadata table" |
| Z2-R-COMPRESS-074 | Dictionary round trip | "Dictionary format and identifiers", "zstd frame format as written" |
| Z2-R-COMPRESS-075 | Dictionary mismatch detected | "One-shot decompression with zstd", "Section compression and decompression" |
| Z2-R-COMPRESS-076 | Dictionary level must match | "One-shot compression", "Context desc validation" |
| Z2-R-COMPRESS-077 | Deterministic dictionary training | "Dictionary creation and training", "Dictionary training in the cook" |
| Z2-R-COMPRESS-078 | Dictionaries shared across threads | "Thread affinity per type", PublicAPI.md "Zenith_CompressionDictionary.h" |
| Z2-R-COMPRESS-079 | Shared codec is thread-safe | "Thread affinity per type" |
| Z2-R-COMPRESS-080 | Context is single-owner | "Thread affinity per type" |
| Z2-R-COMPRESS-081 | No locks and no blocking | "Locks and blocking", "Frame-graph placement" |
| Z2-R-COMPRESS-082 | Codec outlives its objects | "Lifetime and shutdown order", "Programmer errors" |
| Z2-R-COMPRESS-083 | Truncated input detected | "Failure table", "One-shot decompression with zlib", "One-shot decompression with zstd" |
| Z2-R-COMPRESS-084 | Corrupt input detected or harmless | "Failure table", "Untrusted input rules" |
| Z2-R-COMPRESS-085 | Destination never overrun | "Untrusted input rules", "Failure table" |
| Z2-R-COMPRESS-086 | Mid-stream allocation failure | "Context desc validation", "Failure table" |
| Z2-R-COMPRESS-087 | Cancellation releases everything | "Cancellation" |
| Z2-R-COMPRESS-088 | Cancellation stress | "Cancellation", "Conformance cases" |
| Z2-R-COMPRESS-089 | Untrusted input never asserts | "Untrusted input rules", "Programmer errors" |
| Z2-R-COMPRESS-090 | Fuzz targets | "Fuzz targets" |
| Z2-R-COMPRESS-091 | Fuzz full-duration evidence | "Fuzz targets", "Stage table" |
| Z2-R-COMPRESS-092 | Failure logging | "Log category and messages" |
| Z2-R-COMPRESS-093 | Compression counters | "Counters" |
| Z2-R-COMPRESS-094 | Compression zones | "Profiler zones" |
| Z2-R-COMPRESS-095 | Context memory reported | "Memory per context", "Counters" |
| Z2-R-COMPRESS-096 | Conformance executable | "Conformance benchmark and fuzz targets", "Conformance cases" |
| Z2-R-COMPRESS-097 | Conformance corpus | "Conformance corpus" |
| Z2-R-COMPRESS-098 | Conformance case list | "Conformance cases" |
| Z2-R-COMPRESS-099 | Conformance under sanitizers | "Sanitizer coverage", "Test targets and gate steps" |
| Z2-R-COMPRESS-100 | Benchmark harness | "Benchmarks", "Conformance benchmark and fuzz targets" |
| Z2-R-COMPRESS-101 | Benchmark smoke | "Benchmarks" |
| Z2-R-COMPRESS-102 | Benchmark procedure and report | "Benchmarks" |
| Z2-R-COMPRESS-103 | Benchmark on reference hardware | "Rows pending allocation", "Ticket slices and ordering" |
| Z2-R-COMPRESS-104 | Throughput floors allocated | "Rows pending allocation", "Rows COMPRESS is measured against" |
| Z2-R-COMPRESS-105 | Decode cost counted in the asset and boot rows | "Rows COMPRESS is measured against" |
| Z2-R-COMPRESS-106 | Benchmark ratios supplied to package measurement | "Rows COMPRESS is measured against" |
| Z2-R-COMPRESS-107 | API-only slice lands first | "Ticket slices and ordering", "Stage table" |
| Z2-R-COMPRESS-108 | Pulled forward into Stage 1 | "Stage table", "Open decisions" |
| Z2-R-COMPRESS-109 | Licence inventory entries | "Third-party integration of zlib", "Third-party integration of zstd", "Stage table" |
| Z2-R-COMPRESS-110 | zlib engine step semantics | "Zlib engine step", "Library call enumerations and allocation thunks" |
| Z2-R-COMPRESS-111 | zstd engine step semantics | "Zstd engine step", "Library call enumerations and allocation thunks" |
| Z2-R-COMPRESS-112 | Conformance cases fit their time caps | "Conformance time budget", "Conformance cases" |
