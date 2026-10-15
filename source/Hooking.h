// https://github.com/CamxxCore/PauseMenuHelper/blob/master/PauseMenuHelper/Hooking.h
// TweaksV: carried over from PackfileLimitAdjusterEnhanced (Hooking.h) by Chiheb-Bacha,
// https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:".

#pragma once

#include <windows.h>
#include <inttypes.h>

namespace PackfileLimitAdjusterEnhanced // TweaksV: one namespace per original plugin
{

template <typename T>
class Hook
{
public:
    Hook(void* addr, T func) : address((BYTE*)addr), fn(func) {}
    virtual ~Hook() {}
    virtual void remove() = 0;

    BYTE* address;
    T fn;
};

template <typename T>
class CallHook : public Hook<T>
{
public:
    CallHook(void* addr, T func) : Hook<T>(addr, func) {}
    ~CallHook() { remove(); }

    virtual void remove()
    {
        // TweaksV: same protection fix as SetCall below (B-PACK-01). TweaksV never removes its hook -
        // the CallHook is kept for the whole session.
        DWORD oldProtect;
        if (!VirtualProtect(this->address, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
            return;

        // restore original call instruction
        *reinterpret_cast<int32_t*>(this->address + 1) =
            static_cast<int32_t>((intptr_t)this->fn - (intptr_t)this->address - 5);

        VirtualProtect(this->address, 5, oldProtect, &oldProtect);         // TweaksV
        FlushInstructionCache(GetCurrentProcess(), this->address, 5);     // TweaksV
    }
};

class HookManager
{
public:
    // TweaksV: returns nullptr, with nothing written, if the site is not a call (E8), no stub could
    // be allocated, the stub is out of rel32 range or the code cannot be made writable (B-PACK-01)
    template <typename T>
    static CallHook<T>* SetCall(char* address, T func)
    {
        if (!address || *(BYTE*)address != 0xE8) // TweaksV: only a call may be redirected (plan 5.9 R2)
            return nullptr;

        // Get original function target
        T target = reinterpret_cast<T>(
            *reinterpret_cast<int32_t*>(address + 1) + (intptr_t)(address + 5)
            );

        // Allocate stub
        auto pFunc = AllocateFunctionStub(GetModuleHandle(nullptr), (PVOID)func, 0);
        if (!pFunc) // TweaksV: was not checked (plan 5.9 R5)
            return nullptr;
        const intptr_t distance = (intptr_t)pFunc - (intptr_t)address - 5; // TweaksV: must fit the rel32
        if (distance < INT32_MIN || distance > INT32_MAX)
            return nullptr;

        // TweaksV: upstream wrote into the game code without changing its protection - make it
        // writable first, restore the real old protection after (B-PACK-01, plan 5.9 R3)
        DWORD oldProtect;
        if (!VirtualProtect(address, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
            return nullptr;

        // Write the call opcode
        *(BYTE*)address = 0xE8;

        // Write relative offset
        *reinterpret_cast<int32_t*>(address + 1) =
            (int32_t)((intptr_t)pFunc - (intptr_t)address - 5);

        VirtualProtect(address, 5, oldProtect, &oldProtect);               // TweaksV
        FlushInstructionCache(GetCurrentProcess(), address, 5);           // TweaksV

        return new CallHook<T>(address, target);
    }

    static PVOID AllocateFunctionStub(PVOID origin, PVOID function, int type);
    // TweaksV: "static LPVOID FindPrevFreeRegion(...)" removed - a third copy of the shared
    // function; AllocateFunctionStub takes its page from the shared AllocateStubMemory (plan 2.3)
};

} // namespace PackfileLimitAdjusterEnhanced
