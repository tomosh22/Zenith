#pragma once

// ============================================================================
// Zenith_CommandLine
//
// Central parser and accessor for engine-level command-line flags. Authored
// 2026-05-13 to give Flux init + the test harness + the window layer a SINGLE
// source of truth for `--headless` (previously each parsed __argv directly).
//
// **Headless semantics, post-MVP-0.0.8:**
//   --headless    -> hide the GLFW window (legacy behaviour) AND skip every
//                    Vulkan / Flux init step. The exe boots through Zenith
//                    subsystems that don't need a GPU (asset registry CPU
//                    half, scene manager, physics, script registrations) and
//                    is suitable for headless CI runners without a Vulkan ICD.
//
// Tests that DO need rendering input (materials, fog hole tables, render
// hooks, visual wiring counts) set `m_bRequiresGraphics = true` on their
// Zenith_AutomatedTest registration; the harness skips them when
// IsHeadless() returns true. See AutomatedTest.h for that flag and
// Games/DevilsPlayground/Docs/CIPolicy.md for the CI policy that depends
// on this.
//
// Other engine CLI flags (--list-automated-tests, --automated-test,
// --all-automated-tests, --exit-after-frames, --fixed-dt,
// --test-results, --test-results-dir, --skip-tool-exports,
// --skip-unit-tests) continue to be parsed by their respective consumers
// (Zenith_AutomatedTestRunner::ParseCommandLine, Zenith_Main's
// Zenith_HasCommandLineFlag helper). Migrating those into here is a
// follow-up; we centralise just --headless for now to unblock dp-tests
// (Q-2026-05-12-007 resolution).
// ============================================================================

namespace Zenith_CommandLine
{
    // Parse the process command line into the static accessor state. Call
    // ONCE during the earliest start-up step (Zenith_Core::Zenith_Main on
    // Windows, before window creation). Repeat calls overwrite previous
    // state but are otherwise harmless.
    void Parse(int argc, char** argv);

    // True iff `--headless` was on the command line. Implies "no Vulkan
    // device, no GPU resources, no window visible".
    bool IsHeadless();

    // Screenshot capture: `--screenshot <path>` [`--screenshot-frame <N>`].
    // The render backend dumps the swapchain image to <path> (an uncompressed
    // 32-bit TGA) on the EndFrame whose FluxRenderer frame counter equals N,
    // giving a deterministic, OS-compositor-free A/B oracle (cf. the
    // CopyFromScreen fallback). Returns nullptr when --screenshot was absent;
    // the returned pointer is into argv (process-lifetime), mirroring the
    // automated-test runner. GetScreenshotFrame() defaults to 120.
    const char* GetScreenshotPath();
    u_int       GetScreenshotFrame();
}
