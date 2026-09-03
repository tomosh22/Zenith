#include "Zenith.h"

#include "Core/Zenith_CommandLine.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>   // std::snprintf (used by the CLI unit tests' mutable argv buffers)

namespace
{
    bool        s_bParsed           = false;
    bool        s_bAutomatedTestRun = false;
    bool        s_bNoImGuiIni       = false;
    bool        s_bShaderDebugO0    = false;
    const char* s_szScreenshotPath  = nullptr;
    u_int       s_uScreenshotFrame  = 120;
    const char* s_szAssetsRoot      = nullptr;
    const char* s_szTestSaveRoot    = nullptr;
    const char* s_szTestSaveRunId   = nullptr;
    const char* s_szBootProfileDump = nullptr;
    bool        s_bSkipBootCapture  = false;
    const char* s_szUnitTestTimings = nullptr;
    bool        s_bExitAfterUnitTests = false;
    u_int       s_uWindowWidth       = 0;
    u_int       s_uWindowHeight      = 0;
    Zenith_IndirectCountMode s_eIndirectCountMode = Zenith_IndirectCountMode::Auto;

    // --boot-profile-dump with no "=path" writes here. A file-scope literal, not a
    // buffer: the accessor hands back a process-lifetime pointer exactly like the
    // argv-derived paths beside it.
    const char* const szDEFAULT_BOOT_PROFILE_DUMP = "zenith_boot_profile_dump.txt";
    const char* const szDEFAULT_UNIT_TEST_TIMINGS = "zenith_unit_test_timings.txt";
}

namespace
{
    using Zenith_CommandLine::Flags;

    // How a flag consumes the command line.
    enum class FlagArity
    {
        Bare,     // exact-name match, no value       -> "--no-imgui-ini"
        Value,    // exact-name match + the NEXT argv -> "--screenshot <path>"
        Prefixed, // name is a PREFIX of the argument -> "--boot-profile-dump[=path]"
    };

    struct FlagSpec
    {
        const char* m_szName;
        FlagArity   m_eArity;
        // Bare: szValue is null. Value: the following argv entry.
        // Prefixed: the WHOLE argument, so the handler can split at '='.
        void        (*m_pfnApply)(Flags&, const char* szValue);
    };

    // ---- handlers (captureless free functions, no std::function) ----------
    // Every harness SELECTION flag must be in this table. Consumers of
    // IsAutomatedTestRun() swap production state for test state (editor
    // imgui.ini suppression, DP's MetaSave _Test slot, ZM's save-slot
    // aliasing) -- a flag missing from the set silently runs an automated
    // batch against PRODUCTION save data.
    void ApplyAutomatedTest(Flags& x, const char*)      { x.m_bAutomatedTestRun = true; }
    void ApplyNoImGuiIni(Flags& x, const char*)         { x.m_bNoImGuiIni = true; }
    void ApplyShaderDebugO0(Flags& x, const char*)      { x.m_bShaderDebugO0 = true; }
    void ApplySkipBootCapture(Flags& x, const char*)    { x.m_bSkipBootCapture = true; }
    void ApplyExitAfterUnitTests(Flags& x, const char*) { x.m_bExitAfterUnitTests = true; }

    void ApplyScreenshot(Flags& x, const char* sz)      { x.m_szScreenshotPath = sz; }
    void ApplyScreenshotFrame(Flags& x, const char* sz) { x.m_uScreenshotFrame = static_cast<u_int>(std::atoi(sz)); }
    void ApplyAssetsRoot(Flags& x, const char* sz)      { x.m_szAssetsRoot = sz; }
    void ApplyTestSaveRoot(Flags& x, const char* sz)    { x.m_szTestSaveRoot = sz; }
    void ApplyTestSaveRunId(Flags& x, const char* sz)   { x.m_szTestSaveRunId = sz; }
    void ApplyWindowSize(Flags& x, const char* sz)
    {
        u_int uWidth = 0u;
        u_int uHeight = 0u;
        if (Zenith_CommandLine::ParseWindowSizeArg(sz, uWidth, uHeight))
        {
            x.m_uWindowWidth = uWidth;
            x.m_uWindowHeight = uHeight;
        }
    }

    void ApplyBootProfileDump(Flags& x, const char* szArg)
    {
        x.m_szBootProfileDump = Zenith_CommandLine::ResolveBootProfileDumpArg(szArg, szDEFAULT_BOOT_PROFILE_DUMP);
    }
    void ApplyUnitTestTimings(Flags& x, const char* szArg)
    {
        x.m_szUnitTestTimings = Zenith_CommandLine::ResolveBootProfileDumpArg(szArg, szDEFAULT_UNIT_TEST_TIMINGS);
    }
    void ApplyIndirectCountMode(Flags& x, const char* szArg)
    {
        // The bare form (`--indirect-count-mode` with no '=') and an unknown
        // spelling both fall through to Auto via ResolveIndirectCountModeArg,
        // so a malformed CLI never silently flips the shipping mode.
        const char* pxEq = (szArg != nullptr) ? std::strchr(szArg, '=') : nullptr;
        const char* szValue = (pxEq != nullptr) ? (pxEq + 1) : nullptr;
        x.m_eIndirectCountMode = Zenith_CommandLine::ResolveIndirectCountModeArg(szValue);
    }

    // Scanned in order, first match wins — exactly the if/else-if chain this
    // replaces. The two Prefixed entries match on their own length (19 chars
    // each), and no bare/value flag name starts with either, so ordering
    // within the table carries no hidden coupling.
    //
    // Boot-profiling and unit-test-timing flags are parsed HERE rather than by
    // their consumer because the profiler reads them while allocating the boot
    // capture inside Zenith_Init -- long before the automated-test runner
    // parses anything. The bare form uses the default filename; the "=path"
    // form points it elsewhere (bounded artifact runs give every run a unique
    // path under Build/artifacts).
    //
    // The save-sandbox pair: the runner creates the directory under the
    // artifacts root, writes the ownership marker itself, and passes both the
    // path and the run-id; Zenith_SaveData accepts the root ONLY if the
    // marker's run-id matches. See the sandbox block in Zenith_SaveData.h.
    constexpr FlagSpec axFLAG_SPECS[] =
    {
        { "--automated-test",        FlagArity::Bare,     &ApplyAutomatedTest      },
        { "--automated-tests",       FlagArity::Bare,     &ApplyAutomatedTest      },
        { "--all-automated-tests",   FlagArity::Bare,     &ApplyAutomatedTest      },
        { "--no-imgui-ini",          FlagArity::Bare,     &ApplyNoImGuiIni         },
        { "--screenshot",            FlagArity::Value,    &ApplyScreenshot         },
        { "--screenshot-frame",      FlagArity::Value,    &ApplyScreenshotFrame    },
        { "--shader-debug-o0",       FlagArity::Bare,     &ApplyShaderDebugO0      },
        { "--assets-root",           FlagArity::Value,    &ApplyAssetsRoot         },
        { "--test-save-root",        FlagArity::Value,    &ApplyTestSaveRoot       },
        { "--test-save-run-id",      FlagArity::Value,    &ApplyTestSaveRunId      },
        { "--boot-profile-dump",     FlagArity::Prefixed, &ApplyBootProfileDump    },
        { "--skip-boot-capture",     FlagArity::Bare,     &ApplySkipBootCapture    },
        { "--unit-test-timings",     FlagArity::Prefixed, &ApplyUnitTestTimings    },
        { "--exit-after-unit-tests", FlagArity::Bare,     &ApplyExitAfterUnitTests },
        { "--window-size",           FlagArity::Value,    &ApplyWindowSize         },
        { "--indirect-count-mode",   FlagArity::Prefixed, &ApplyIndirectCountMode  },
    };

    // True when szArg selects xSpec. Prefixed specs match on their own length,
    // reproducing the original strncmp(argv[i], "--boot-profile-dump", 19).
    bool FlagSpecMatches(const FlagSpec& xSpec, const char* szArg)
    {
        return xSpec.m_eArity == FlagArity::Prefixed
            ? std::strncmp(szArg, xSpec.m_szName, std::strlen(xSpec.m_szName)) == 0
            : std::strcmp(szArg, xSpec.m_szName) == 0;
    }
}

namespace Zenith_CommandLine
{
    Flags ParseArgs(int argc, char** argv)
    {
        Flags xFlags;
        if (argv == nullptr) return xFlags;

        for (int i = 1; i < argc; ++i)
        {
            if (argv[i] == nullptr) continue;

            for (const FlagSpec& xSpec : axFLAG_SPECS)
            {
                if (!FlagSpecMatches(xSpec, argv[i])) continue;

                if (xSpec.m_eArity == FlagArity::Value)
                {
                    // A value flag with nothing after it is ignored outright —
                    // matching the original chain, where the `i + 1 < argc`
                    // guard was part of the match condition.
                    if (i + 1 < argc) xSpec.m_pfnApply(xFlags, argv[++i]);
                }
                else
                {
                    xSpec.m_pfnApply(xFlags,
                        xSpec.m_eArity == FlagArity::Prefixed ? argv[i] : nullptr);
                }
                break;
            }
        }
        return xFlags;
    }

    void Parse(int argc, char** argv)
    {
        // Reset state on every call so a test process re-parsing with a
        // different argv set (Tests/Test_T0Harness_RunnerFlagsExist or the
        // CommandLineParse* units) doesn't leak the previous run. Taking the
        // whole struct by value IS the reset — Flags' member initialisers are
        // the no-flag defaults.
        const Flags xFlags = ParseArgs(argc, argv);

        s_bAutomatedTestRun   = xFlags.m_bAutomatedTestRun;
        s_bNoImGuiIni         = xFlags.m_bNoImGuiIni;
        s_bShaderDebugO0      = xFlags.m_bShaderDebugO0;
        s_szScreenshotPath    = xFlags.m_szScreenshotPath;
        s_uScreenshotFrame    = xFlags.m_uScreenshotFrame;
        s_szAssetsRoot        = xFlags.m_szAssetsRoot;
        s_szTestSaveRoot      = xFlags.m_szTestSaveRoot;
        s_szTestSaveRunId     = xFlags.m_szTestSaveRunId;
        s_szBootProfileDump   = xFlags.m_szBootProfileDump;
        s_bSkipBootCapture    = xFlags.m_bSkipBootCapture;
        s_szUnitTestTimings   = xFlags.m_szUnitTestTimings;
        s_bExitAfterUnitTests = xFlags.m_bExitAfterUnitTests;
        s_uWindowWidth        = xFlags.m_uWindowWidth;
        s_uWindowHeight       = xFlags.m_uWindowHeight;
        s_eIndirectCountMode  = xFlags.m_eIndirectCountMode;

        s_bParsed = true;
    }

    bool IsAutomatedTestRun()
    {
        if (!s_bParsed) return false;
        return s_bAutomatedTestRun;
    }

    bool IsImGuiIniDisabled()
    {
        if (!s_bParsed) return false;
        return s_bNoImGuiIni;
    }

    const char* GetScreenshotPath()
    {
        if (!s_bParsed) return nullptr;
        return s_szScreenshotPath;
    }

    u_int GetScreenshotFrame()
    {
        return s_uScreenshotFrame;
    }

    bool IsShaderDebugO0()
    {
        if (!s_bParsed) return false;
        return s_bShaderDebugO0;
    }

    const char* GetAssetsRoot()
    {
        if (!s_bParsed) return nullptr;
        return s_szAssetsRoot;
    }

    const char* GetTestSaveRoot()
    {
        if (!s_bParsed) return nullptr;
        return s_szTestSaveRoot;
    }

    const char* GetTestSaveRunId()
    {
        if (!s_bParsed) return nullptr;
        return s_szTestSaveRunId;
    }

    const char* GetBootProfileDumpPath()
    {
        if (!s_bParsed) return nullptr;
        return s_szBootProfileDump;
    }

    bool IsBootCaptureSkipped()
    {
        if (!s_bParsed) return false;   // Android never parses: capture stays ON
        return s_bSkipBootCapture;
    }

    const char* GetUnitTestTimingsPath()
    {
        if (!s_bParsed) return nullptr;
        return s_szUnitTestTimings;
    }

    bool IsExitAfterUnitTestsRequested()
    {
        if (!s_bParsed) return false;
        return s_bExitAfterUnitTests;
    }

    bool ParseWindowSizeArg(const char* szValue, u_int& uWidthOut, u_int& uHeightOut)
    {
        if (szValue == nullptr) return false;
        char* szEnd = nullptr;
        const unsigned long ulWidth = std::strtoul(szValue, &szEnd, 10);
        if (szEnd == szValue || szEnd == nullptr) return false;
        if (*szEnd != 'x' && *szEnd != 'X' && *szEnd != '*') return false;
        const char* szHeightStart = szEnd + 1;
        const unsigned long ulHeight = std::strtoul(szHeightStart, &szEnd, 10);
        if (szEnd == szHeightStart || *szEnd != '\0') return false;
        constexpr unsigned long ulMAX_DIMENSION = 16384ul;
        if (ulWidth == 0ul || ulHeight == 0ul || ulWidth > ulMAX_DIMENSION || ulHeight > ulMAX_DIMENSION) return false;
        uWidthOut = static_cast<u_int>(ulWidth);
        uHeightOut = static_cast<u_int>(ulHeight);
        return true;
    }

    bool GetWindowSizeOverride(u_int& uWidthOut, u_int& uHeightOut)
    {
        if (!s_bParsed || s_uWindowWidth == 0u || s_uWindowHeight == 0u) return false;
        uWidthOut = s_uWindowWidth;
        uHeightOut = s_uWindowHeight;
        return true;
    }

    Zenith_IndirectCountMode GetIndirectCountMode()
    {
        // Android never calls Parse, so this returns Auto there (the device's
        // raw capability selects the effective mode). The desktop CLI is boot-
        // time immutable: a process either parses once at launch or never.
        if (!s_bParsed) return Zenith_IndirectCountMode::Auto;
        return s_eIndirectCountMode;
    }

    Zenith_IndirectCountMode ResolveIndirectCountModeArg(const char* szValue,
        Zenith_IndirectCountMode eDefault)
    {
        if (szValue == nullptr || szValue[0] == '\0')
            return eDefault;
        if (std::strcmp(szValue, "auto") == 0)
            return Zenith_IndirectCountMode::Auto;
        if (std::strcmp(szValue, "native") == 0)
            return Zenith_IndirectCountMode::Native;
        if (std::strcmp(szValue, "padded") == 0)
            return Zenith_IndirectCountMode::Padded;
        if (std::strcmp(szValue, "single") == 0)
            return Zenith_IndirectCountMode::Single;
        return eDefault;
    }

    const char* ResolveBootProfileDumpArg(const char* szArg, const char* szDefaultPath)
    {
        if (szArg == nullptr) return szDefaultPath;
        const char* pxEq = std::strchr(szArg, '=');
        // A trailing "=" with nothing after it is the bare form, not an empty path.
        return (pxEq != nullptr && pxEq[1] != '\0') ? (pxEq + 1) : szDefaultPath;
    }

    std::string ResolveUnderAssetsRoot(const std::string& strBakedDir, const char* szOverrideRoot, const std::string& strRelativeUnderRoot)
    {
        if (szOverrideRoot == nullptr || szOverrideRoot[0] == '\0')
        {
            return strBakedDir;
        }
        std::string strRoot(szOverrideRoot);
        // Trim trailing separators off the root so the join never doubles up
        // (`run.bat` passes "%~dp0", which ends in a backslash).
        while (!strRoot.empty() && (strRoot.back() == '/' || strRoot.back() == '\\'))
        {
            strRoot.pop_back();
        }
        return strRoot + "/" + strRelativeUnderRoot;
    }
}

#include "Core/Zenith_CommandLine.Tests.inl"
