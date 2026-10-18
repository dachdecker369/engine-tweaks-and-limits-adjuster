// TweaksV: carried over from ModkitLimitAdjusterEnhanced (Game.cpp:174-240) by Chiheb-Bacha -
// identical in PackfileLimitAdjusterEnhanced's Game.cpp -
// https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0). The search follows
// MinHook's buffer.c (BSD-2-Clause, see THIRD_PARTY_NOTICES.md).
// Modified for TweaksV; every change is marked "TweaksV:".
#include "StubMemory.h"
#include <cstdint>

LPVOID FindPrevFreeRegion(LPVOID pAddress, LPVOID pMinAddr, DWORD dwAllocationGranularity)
{
	ULONG_PTR tryAddr = (ULONG_PTR)pAddress;

	// Round down to the next allocation granularity.
	tryAddr -= tryAddr % dwAllocationGranularity;

	// Start from the previous allocation granularity multiply.
	tryAddr -= dwAllocationGranularity;

	while (tryAddr >= (ULONG_PTR)pMinAddr)
	{
		MEMORY_BASIC_INFORMATION mbi;
		if (VirtualQuery((LPVOID)tryAddr, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) ==
			0)
			break;

		if (mbi.State == MEM_FREE)
			return (LPVOID)tryAddr;

		if ((ULONG_PTR)mbi.AllocationBase < dwAllocationGranularity)
			break;

		tryAddr = (ULONG_PTR)mbi.AllocationBase - dwAllocationGranularity;
	}

	return NULL;
}

void* AllocateStubMemory(size_t size, DWORD protect) // TweaksV: + protect (was always PAGE_EXECUTE_READWRITE)
{
	// Max range for seeking a memory block. (= 1024MB)
	const uint64_t MAX_MEMORY_RANGE = 0x40000000;

	void* origin = GetModuleHandle(NULL);

	ULONG_PTR minAddr;
	ULONG_PTR maxAddr;

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	minAddr = (ULONG_PTR)si.lpMinimumApplicationAddress;
	maxAddr = (ULONG_PTR)si.lpMaximumApplicationAddress;

	if ((ULONG_PTR)origin > MAX_MEMORY_RANGE &&
		minAddr < (ULONG_PTR)origin - MAX_MEMORY_RANGE)
		minAddr = (ULONG_PTR)origin - MAX_MEMORY_RANGE;

	if (maxAddr > (ULONG_PTR)origin + MAX_MEMORY_RANGE)
		maxAddr = (ULONG_PTR)origin + MAX_MEMORY_RANGE;

	LPVOID pAlloc = origin;

	void* stub = nullptr;
	while ((ULONG_PTR)pAlloc >= minAddr)
	{
		pAlloc = FindPrevFreeRegion(pAlloc, (LPVOID)minAddr, si.dwAllocationGranularity);
		if (pAlloc == NULL)
			break;

		stub = VirtualAlloc(pAlloc, size, MEM_COMMIT | MEM_RESERVE, protect); // TweaksV: was PAGE_EXECUTE_READWRITE
		if (stub != NULL)
			break;
	}

	return stub;
}
