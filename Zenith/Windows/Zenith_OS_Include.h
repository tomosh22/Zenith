#pragma once

#include "Windows/Zenith_Windows_Window.h"
#include "Windows/Multithreading/Zenith_Windows_Multithreading.h"
#include "Windows/Callstack/Zenith_Windows_Callstack.h"

#define Zenith_Mutex Zenith_Windows_Mutex
#define Zenith_Mutex_NoProfiling Zenith_Windows_Mutex_T<false>
#define Zenith_Semaphore Zenith_Windows_Semaphore