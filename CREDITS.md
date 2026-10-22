# Credits

TweaksV's parts are rewritten from the projects below; the research and the original code are theirs, and what little of their code TweaksV still carries is named in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). All credit for them goes to their authors.

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

- **alloc8or** - DirectStorageFix, the idea behind TweaksV's DirectStorage part (only its description is used, none of its code) - https://github.com/alloc8or/DirectStorageFix
- **CitizenFX Collective** - the box streamer traversal list fix and the decal definitions limit fix in FiveM, the ideas behind TweaksV's BoxStreamer and Decals parts (none of their code is used) - https://github.com/citizenfx/fivem
- **Tanuki / 0x-FADED** - the port of those fixes to GTA V Legacy (none of its code is used) - https://github.com/0x-FADED/GTA-V-fwBoxStreamerVariable-and-decals-limit-Patch

## Libraries and tools

- **Tsuda Kageyu** - MinHook, 1.3.4 unchanged in `vendor\MinHook` - https://github.com/TsudaKageyu/minhook
- **Alexander Blade** - Script Hook V, whose ASI loader file (`xinput1_4.dll`) loads TweaksV. It is not bundled, and TweaksV does not use Script Hook V itself. - http://www.dev-c.com/gtav/scripthookv/
- **ThirteenAG** - Ultimate ASI Loader, an alternative loader (not bundled) - https://github.com/ThirteenAG/Ultimate-ASI-Loader
