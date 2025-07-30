#pragma once
#include <cstdint>

void* operator new(size_t ullSize);
void* operator new(size_t ullSize, const std::nothrow_t&);
void* operator new(size_t ullSize, std::align_val_t);
void* operator new(size_t ullSize, std::align_val_t, const std::nothrow_t&);
void* operator new(size_t ullSize, const int32_t iLine, const char* szFile);

void* operator new[](size_t ullSize);
void* operator new[](size_t ullSize, const std::nothrow_t&);
void* operator new[](size_t ullSize, std::align_val_t);
void* operator new[](size_t ullSize, std::align_val_t, const std::nothrow_t&);
void* operator new[](size_t ullSize, const int32_t iLine, const char* szFile);

void operator delete(void* p);
void operator delete(void* p, const std::nothrow_t&);
void operator delete[](void* p);
void operator delete[](void* p, const std::nothrow_t&);

class Zenith_MemoryManagement
{
public:
	static void Initialise();
	static void EndFrame();

	static void* Allocate(size_t ullSize);
	static void Deallocate(void* p);
};
