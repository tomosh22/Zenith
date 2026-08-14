#pragma once

// ============================================================================
// Zenith_CommandLine
//
// Central parser and accessor for engine-level command-line flags.
//
// **HEADLESS IS BUILD-TIME, NOT A FLAG.** There is no `--headless`. A headless
// run is a `Null_*` config: it defines `ZENITH_NULL_RENDERER`, compiles the
// GPU-less `Zenith/Null` backend instead of Vulkan, and creates its window
// hidden. Every render path therefore RUNS (pass callbacks, uploads, the editor
// ImGui frame) against no-op backend calls, rather than being skipped by a
// runtime branch -- which is what makes a headless CI run representative of a
// windowed one. Compile-time gates use `#ifdef ZENITH_NULL_RENDERER`.
//
// Tests that genuinely READ PIXELS (screenshots, bitmap asserts, A/B captures)
// set `m_bRequiresGraphics = true` on their Zenith_AutomatedTest registration;
// the harness skips those in Null builds. See AutomatedTest.h for that flag and
// Games/DevilsPlayground/Docs/CIPolicy.md for the CI policy that depends on it.
//
// NOTE --exit-after-frames is NOT a general "quit after N frames" switch: it is
// consumed only inside Zenith_AutomatedTestRunner's Stepping phase, so it does
// nothing without an --automated-test selection flag. To boot and quit, use
// --exit-after-unit-tests (below) or --bench-ecs.
//
// Other engine CLI flags (--list-automated-tests, --automated-test,
// --all-automated-tests, --exit-after-frames, --fixed-dt,
// --test-results, --test-results-dir, --skip-tool-exports,
// --skip-unit-tests) are parsed by their respective consumers
// (Zenith_AutomatedTestRunner::ParseCommandLine, Zenith_Main's
// Zenith_HasCommandLineFlag helper).
// ============================================================================

// ============================================================================
// Zenith_IndirectCountMode — the Core-owned vocabulary for
// --indirect-count-mode. Deliberately NOT Flux_IndirectDrawOverride: Core must
// not include a Flux type, and the backend converts this value into the
// Flux enum at device-initialisation time. auto is the shipping default;
// native/padded/single are test assertions that fail closed when their tier
// cannot legally run. See Docs/design/TerrainIndirectCountFallback.md.
// ============================================================================
enum class Zenith_IndirectCountMode : uint8_t
{
    Auto,
    Native,
    Padded,
    Single,
};

namespace Zenith_CommandLine
{
    // Every flag this parser owns, in one value type. Defaults here ARE the
    // no-flag defaults, which is what makes Parse's "reset before parsing"
    // contract a single assignment rather than a dozen lines that can drift
    // out of step with the accessors.
    struct Flags
    {
        bool        m_bAutomatedTestRun   = false;
        bool        m_bNoImGuiIni         = false;
        bool        m_bShaderDebugO0      = false;
        const char* m_szScreenshotPath    = nullptr;
        u_int       m_uScreenshotFrame    = 120;
        const char* m_szAssetsRoot        = nullptr;
        const char* m_szTestSaveRoot      = nullptr;
        const char* m_szTestSaveRunId     = nullptr;
        const char* m_szBootProfileDump   = nullptr;
        bool        m_bSkipBootCapture    = false;
        const char* m_szUnitTestTimings   = nullptr;
        bool        m_bExitAfterUnitTests = false;
        // --indirect-count-mode=auto|native|padded|single (Phase 1 of the
        // terrain indirect-count compatibility plan). Stored as a small enum
        // so the parser owns the vocabulary — Core must not include or return
        // a Flux type. The backend converts this value into
        // Flux_IndirectDrawOverride during device initialisation. auto is the
        // shipping default; native/padded/single are test assertions that
        // fail closed when their tier cannot legally run.
        Zenith_IndirectCountMode m_eIndirectCountMode = Zenith_IndirectCountMode::Auto;
    };

    // Pure parse: reads argv, touches NO process state, returns the result.
    // Exposed for the same reason as ResolveBootProfileDumpArg — it lets the
    // parser be exercised against arbitrary argv without clobbering the flag
    // state the rest of the running test batch depends on. Unrecognised
    // arguments are ignored, as is a value-taking flag with no value after it.
    Flags ParseArgs(int argc, char** argv);

    // Parse the process command line into the static accessor state. Call
    // ONCE during the earliest start-up step (Zenith_Core::Zenith_Main on
    // Windows, before window creation). Repeat calls overwrite previous
    // state but are otherwise harmless.
    void Parse(int argc, char** argv);

    // True iff `--automated-test` or `--all-automated-tests` was on the
    // command line. Parsed HERE (not just by Zenith_AutomatedTestRunner,
    // whose ParseCommandLine deliberately runs after Zenith_Init) because
    // ImGui ini gating needs the answer during editor init — tests must get
    // the deterministic code-built dock layout, never a stale imgui.ini.
    bool IsAutomatedTestRun();

    // True iff `--no-imgui-ini` was on the command line. Forces the editor
    // to skip ImGui ini load/save even for interactive runs (used by capture
    // harnesses / smoke scripts that want the code-built default layout).
    bool IsImGuiIniDisabled();

    // Screenshot capture: `--screenshot <path>` [`--screenshot-frame <N>`].
    // The render backend dumps the swapchain image to <path> (an uncompressed
    // 32-bit TGA) on the EndFrame whose FluxRenderer frame counter equals N,
    // giving a deterministic, OS-compositor-free A/B oracle (cf. the
    // CopyFromScreen fallback). Returns nullptr when --screenshot was absent;
    // the returned pointer is into argv (process-lifetime), mirroring the
    // automated-test runner. GetScreenshotFrame() defaults to 120.
    const char* GetScreenshotPath();
    u_int       GetScreenshotFrame();

    // True iff `--shader-debug-o0` was on the command line. Opt-in deep-debug: the
    // runtime Slang compile disables optimization (O0) in ADDITION to the Debug-build
    // debug info. Off by default because O0 changes float re-association (moves pixels),
    // so it is never the shipping/default path. (Flux Shader System Overhaul — Stage 1.)
    bool        IsShaderDebugO0();

    // Relocatable-package asset override: `--assets-root <path>`. The baked
    // GAME_ASSETS_DIR / ENGINE_ASSETS_DIR / SHADER_SOURCE_ROOT defines are
    // ABSOLUTE paths into the build machine's source tree, so a packaged exe
    // cannot find assets on another machine. `zenith package` emits a run.bat
    // that passes the package root here. Returns nullptr when absent (the
    // default: baked paths, unchanged behaviour); the pointer is into argv
    // (process-lifetime), mirroring GetScreenshotPath.
    const char* GetAssetsRoot();

    // Automated-test save sandbox: `--test-save-root <path>` +
    // `--test-save-run-id <id>`. The test runner creates a per-run directory
    // under the artifacts root, writes the ownership marker into it, and passes
    // both here; Zenith_SaveData accepts the root ONLY if that marker's run-id
    // matches this id, and otherwise falls back to an engine-owned sandbox.
    // Both return nullptr when absent (pointers are into argv, process-lifetime).
    const char* GetTestSaveRoot();
    const char* GetTestSaveRunId();

    // Boot profiling: `--boot-profile-dump[=path]` and `--skip-boot-capture`.
    //
    // GetBootProfileDumpPath returns nullptr when the flag was absent, otherwise the
    // requested path (or "zenith_boot_profile_dump.txt" for the bare form). The
    // profiler reads it while allocating the boot capture -- inside Zenith_Init, long
    // before Zenith_AutomatedTestRunner::ParseCommandLine runs -- which is why these
    // two live here rather than with their consumer. In a NON-tools build the flag
    // also opts the capture into retaining raw events (tools builds always do).
    //
    // IsBootCaptureSkipped is the calibration switch: the capture is never allocated,
    // a full ring drops exactly as it did before boot capture existed, and only the
    // machine-readable BootSummary line is logged -- so a calibration run stays
    // directly comparable with a captured one. Android never calls Parse, so both
    // return their defaults there (capture on, no dump, aggregates only).
    const char* GetBootProfileDumpPath();
    bool        IsBootCaptureSkipped();

    // `--unit-test-timings[=path]`: dump every registered unit test with its wall
    // clock, slowest first, at the end of the boot-time RunAllTests batch. Same
    // parse-here rationale as the boot flags — the batch runs inside Zenith_Init.
    // Returns nullptr when absent (the default: one extra summary LINE is logged
    // either way, but no table is built and no file is written).
    const char* GetUnitTestTimingsPath();

    // `--exit-after-unit-tests`: shut down cleanly the moment Zenith_Init returns,
    // which is AFTER the boot-time RunAllTests batch has run and logged its tally.
    //
    // This exists because `--exit-after-frames N` is NOT a general "run N frames
    // then quit" switch -- it is parsed by Zenith_AutomatedTestRunner and consumed
    // only inside its Stepping phase, so with no --automated-test selection flag
    // the runner is inactive, Tick() early-outs, and the flag does nothing at all.
    // The unit gate passed it for exactly that purpose and consequently idled
    // forever, with its watchdog kill as the only thing ending the process -- which
    // made every gate run cost the FULL -TimeoutSec whether it passed or failed.
    //
    // Exiting here (rather than from inside RunAllTests) means the engine is fully
    // initialised, so teardown runs the normal ordered shutdown. Same run-then-exit
    // shape as --bench-ecs.
    bool IsExitAfterUnitTestsRequested();

    // `--indirect-count-mode=auto|native|padded|single`: the boot-time immutable
    // override for the semantic counted-indirect draw operation. Auto is the
    // shipping default (and the only legal value Android takes today, because
    // android_main never calls Parse); native/padded/single are test assertions.
    // The backend's device initialisation converts this value into a
    // Flux_IndirectDrawOverride and reads it once at boot — worker recording
    // never mutates it. See Docs/design/TerrainIndirectCountFallback.md and the
    // Phase 1 CLI/parser tests in Core/Zenith_CommandLine.Tests.inl.
    Zenith_IndirectCountMode GetIndirectCountMode();

    // Pure string -> enum resolver, exposed so the parsing contract is testable
    // WITHOUT re-running Parse (which would clobber the process-wide flag state
    // for the rest of a test batch). Returns the value to set on Flags. A bare
    // `--indirect-count-mode` (no '=') and an unknown spelling both fall through
    // to the default (Auto) rather than silently taking a partial value.
    // szValue is the text after '=' (or nullptr for the bare form).
    Zenith_IndirectCountMode ResolveIndirectCountModeArg(const char* szValue,
        Zenith_IndirectCountMode eDefault = Zenith_IndirectCountMode::Auto);

    // Pure split of `--boot-profile-dump[=path]`: returns the text after the first
    // '=', or szDefaultPath for the bare form (and for a trailing '=' with nothing
    // after it). Exposed so the parsing contract is testable WITHOUT re-running
    // Parse, which would clobber the process-wide flag state for the rest of a
    // test batch (see the note at the head of Zenith_CommandLine.Tests.inl).
    const char* ResolveBootProfileDumpArg(const char* szArg, const char* szDefaultPath);

    // Pick the effective on-disk dir for a baked compile-time path given an
    // optional override root. Pure: no override (null/empty) returns the baked
    // dir UNCHANGED (including the deliberately-empty "" that FluxCompiler/
    // hub/Android bake); otherwise returns "<override root>/<relative under
    // root>" with trailing root separators trimmed (`run.bat` passes "%~dp0",
    // which ends in a backslash). Lives HERE (Core, L0) because both the asset
    // dirs (Zenith_Engine::InitialiseAssets) and the shader source root
    // (Flux_SlangCompiler / hot reload) resolve through it -- Flux must not
    // reach up into AssetHandling.
    std::string ResolveUnderAssetsRoot(const std::string& strBakedDir, const char* szOverrideRoot, const std::string& strRelativeUnderRoot);
}
