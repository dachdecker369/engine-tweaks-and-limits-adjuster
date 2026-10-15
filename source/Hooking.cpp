// TweaksV: carried over from PackfileLimitAdjusterEnhanced (Hooking.cpp) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced (GPL-3.0); the notice below
// covers the code it is based on. Modified for TweaksV; every change is marked "TweaksV:".
#include <windows.h>		// TweaksV: was #include "pch.h"
#include <cstdint>			// TweaksV

/* Copyright(c) 2014 Bas Timmer / NTAuthority et al.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "Hooking.h"
#include "StubMemory.h"		// TweaksV: the shared AllocateStubMemory

namespace PackfileLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{

// Size of each memory block. (= page size of VirtualAlloc)
const uint64_t MEMORY_BLOCK_SIZE = 0x1000;

// TweaksV: MAX_MEMORY_RANGE removed - the search for a block near the game is the shared
// AllocateStubMemory's job now, which uses the same 1024 MB range

PVOID HookManager::AllocateFunctionStub(PVOID origin, PVOID function, int type)
{
	static void* g_currentStub = nullptr;

	if (!g_currentStub) {
		// TweaksV: was a copy of AllocateStubMemory's search (with HookManager's own
		// FindPrevFreeRegion); the shared function searches near the game module, which is the
		// origin every caller passes (plan 2.3)
		(void)origin;
		g_currentStub = AllocateStubMemory(MEMORY_BLOCK_SIZE);
	}
	if (!g_currentStub)
		return nullptr;

	char* code = (char*)g_currentStub;

	*(uint8_t*)code = 0x48;
	*(uint8_t*)(code + 1) = 0xb8 | type;

	*(uint64_t*)(code + 2) = (uint64_t)function;

	*(uint16_t*)(code + 10) = 0xE0FF | (type << 8);

	*(uint32_t*)(code + 12) = 0xCCCCCCCC;

	FlushInstructionCache(GetCurrentProcess(), code, 0x10); // TweaksV: new code (plan 5.9 R3)

	g_currentStub = (void*)((uint64_t)g_currentStub + 0x10);

	return code;
}

// TweaksV: HookManager::FindPrevFreeRegion removed - the third copy of the same function; the
// shared one is in StubMemory.cpp (plan 2.3)

} // namespace PackfileLimitAdjusterEnhanced
