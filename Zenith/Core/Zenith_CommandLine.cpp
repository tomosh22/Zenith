#include "Zenith.h"

#include "Core/Zenith_CommandLine.h"

#include <cstring>

namespace
{
    bool s_bParsed   = false;
    bool s_bHeadless = false;
}

namespace Zenith_CommandLine
{
    void Parse(int argc, char** argv)
    {
        // Reset state on every call so a test process re-parsing with a
        // different argv set (Tests/Test_T0Harness_RunnerFlagsExist or
        // future unit tests of this parser) doesn't leak the previous run.
        s_bHeadless = false;

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
}
