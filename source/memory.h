// TweaksV: carried over from RageOpenV (src/utils/memory.h) by Chiheb-Bacha, based on ClosedIV by
// martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV: namespace, the shared scanner, safety fixes (B-ROV-09/20), the code
// writers replaced by TweaksVPatch.h, dead code removed. Every change is marked "TweaksV:".
#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include "MinHook.h"		// TweaksV: from vendor/MinHook (was vendor/minhook/include/MinHook.h)
#include "log.h"
#include <string>			// TweaksV
#include "MemScanner.h"		// TweaksV: the shared scanner

namespace RageOpenV // TweaksV: one namespace per original plugin
{

class memory {
public:
	uintptr_t address;
	memory(memory& other) : address(other.address) {}
	memory(uintptr_t offset) : address(offset) {} // TweaksV: useBase removed - never true

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

	// TweaksV: base() removed - only the never-used useBase read it. virtual_mem(), get_virtual_mem()
	// and their trampoline area removed - the calls are redirected by TweaksV::RedirectCalls
	// (TweaksVPatch.h), whose stubs are never writable and executable at once (upstream's area was
	// read/write/execute)

	// TweaksV: returns false if MinHook could not be initialised
	inline static bool init()
	{
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

	// TweaksV: rip(), nop(), make_jmp(), make_call(), set_call() and put() removed - TweaksV writes
	// game code only through TweaksVPatch.h (WriteCode, RedirectCalls), which keeps every write all
	// or nothing, gives each page its own protection back and honours DRY_RUN and DIAGNOSTIC_MODE.
	// ret(), call() and both make_jmp_ret() overloads were never used (Appendix G).

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
		return memory((uintptr_t)found);
	}

	// TweaksV: scoped_unlock removed with the code writers that used it
};

} // namespace RageOpenV
