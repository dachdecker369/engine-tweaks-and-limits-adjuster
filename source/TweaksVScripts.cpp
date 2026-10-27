// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the game's script programs (see TweaksVScripts.h).
//
// The hook is the call at +3 of Y-E-H1 in a script program's resource constructor ("mov rcx, r12; call"), to
// the game's function that resolves the program's natives: it takes the program and returns a byte, 1 when
// every native was found, which the constructor ignores. Before the call the native table holds the natives'
// 64-bit hashes; after it, each entry holds its handler's address. The call goes through a call stub to
// ResolveHook, which hands the program to every user before and after it calls the game's function, keeps
// the game's last error around their work, lets no exception out and returns the game's byte. Y-E-N1 and
// Y-E-I1 pin three of a program's fields: they must be found too, or the layout the users rely on cannot be
// trusted on this game build.
// Written for TweaksV from the facts in PATTERNS.md; no code of another project is used.

#include "TweaksVScripts.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <vector>

#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVPatterns.h"
#include "TweaksVScanner.h"
#include "TweaksVSettings.h"

namespace TweaksV
{
	namespace Scripts
	{
		namespace
		{
			constexpr const GamePattern& kHookPattern = Pattern("Y-E-H1");			// the call, at +3
			constexpr const GamePattern& kNativeLoopPattern = Pattern("Y-E-N1");	// the code length's and the native table's offsets
			constexpr const GamePattern& kInterpreterPattern = Pattern("Y-E-I1");	// the code page table's offset
			constexpr size_t kCallOffset = 3;
			constexpr size_t kCodeLengthAt = 4;		// in Y-E-N1's match: "mov ecx, [r12+0x1C]"
			constexpr size_t kNativesAt = 11;		// in Y-E-N1's match: "mov rdx, [r12+0x40]"
			constexpr size_t kCodePagesAt = 3;		// in Y-E-I1's match: "mov r10, [r8+0x10]"
			static_assert(PatternByte(kHookPattern, kCallOffset) == 0xE8, "TweaksV: Y-E-H1 holds the call at +3");
			static_assert(PatternByte(kNativeLoopPattern, kCodeLengthAt) == kCodeLengthOffset, "TweaksV: Y-E-N1 pins the code length's offset");
			static_assert(PatternByte(kNativeLoopPattern, kNativesAt) == kNativesOffset, "TweaksV: Y-E-N1 pins the native table's offset");
			static_assert(PatternByte(kInterpreterPattern, kCodePagesAt) == kCodePagesOffset, "TweaksV: Y-E-I1 pins the code page table's offset");

			constexpr const char* kWhat = "natives' resolution call";
			constexpr size_t kMaxUsers = 4;			// the switch of the test builds and the parts that work on a script program

			enum class HookState { NotTried, Made, Failed };

			// Written in DllMain only, while the game has not placed a program yet; the hook reads the users up
			// to g_userCount, which is published after the entry it counts.
			User g_users[kMaxUsers] = {};
			std::atomic<size_t> g_userCount{ 0 };
			HookState g_state = HookState::NotTried;
			std::string g_reason;					// why the hook could not be made
			std::atomic<uint32_t> g_placed{ 0 };	// every program the hook has seen

			using Resolve = uint8_t (*)(uint8_t* program);
			Resolve g_original = nullptr;			// the game's function behind the call

			// On the game's resource placement thread, for every script program it places.
			uint8_t ResolveHook(uint8_t* program)
			{
				const size_t users = std::min(g_userCount.load(std::memory_order_acquire), kMaxUsers);
				const uint32_t placed = g_placed.fetch_add(1, std::memory_order_relaxed) + 1;
				void* contexts[kMaxUsers] = {};
				DWORD lastError = GetLastError();
				for (size_t i = 0; i < users; i++)
				{
					if (!g_users[i].before)
						continue;
					try
					{
						contexts[i] = g_users[i].before(program, placed);
					}
					catch (...)
					{
					}
				}
				SetLastError(lastError);
				const uint8_t resolved = g_original(program);
				lastError = GetLastError();
				for (size_t i = 0; i < users; i++)
				{
					if (!g_users[i].after)
						continue;
					try
					{
						g_users[i].after(program, resolved, contexts[i]);
					}
					catch (...)
					{
					}
				}
				SetLastError(lastError);
				return resolved;
			}

			// "holds 0x1C" - an offset a site's instruction carries, for the DIAGNOSTIC lines
			std::string Holds(const uint8_t* at)
			{
				char text[16];
				snprintf(text, sizeof(text), "holds 0x%X", static_cast<unsigned>(*at));
				return text;
			}

			// Finds and checks the three sites and redirects the call; false with g_reason set if it cannot.
			bool MakeHook(const char* tag)
			{
				// every pattern is looked for, so that the log names each one missing
				uint8_t* const hook = Scanner::Find(kHookPattern);
				uint8_t* const nativeLoop = Scanner::Find(kNativeLoopPattern);
				uint8_t* const interpreter = Scanner::Find(kInterpreterPattern);
				const char* const missing = !hook ? kHookPattern.label : !nativeLoop ? kNativeLoopPattern.label :
					!interpreter ? kInterpreterPattern.label : nullptr;
				if (missing)
				{
					g_reason = std::string(missing) + " was not found.";
					return false;
				}

				uint8_t* const call = hook + kCallOffset;
				const uint8_t* const target = CallTarget(call);
				if (IsDiagnosticMode())
				{
					WriteLog(tag, std::string("DIAGNOSTIC: ") + kWhat + " at " + DescribeAddress(call) + " -> " +
						(target ? DescribeAddress(target) : std::string("not a call")) + " (" + kHookPattern.label + ")");
					WriteLog(tag, "DIAGNOSTIC: code length offset at " + DescribeAddress(nativeLoop + kCodeLengthAt) + " " +
						Holds(nativeLoop + kCodeLengthAt) + ", native table offset at " + DescribeAddress(nativeLoop + kNativesAt) + " " +
						Holds(nativeLoop + kNativesAt) + " (" + kNativeLoopPattern.label + ")");
					WriteLog(tag, "DIAGNOSTIC: code page table offset at " + DescribeAddress(interpreter + kCodePagesAt) + " " +
						Holds(interpreter + kCodePagesAt) + " (" + kInterpreterPattern.label + ")");
				}
				if (!target || !IsInsideGameImage(target))
				{
					g_reason = std::string("the ") + kWhat + " at " + DescribeAddress(call) + " is not a call into the game (another mod may "
						"have hooked it already).";
					return false;
				}

				const std::vector<CallRedirect> redirects = { { call, reinterpret_cast<const void*>(&ResolveHook), kWhat,
					reinterpret_cast<void**>(&g_original) } };
				if (!RedirectCalls(redirects, {}, tag))
				{
					g_reason = "the game code could not be changed (details in TweaksV.log).";
					return false;
				}
				if (!IsDryRun())
					WriteLog(tag, Capitalised(std::string("the ") + kWhat) + " at " + DescribeAddress(call) + " is redirected: every script "
						"program the game places passes TweaksV, before and after the game resolves its natives.");
				return true;
			}
		}

		bool AddUser(const User& user, std::string& reason)
		{
			if (g_state == HookState::Failed)
			{
				reason = g_reason;
				return false;
			}
			const size_t index = g_userCount.load(std::memory_order_relaxed);
			if (index >= kMaxUsers)
			{
				reason = "the script programs' hook has room for " + std::to_string(kMaxUsers) + " users only.";
				return false;
			}
			g_users[index] = user;	// in the table before the hook can call it
			g_userCount.store(index + 1, std::memory_order_release);
			if (g_state == HookState::Made)
				return true;

			if (!MakeHook(user.tag))
			{
				g_userCount.store(index, std::memory_order_release);	// nothing calls the table: the hook is not there
				g_state = HookState::Failed;
				reason = g_reason;
				return false;
			}
			g_state = HookState::Made;
			return true;
		}
	}
}
