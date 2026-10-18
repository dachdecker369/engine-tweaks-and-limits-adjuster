// TweaksV: FindPrevFreeRegion and AllocateStubMemory, carried over from
// ModkitLimitAdjusterEnhanced (Game.cpp:174-240) by Chiheb-Bacha - the same code is in
// PackfileLimitAdjusterEnhanced's Game.cpp - https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced
// (GPL-3.0). Modified for TweaksV: one shared copy; AllocateStubMemory takes the protection the
// memory gets - read/write data for the moved arrays, read/write for the call stubs, which
// TweaksV::RedirectCalls makes read+execute once they are written. Nothing gets read/write/execute memory.
#pragma once
#include <windows.h>

LPVOID FindPrevFreeRegion(LPVOID pAddress, LPVOID pMinAddr, DWORD dwAllocationGranularity);
void* AllocateStubMemory(size_t size, DWORD protect); // TweaksV: + protect (was always PAGE_EXECUTE_READWRITE)
