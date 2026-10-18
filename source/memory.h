// TweaksV: carried over from RageOpenV (src/utils/memory.h) by Chiheb-Bacha, based on ClosedIV by
// martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV: namespace, the shared scanner and allocator, safety fixes
// (B-ROV-02/08/09/20), dead code removed. Every change is marked "TweaksV:".
#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include "MinHook.h"		// TweaksV: from vendor/MinHook (was vendor/minhook/include/MinHook.h)
#include "log.h"
#include <cstring>			// TweaksV: memcpy, memset
#include <string>			// TweaksV
#include "MemScanner.h"		// TweaksV: the shared scanner
#include "StubMemory.h"		// TweaksV: the shared AllocateStubMemory

namespace RageOpenV // TweaksV: one namespace per original plugin
{

class memory {
public:
	uintptr_t address;
	memory(memory& other) : address(other.address) {}
	memory(uintptr_t offset, bool useBase = false) : address(useBase ? base() + offset : offset) {}

	struct InitFuncs
	{
		std::function<void()> fn;
		static std::vector<InitFuncs>& funcs()
		{
			static std::vector<InitFuncs> _funcs;
			return _funcs;
		}

		static void run()
		{
			for (auto& cb : funcs())
			{
				// TweaksV: one group's failure must not stop the others, and nothing may throw
				// back into the game (plan 5.8)
				try
				{
					cb.fn();
				}
				catch (...)
				{
					logger::write("error", " An init function threw an exception - its hooks are skipped.");
				}
			}
		}

		InitFuncs(const std::function<void()>& fn) : fn(fn)
		{
			funcs().push_back(*this);
		}
	};

	inline static uintptr_t& base()
	{
		static uintptr_t _base;
		return _base;
	}

	inline static memory& virtual_mem()
	{
		static memory _virtualmem(0, false);
		return _virtualmem;
	}

	// TweaksV: the size of the trampoline area - one page
	static constexpr size_t virtual_mem_size = 0x1000;

	inline static memory get_virtual_mem(size_t size)
	{
		static memory current = virtual_mem();

		while (current.address % 16) {
			current.address += 1;
		}

		// TweaksV: nothing is handed out without an area or past its end (was unchecked - B-ROV-08)
		if (current.address == 0 || current.address + size > virtual_mem().address + virtual_mem_size)
			return memory(0, false);

		memory res(current);
		current.address += size;
		return res;
	}

	// TweaksV: returns false if MinHook could not be initialised
	inline static bool init()
	{
		memory::base() = (uintptr_t)GetModuleHandle(NULL);
		// TweaksV: the trampoline area comes from the shared AllocateStubMemory - a free page near
		// the game, as a call rel32 needs - and is checked; upstream asked for the fixed address
		// exeBase+0x20000000 and did not check the result (B-ROV-08)
		memory::virtual_mem() = memory((uintptr_t)AllocateStubMemory(virtual_mem_size), false);
		if (memory::virtual_mem().address == 0)
			logger::write("error", " The trampoline area could not be allocated near the game.");

		auto status = MH_Initialize();
		if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) { // TweaksV: + already initialised
			logger::write("error", "MH_Initialize failed %d", (int)status); // TweaksV: "error" - was "info"
			return false;
		}
		return true;
	}

	template <typename T>
	inline T as()
	{
		return (T)(address);
	}

	template <typename T>
	inline memory add(T offset)
	{
		return memory(address ? address + offset : 0); // TweaksV: a pattern miss (0) stays 0
	}

	inline memory rip()
	{
		if (!address) // TweaksV: a pattern miss (0) stays 0 instead of reading address 0 (B-ROV-01)
			return memory(0, false);
		return memory(address + *(int32_t*)address + 4);
	}

	// TweaksV: returns false, with nothing written, if the code could not be made writable
	inline bool nop(size_t len)
	{
		scoped_unlock lock(address, len);
		if (!lock.ok)
			return false;
		memset((void*)address, 0x90, len);
		return true;
	}

	// TweaksV: ret() removed - never used (Appendix G)

	// TweaksV: returns false if any part could not be written
	inline bool make_jmp(uintptr_t func)
	{
		return memory(address, false).put<uint16_t>(0xB848) &&
			memory(address + 2, false).put<uintptr_t>(func) &&
			memory(address + 10, false).put<uint16_t>(0xE0FF);
	}

	// TweaksV: both make_jmp_ret() overloads removed - only set_call(..., true) used them, and
	// nothing called that (Appendix G)

	// TweaksV: returns false, with nothing written, if func is out of rel32 range
	inline bool make_call(uintptr_t func)
	{
		const intptr_t distance = (intptr_t)func - (intptr_t)address - 5;
		if (distance < INT32_MIN || distance > INT32_MAX)
			return false;
		return put<uint8_t>(0xE8) &&
			memory(address + 1, false).put(int32_t(func - address - 5));
	}

	// TweaksV: a template, so any hook function can be passed; returns false if no trampoline
	// could be made or the call could not be written. The unused "ret" parameter is gone with
	// make_jmp_ret.
	template <typename F>
	inline bool set_call(F* func)
	{
		memory jmpMem = get_virtual_mem(12);
		if (jmpMem.address == 0) // TweaksV: no trampoline area (B-ROV-08)
			return false;

		if (!jmpMem.make_jmp((uintptr_t)func))
			return false;

		return make_call(jmpMem.address);
	}

	// TweaksV: returns false, with nothing written, if the code could not be made writable
	template<typename T>
	inline bool put(const T& value)
	{
		scoped_unlock lock(address, sizeof(T));
		if (!lock.ok)
			return false;
		memcpy((void*)address, &value, sizeof(T));
		return true;
	}

	// TweaksV: call() removed - never used (Appendix G)

	// TweaksV: returns false if the hook could not be created. It is queued, not enabled: each
	// group enables all of its hooks at once with apply_hooks(), instead of every hook freezing
	// all threads with MH_EnableHook(MH_ALL_HOOKS) (B-ROV-09)
	template<class T>
	bool hook(T* target, T** orig = nullptr)
	{
		auto status = MH_CreateHook((void*)address, (void*)target, (void**)orig);
		if (status != MH_OK) {
			logger::write("error", "MH_CreateHook failed %d", (int)status); // TweaksV: "error" - was "info"
			return false;
		}

		status = MH_QueueEnableHook((void*)address);
		if (status != MH_OK) {
			logger::write("error", "MH_QueueEnableHook failed %d", (int)status);
			MH_RemoveHook((void*)address);
			return false;
		}
		return true;
	}

	// TweaksV: enables every queued hook at once (B-ROV-09)
	static bool apply_hooks()
	{
		auto status = MH_ApplyQueued();
		if (status != MH_OK) {
			logger::write("error", "MH_ApplyQueued failed %d", (int)status);
			return false;
		}
		return true;
	}

	// TweaksV: removes a hook made by hook() again (disabling it first if needed) - for a group
	// that cannot be completed
	void unhook()
	{
		MH_RemoveHook((void*)address);
	}

	template<typename RetType, typename... Args>
	class func
	{
		using FuncType = RetType(__fastcall*)(Args...);
		FuncType mainFunc;

		std::string sig;
	public:
		// TweaksV: no longer registers an init function of its own - the init function of the
		// group that uses it calls run() after checking its switch, and skips the group if the
		// function was not found (B-ROV-01)
		func(const char* _sig) : sig(_sig), mainFunc(nullptr)
		{
		}

		// TweaksV: returns whether the function was found
		bool run()
		{
			mainFunc = memory::scan(sig.c_str()).as<FuncType>();
			return mainFunc != nullptr;
		}

		// TweaksV: the function's address, for checks and the log
		void* get() const
		{
			return (void*)mainFunc;
		}

		RetType operator()(Args...args)
		{
			return this->mainFunc(args...);
		}
	};

	// TweaksV: HookIAT() removed - import hooking cannot work on GTA V Enhanced (its imports are
	// obfuscated, as the note in main.cpp says) and nothing called it (F-ROV-9)

	// TweaksV: + pTarget, the hooked function, for disabling the hook again later
	static bool HookApi(const wchar_t* module, const char* funcName, PVOID pNewFunc, PVOID* pOldFunc, PVOID* pTarget = nullptr)
	{
		LPVOID target = nullptr; // TweaksV: so that only this hook is enabled below
		auto status = MH_CreateHookApiEx(module, funcName, pNewFunc, pOldFunc, &target);
		if (status != MH_OK) {
			logger::write("error", " HookApi failed for %s: %d", funcName, (int)status); // TweaksV: "error"
			return false;
		}
		status = MH_EnableHook(target); // TweaksV: this hook only (was MH_ALL_HOOKS)
		if (status != MH_OK) {
			logger::write("error", " MH_EnableHook failed: %d", (int)status); // TweaksV: "error"
			MH_RemoveHook(target); // TweaksV
			return false;
		}
		if (pTarget) // TweaksV
			*pTarget = target;
		return true;
	}

	static memory scan(const char* signature, bool ignoreFail = false)
	{
		// TweaksV: delegates to the shared MemScanner, which searches the same game
		// image, logs every miss and adds the DIAGNOSTIC_MODE output; a miss still gives address 0
		auto found = MemScanner::FindPatternBmh(signature);
		if (!found && !ignoreFail) {
			logger::write("info", " !! Pattern %s not found!", signature);
		}
		return memory((uintptr_t)found, false);
	}

private:
	struct scoped_unlock
	{
		DWORD rights;
		size_t len;
		void* addr;
		bool ok; // TweaksV: false if the protection could not be changed - then nothing is written

		scoped_unlock(uint64_t _addr, size_t _len) :addr((void*)_addr), len(_len)
		{
			ok = _addr != 0 && VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &rights) != FALSE; // TweaksV: checked
		}

		~scoped_unlock()
		{
			if (!ok) // TweaksV
				return;
			// TweaksV: Windows rejects a NULL old-protect pointer, so upstream never restored the
			// protection (B-ROV-02); the instruction cache is flushed after the write (plan 5.9 R3)
			DWORD unused = 0;
			VirtualProtect(addr, len, rights, &unused);
			FlushInstructionCache(GetCurrentProcess(), addr, len);
		}
	};
};

} // namespace RageOpenV
