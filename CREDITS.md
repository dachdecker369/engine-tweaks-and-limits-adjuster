# Credits

Five of TweaksV's parts are rewritten from Chiheb-Bacha's plugins below, and the other four - BoxStreamer, Decals, DirectStorage and Blips - are TweaksV's own, from the ideas and the design credited below; the research and the original code are theirs, and what little of their code TweaksV still carries is named in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). All credit for them goes to their authors.

## Chiheb-Bacha - https://github.com/Chiheb-Bacha

The five plugins TweaksV 1.x merged and 2.0.0 rewrote as its own parts, and the scanner they carried. Each plugin was taken from its main branch as it was when TweaksV merged it (the copies held no git data, so no commit is known); when a pattern stops matching after a game update, these projects are where it came from and worth a look ([PATTERNS.md](PATTERNS.md), "When Game Updates").

- [HeapAdjusterEnhanced](https://github.com/Chiheb-Bacha/HeapAdjusterEnhanced) - the HeapAdjuster part. Its LICENSE file says "licensed under LGPLv2" but holds the text of the MIT license; TweaksV 1.x carried its code under that text, 2.0.0 carries none of it
- [ModkitLimitAdjusterEnhanced](https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced) - the ModkitLimit part
- [PackfileLimitAdjusterEnhanced](https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced) - the PackfileLimit part
- [RageOpenV](https://github.com/Chiheb-Bacha/RageOpenV) - the RageOpenV part; its experimental newmods folder, which 1.x carried, went in 2.0.0
- [StraightToStoryMode](https://github.com/Chiheb-Bacha/StraightToStoryMode) - the StraightToStoryMode part
- [ScriptHookVDotNetEnhanced](https://github.com/Chiheb-Bacha/ScriptHookVDotNetEnhanced) - the C++ port of ScriptHookVDotNet's `MemScanner.cs` that those plugins carried, altered into the scanner's pattern parser

## The work those plugins build on

- **martonp96** - the original ClosedIV, the basis of RageOpenV (its repository is no longer public; RageOpenV's README is the source of this credit) - https://github.com/martonp96
- **CitizenFX Collective** - the original GTA V heap adjuster - https://github.com/citizenfx/gtav-heap-adjuster
- **Dilapidated** - the INI-configurable heap adjuster - https://github.com/Dilapidated/gtav-HeapAdjuster-ini
- **alloc8or ("Unknown Modder")** - the original Packfile Limit Adjuster and the Enhanced landing-page patch - https://github.com/alloc8or
- **Tanuki / 0x-FADED** - the original ModkitLimit Extender - https://github.com/0x-FADED/GTA5-VehicleModKitLimit-Fix. Its LICENSE places one part, the `ModKitIdRelocation` code for the weapon components limit, under the CitizenFX Collective's FiveM terms and the rest under the MIT license; nothing of that part was ever in TweaksV, and 2.0.0 carries none of the project's code
- **ChaosModV team** - the splash and legal screen skip patches - https://github.com/gta-chaos-mod/ChaosModV
- **CamxxCore** - the PauseMenuHelper pattern helper - https://github.com/CamxxCore/PauseMenuHelper
- **Bas Timmer (NTAuthority) et al.** - the stub allocator
- **crosire, kagikn and contributors** - the ScriptHookVDotNet memory scanner - https://github.com/scripthookvdotnet/scripthookvdotnet

## Ideas behind TweaksV's own parts

Where a project's license allows no reuse, TweaksV took the idea alone: nothing of its code was read, copied or adapted, and every game fact behind the part was found anew in Enhanced ([PATTERNS.md](PATTERNS.md)).

- **alloc8or** - DirectStorageFix, the idea behind TweaksV's DirectStorage part. Its repository carries no license, so only its README was read and none of its code: the part is a clean-room implementation of the idea - https://github.com/alloc8or/DirectStorageFix
- **CitizenFX Collective** - the box streamer traversal list fix and the decal definitions limit fix in FiveM, the ideas behind TweaksV's BoxStreamer and Decals parts. FiveM's `gta-streaming-five` component lies outside the folders its license frees, so that code is all rights reserved: none of it was read, and none of it is used - https://github.com/citizenfx/fivem
- **Tanuki / 0x-FADED** - the port of those fixes to GTA V Legacy. It carries an MIT license but says itself that it ports FiveM's code, which that license cannot cover; none of its code was read or used, and its Legacy layout differs from Enhanced's anyway - https://github.com/0x-FADED/GTA-V-fwBoxStreamerVariable-and-decals-limit-Patch
- **rootBrz** - Blip Limit Fix 1.4.2 (commit 09bf742 of 2026-10-24), GPL-3.0: the design behind TweaksV's Blips part - `blip_controller` found by its name's hash and its name, its whole code decoded before its guard is matched, the match in a window of 256 bytes and its checks, the rewrite of the threshold, and the setting's range and default - and the findings of its README on the blip pool. Its license would allow its code, but its sources carry another project's code, which allows no reuse, in every file that matters: so none of its sources or tests was read, the design came as prose with every pattern, offset, layout value and opcode value held back, no line of it is carried over, and every game fact behind the part was found anew in Enhanced - https://github.com/rootBrz/gtav-blip-limit-fix

## Libraries and tools

- **Tsuda Kageyu** - MinHook, 1.3.4 unchanged in `vendor\MinHook` - https://github.com/TsudaKageyu/minhook
- **Alexander Blade** - Script Hook V, whose ASI loader file (`xinput1_4.dll`) loads TweaksV. It is not bundled, and TweaksV does not use Script Hook V itself. - http://www.dev-c.com/gtav/scripthookv/
