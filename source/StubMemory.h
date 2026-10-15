// TweaksV: FindPrevFreeRegion and AllocateStubMemory, carried over from
// ModkitLimitAdjusterEnhanced (Game.cpp:174-240) by Chiheb-Bacha - the same code is in
// PackfileLimitAdjusterEnhanced's Game.cpp - https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced
// (GPL-3.0). Modified for TweaksV: one shared copy; AllocateStubMemory gained an
// optional protect parameter (relocated arrays are read/write data, never RWX - plan 5.9 R4).
#pragma once
#include <windows.h>

LPVOID FindPrevFreeRegion(LPVOID pAddress, LPVOID pMinAddr, DWORD dwAllocationGranularity);
void* AllocateStubMemory(size_t size, DWORD protect = PAGE_EXECUTE_READWRITE); // TweaksV: + protect
