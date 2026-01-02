#pragma once

// Platform-agnostic debug break
// Declaration only - implementation in platform-specific .cpp files
// Windows: Zenith/Windows/Zenith_Windows_DebugBreak.cpp
// Android: Zenith/Android/Zenith_Android_DebugBreak.cpp

void Zenith_DebugBreak();
