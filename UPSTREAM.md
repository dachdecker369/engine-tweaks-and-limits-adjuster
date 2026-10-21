# Upstream

The projects TweaksV's parts are derived from. TweaksV 1.x merged five of them as they were, with their archives in this repository for comparison; since 2.0.0 every part is written anew from what the original does and from the game facts in [PATTERNS.md](PATTERNS.md), none of their code remains, and the archives are gone. The patterns, the game facts and the ideas are theirs: [CREDITS.md](CREDITS.md) names everyone, [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) carries the notices still needed.

| Part of TweaksV | Derived from | Version |
|---|---|---|
| HeapAdjuster | [HeapAdjusterEnhanced](https://github.com/Chiheb-Bacha/HeapAdjusterEnhanced) | the main branch as it was when TweaksV merged it; the copy held no git data, so no commit is known |
| ModkitLimit | [ModkitLimitAdjusterEnhanced](https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced) | the same |
| PackfileLimit | [PackfileLimitAdjusterEnhanced](https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced) | the same |
| RageOpenV | [RageOpenV](https://github.com/Chiheb-Bacha/RageOpenV) | the same; its newmods folder is not carried over |
| StraightToStoryMode | [StraightToStoryMode](https://github.com/Chiheb-Bacha/StraightToStoryMode) | the same |
| The scanner's pattern parser | [ScriptHookVDotNetEnhanced](https://github.com/Chiheb-Bacha/ScriptHookVDotNetEnhanced) | the C++ port of ScriptHookVDotNet's `MemScanner.cs` that those plugins carried, altered for TweaksV |
| MinHook | [minhook](https://github.com/TsudaKageyu/minhook) | 1.3.4, unchanged, in `vendor\MinHook` |

## Notes

- **After a game update:** the patterns are TweaksV's to keep up ([PATTERNS.md](PATTERNS.md), "When Game Updates"). The projects above are where each pattern came from, and worth a look when one no longer matches.
- **DirectStorageFix:** alloc8or's [DirectStorageFix](https://github.com/alloc8or/DirectStorageFix) is the idea behind TweaksV's DirectStorage part; none of its code is used (see [CREDITS.md](CREDITS.md)).
- **Box streamer and decal definitions:** the CitizenFX Collective's fixes for FiveM and 0x-FADED's port of them to GTA V Legacy are the ideas behind TweaksV's BoxStreamer and Decals parts; none of their code is used (see [CREDITS.md](CREDITS.md)).
