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
// Other engine CLI flags (--list-automated-tests, --automated-test,
// --all-automated-tests, --exit-after-frames, --fixed-dt,
// --test-results, --test-results-dir, --skip-tool-exports,
// --skip-unit-tests) are parsed by their respective consumers
// (Zenith_AutomatedTestRunner::ParseCommandLine, Zenith_Main's
// Zenith_HasCommandLineFlag helper).
// ============================================================================

namespace Zenith_CommandLine
{
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
