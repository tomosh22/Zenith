#include "Zenith.h"

#include "Core/Zenith_CommandLine.h"

#include <cstring>
#include <cstdlib>

namespace
{
    bool        s_bParsed           = false;
    bool        s_bHeadless         = false;
    bool        s_bAutomatedTestRun = false;
    bool        s_bNoImGuiIni       = false;
    bool        s_bShaderDebugO0    = false;
    bool        s_bSkipToolExports  = false;
    const char* s_szScreenshotPath  = nullptr;
    u_int       s_uScreenshotFrame  = 120;
    const char* s_szAssetsRoot      = nullptr;
}

namespace Zenith_CommandLine
{
    void Parse(int argc, char** argv)
    {
        // Reset state on every call so a test process re-parsing with a
        // different argv set (Tests/Test_T0Harness_RunnerFlagsExist or
        // future unit tests of this parser) doesn't leak the previous run.
        s_bHeadless         = false;
        s_bAutomatedTestRun = false;
        s_bNoImGuiIni       = false;
        s_bShaderDebugO0    = false;
        s_bSkipToolExports  = false;
        s_szScreenshotPath  = nullptr;
        s_uScreenshotFrame  = 120;
        s_szAssetsRoot      = nullptr;

        if (argv != nullptr)
        {
            for (int i = 1; i < argc; ++i)
            {
                if (argv[i] == nullptr) continue;
                if (std::strcmp(argv[i], "--headless") == 0)
                {
                    s_bHeadless = true;
                    // Don't break; we want to drain the rest of argv for
                    // future flags (none today, but cheap to keep the loop
                    // exhaustive).
                }
                else if (std::strcmp(argv[i], "--automated-test") == 0
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
                else if (std::strcmp(argv[i], "--skip-tool-exports") == 0)
                {
                    s_bSkipToolExports = true;
                }
            }
        }

        s_bParsed = true;
    }

    bool IsHeadless()
    {
        // Tolerate accidental pre-Parse access: report not-headless rather
        // than asserting. Lets static initializers that incidentally call
        // this stay safe before main() has parsed argv.
        if (!s_bParsed) return false;
        return s_bHeadless;
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

    bool IsToolExportSkipped()
    {
        if (!s_bParsed) return false;
        return s_bSkipToolExports;
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
