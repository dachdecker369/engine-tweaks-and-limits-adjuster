// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - straight to story mode (see TweaksVStraightToStoryMode.h).
//
// Three screens, three small patches, one switch each:
//   SKIP_SPLASH_SCREEN  the splash screen: the "jz rel32" 0x0D before S-E1 becomes a "jmp" to the same
//                       target, computed from the game's own instruction, so the jump is always taken
//   SKIP_LEGAL_SCREEN   the legal screen: the 6-byte "jz rel32" 0x0F before S-E2 becomes six NOPs
//   SKIP_LANDING_PAGE   the landing page (story mode or online): the function the call in S-E3 leads
//                       to returns 0 at once ("xor eax, eax; ret")
// The bytes must be the game's own before a write: the splash and legal sites must hold a "jz". A
// screen already patched - an old StraightToStoryMode.asi under another name, say - is skipped; a
// landing-page function another mod hooked is left alone and reported; a missing pattern skips only
// its screen. Every write goes through TweaksVPatch, which logs it with DIAGNOSTIC_MODE and only
// describes it with DRY_RUN.
// Written for TweaksV. The splash and legal patches follow ChaosModV's, the landing-page patch is
// alloc8or's ("Unknown Modder"); StraightToStoryMode by Chiheb-Bacha carried both to GTA V Enhanced
// (CREDITS.md).

#include "TweaksVStraightToStoryMode.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "MemScanner.h"
#include "TweaksVConflicts.h"
#include "TweaksVLog.h"
#include "TweaksVPatch.h"
#include "TweaksVSettings.h"
#include "TweaksVSummary.h"

namespace TweaksV
{
	namespace
	{
		constexpr const char* kTag = "StraightToStoryMode";

		constexpr size_t kJzSize = 6;				// "jz rel32": 0F 84 rel32
		constexpr size_t kJmpSize = 5;				// "jmp rel32": E9 rel32
		constexpr uint8_t kNop = 0x90;
		constexpr uint8_t kReturnZero[] = { 0x31, 0xC0, 0xC3 };	// xor eax, eax; ret

		// Logs why for the start-up summary; the caller then leaves that screen as it is.
		void Fail(const char* screen, const std::string& why)
		{
			ReportFailure(kTag, std::string("The ") + screen + " is not skipped this session: " + why);
		}

		// "splash screen" -> "Splash screen", for the start of a line
		std::string Capitalised(const char* screen)
		{
			std::string text = screen;
			if (!text.empty() && text[0] >= 'a' && text[0] <= 'z')
				text[0] = static_cast<char>(text[0] - 'a' + 'A');
			return text;
		}

		// The one write of a screen's patch, in one wording: "<Screen> patched: <old bytes> -> <new bytes>."
		void Patch(uint8_t* address, const std::vector<uint8_t>& bytes, const char* screen)
		{
			const std::string before = HexBytes(address, bytes.size());
			if (!WriteCode({ { address, bytes, screen } }, kTag))
			{
				Fail(screen, "the game code could not be made writable (details in TweaksV.log).");
				return;
			}
			if (!IsDryRun())
				WriteLog(kTag, Capitalised(screen) + " patched: " + before + " -> " + HexBytes(bytes.data(), bytes.size()) + ".");
		}

		// The site of a jump that lies before its pattern's match: readable game code, or the reason not.
		uint8_t* JumpSite(const char* screen, uint8_t* match, size_t before)
		{
			uint8_t* const site = match - before;
			if (!IsInsideGameImage(site) || !IsReadable(site, kJzSize))
			{
				Fail(screen, "the game code before the pattern at " + DescribeAddress(match) + " cannot be read.");
				return nullptr;
			}
			return site;
		}

		void SkipSplashScreen()
		{
			constexpr const char* kScreen = "splash screen";
			if (!IsOn(kSkipSplashScreen))
			{
				WriteLog(kTag, "SKIP_SPLASH_SCREEN=0 - the splash screen is not skipped.");
				return;
			}
			uint8_t* const match = MemScanner::Find(Pattern("S-E1"));
			if (!match)
			{
				Fail(kScreen, "S-E1 was not found.");
				return;
			}
			uint8_t* const site = JumpSite(kScreen, match, 0x0D);
			if (!site)
				return;
			if (site[0] == 0xE9 && site[kJmpSize] == kNop)
			{
				WriteLog(kTag, "Splash screen already patched - skipped.");
				return;
			}
			if (site[0] != 0x0F || site[1] != 0x84)
			{
				Fail(kScreen, "the game code found is not the expected instruction (" + HexBytes(site, kJzSize) + ").");
				return;
			}

			// "jz rel32" (6 bytes) becomes "jmp rel32" (5 bytes) to the same target and a nop: the jump is
			// one byte shorter, so its displacement grows by one
			int32_t displacement = 0;
			memcpy(&displacement, site + 2, sizeof(displacement));
			const int64_t target = static_cast<int64_t>(displacement) + 1;
			if (target > INT32_MAX)
			{
				Fail(kScreen, "the jump found reaches too far for the patch.");
				return;
			}
			std::vector<uint8_t> patch = { 0xE9 };
			const std::vector<uint8_t> rel32 = BytesOf(static_cast<int32_t>(target));
			patch.insert(patch.end(), rel32.begin(), rel32.end());
			patch.push_back(kNop);
			Patch(site, patch, kScreen);
		}

		void SkipLegalScreen()
		{
			constexpr const char* kScreen = "legal screen";
			if (!IsOn(kSkipLegalScreen))
			{
				WriteLog(kTag, "SKIP_LEGAL_SCREEN=0 - the legal screen is not skipped.");
				return;
			}
			uint8_t* const match = MemScanner::Find(Pattern("S-E2"));
			if (!match)
			{
				Fail(kScreen, "S-E2 was not found.");
				return;
			}
			uint8_t* const site = JumpSite(kScreen, match, 0x0F);
			if (!site)
				return;
			const std::vector<uint8_t> nops(kJzSize, kNop);
			if (memcmp(site, nops.data(), kJzSize) == 0)
			{
				WriteLog(kTag, "Legal screen already patched - skipped.");
				return;
			}
			if (site[0] != 0x0F || site[1] != 0x84)	// the jump's distance is not checked: game updates change it
			{
				Fail(kScreen, "the game code found is not the expected instruction (" + HexBytes(site, kJzSize) + ").");
				return;
			}
			Patch(site, nops, kScreen);
		}

		void SkipLandingPage()
		{
			constexpr const char* kScreen = "landing page";
			if (!IsOn(kSkipLandingPage))
			{
				WriteLog(kTag, "SKIP_LANDING_PAGE=0 - the landing page is not skipped.");
				return;
			}
			uint8_t* const match = MemScanner::Find(Pattern("S-E3"));
			if (!match)
			{
				Fail(kScreen, "S-E3 was not found.");
				return;
			}
			uint8_t* const function = const_cast<uint8_t*>(CallTarget(match));	// the call the pattern starts with
			if (!function)
			{
				Fail(kScreen, "the game code found is not a call (" + HexBytes(match, kJmpSize) + ").");
				return;
			}
			if (!IsInsideGameImage(function) || !IsReadable(function, sizeof(kReturnZero)))
			{
				Fail(kScreen, "the call found does not lead into the game code (another mod may have redirected it).");
				return;
			}
			if (memcmp(function, kReturnZero, sizeof(kReturnZero)) == 0)
			{
				WriteLog(kTag, "Landing page already patched - skipped.");
				return;
			}
			if (StartsWithJump(function))
			{
				Fail(kScreen, "another mod has already hooked the function it patches.");
				return;
			}
			Patch(function, std::vector<uint8_t>(kReturnZero, kReturnZero + sizeof(kReturnZero)), kScreen);
		}
	}

	void InitStraightToStoryMode()
	{
		SkipSplashScreen();
		SkipLegalScreen();
		SkipLandingPage();
	}
}
