#include "Zenith.h"

#include "Core/Zenith_CommandLine.h"

#include <cstring>
#include <cstdlib>

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

    // --boot-profile-dump with no "=path" writes here. A file-scope literal, not a
    // buffer: the accessor hands back a process-lifetime pointer exactly like the
    // argv-derived paths beside it.
    const char* const szDEFAULT_BOOT_PROFILE_DUMP = "zenith_boot_profile_dump.txt";
    const char* const szDEFAULT_UNIT_TEST_TIMINGS = "zenith_unit_test_timings.txt";
}

namespace Zenith_CommandLine
{
    void Parse(int argc, char** argv)
    {
        // Reset state on every call so a test process re-parsing with a
        // different argv set (Tests/Test_T0Harness_RunnerFlagsExist or
        // future unit tests of this parser) doesn't leak the previous run.
        s_bAutomatedTestRun = false;
        s_bNoImGuiIni       = false;
        s_bShaderDebugO0    = false;
        s_szScreenshotPath  = nullptr;
        s_uScreenshotFrame  = 120;
        s_szAssetsRoot      = nullptr;
        s_szTestSaveRoot    = nullptr;
        s_szTestSaveRunId   = nullptr;
        s_szBootProfileDump = nullptr;
        s_bSkipBootCapture  = false;
        s_szUnitTestTimings = nullptr;

        if (argv != nullptr)
        {
            for (int i = 1; i < argc; ++i)
            {
                if (argv[i] == nullptr) continue;
                // Every harness selection flag must be listed here. Consumers
                // of IsAutomatedTestRun() swap production state for test state
                // (editor imgui.ini suppression, DP's MetaSave _Test slot, ZM's
                // save-slot aliasing) -- a flag missing from this set silently
                // runs an automated batch against PRODUCTION save data.
                if (std::strcmp(argv[i], "--automated-test") == 0
                      || std::strcmp(argv[i], "--automated-tests") == 0
                      || std::strcmp(argv[i], "--all-automated-tests") == 0)
                {
                    s_bAutomatedTestRun = true;
                }
                else if (std::strcmp(argv[i], "--no-imgui-ini") == 0)
                {
                    s_bNoImGuiIni = true;
                }
                else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
                {
                    s_szScreenshotPath = argv[++i];
                }
                else if (std::strcmp(argv[i], "--screenshot-frame") == 0 && i + 1 < argc)
                {
                    s_uScreenshotFrame = static_cast<u_int>(std::atoi(argv[++i]));
                }
                else if (std::strcmp(argv[i], "--shader-debug-o0") == 0)
                {
                    s_bShaderDebugO0 = true;
                }
                else if (std::strcmp(argv[i], "--assets-root") == 0 && i + 1 < argc)
                {
                    s_szAssetsRoot = argv[++i];
                }
                // Automated-test save sandbox. The runner creates the directory
                // under the artifacts root, writes the ownership marker itself,
                // and passes both the path and the run-id; Zenith_SaveData
                // accepts the root ONLY if the marker's run-id matches. See the
                // sandbox block in Zenith_SaveData.h.
                else if (std::strcmp(argv[i], "--test-save-root") == 0 && i + 1 < argc)
                {
                    s_szTestSaveRoot = argv[++i];
                }
                else if (std::strcmp(argv[i], "--test-save-run-id") == 0 && i + 1 < argc)
                {
                    s_szTestSaveRunId = argv[++i];
                }
                // Boot profiling. Parsed HERE (not by a consumer) because the profiler
                // reads both while allocating the boot capture, which happens inside
                // Zenith_Init -- long before the automated-test runner parses anything.
                // `--boot-profile-dump` alone uses the default filename; the "=path"
                // form points it somewhere else (the bounded artifact runs give every
                // run a unique path under Build/artifacts).
                else if (std::strncmp(argv[i], "--boot-profile-dump", 19) == 0)
                {
                    s_szBootProfileDump = ResolveBootProfileDumpArg(argv[i], szDEFAULT_BOOT_PROFILE_DUMP);
                }
                else if (std::strcmp(argv[i], "--skip-boot-capture") == 0)
                {
                    s_bSkipBootCapture = true;
                }
                // Per-test timings. Parsed here for the same reason as the boot
                // flags: RunAllTests fires inside Zenith_Init, long before the
                // automated-test runner parses anything.
                else if (std::strncmp(argv[i], "--unit-test-timings", 19) == 0)
                {
                    s_szUnitTestTimings = ResolveBootProfileDumpArg(argv[i], szDEFAULT_UNIT_TEST_TIMINGS);
                }
            }
        }

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
