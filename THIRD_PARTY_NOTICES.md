# Third-Party Notices

Engine Tweaks and Limits Adjuster (TweaksV) is free software: Copyright (C) dachdecker369, licensed under version 3 of the GNU General Public License only (GPL-3.0-only). It contains the third-party code listed below, each piece used under its own license. All of these licenses are compatible with the GPL-3.0, and TweaksV as a whole is distributed under the GPL-3.0-only.

This file reproduces the copyright notices and license texts that this code requires. It must accompany every copy of TweaksV, in source or binary form. The source of TweaksV is at https://github.com/dachdecker369/engine-tweaks-and-limits-adjuster; a release's notes name the tag it was built from. Everyone whose work TweaksV builds on is credited in [CREDITS.md](CREDITS.md), the projects whose ideas its own parts follow among them: nothing of their code is in TweaksV, so nothing of theirs is listed here.

## Components

| Component | Copyright | License | Where in TweaksV |
|---|---|---|---|
| [MinHook](https://github.com/TsudaKageyu/minhook) 1.3.4 | Tsuda Kageyu; the Hacker Disassembler Engine in it by Vyacheslav Patkov | BSD-2-Clause | `vendor\MinHook`, unchanged; `hook.c`, `buffer.c`, `trampoline.c` and `hde64.c` are compiled into `TweaksV.asi`. The search for free memory near the game in `TweaksVPatch.cpp` is an adapted version of its `buffer.c` |
| [ScriptHookVDotNet](https://github.com/scripthookvdotnet/scripthookvdotnet) memory scanner | crosire, kagikn; the C++ port by Chiheb-Bacha | zlib | the pattern parser in `TweaksVScanner.cpp`, an altered version (see the notes) |
| [RageOpenV](https://github.com/Chiheb-Bacha/RageOpenV) | Chiheb-Bacha | GPL-3.0 | the console opener in `TweaksVLog.cpp`, carried over as it was |

Everything else in `dllmain.cpp`, `version.rc` and the `TweaksV*` files is TweaksV's own code, Copyright (C) dachdecker369, licensed under the GPL-3.0-only.

## Notes

- **ScriptHookVDotNet:** the pattern parser is an altered version, as the zlib license requires to be stated. It is Chiheb-Bacha's C++ port of ScriptHookVDotNet's `MemScanner.cs`, which came to TweaksV through his plugins, altered again for TweaksV: a malformed token is refused instead of throwing - `??` among them, so that one `?` is the only wildcard - and a miss returns nothing instead of ending the game. The search itself is TweaksV's own.
- **RageOpenV:** its license is the GPL-3.0, the same as TweaksV's; the text is [LICENSE](LICENSE), which accompanies every copy.

## License texts

### MinHook

```text
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

### ScriptHookVDotNet

```text
Copyright (C) 2015 crosire, kagikn

This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
authors be held liable for any damages arising from the use of this software.
Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
applications, and to alter it and redistribute it freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
     original  software. If you use this  software  in a product, an  acknowledgment in the product
     documentation would be appreciated but is not required.
  2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
     being the original software.
  3. This notice may not be removed or altered from any source distribution.
```

### GNU General Public License, version 3

Applies to TweaksV as a whole and to the console opener from RageOpenV. The text is [LICENSE](LICENSE), next to this file in the repository and in every release.
