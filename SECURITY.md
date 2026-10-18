# Security

TweaksV runs inside GTA V Enhanced on players' PCs, so it treats every file and setting it reads as possibly hostile. A way to make it do harm all the same - with a crafted `TweaksV.ini`, a file in the game, `mods` or `newmods` folder, or a link where its own files go - is a security problem.

## Reporting a Problem

Please report it privately, not in a public issue:

1. Open the repository's **Security** tab and choose **Report a vulnerability**, or go straight to the [report form](https://github.com/dachdecker369/engine-tweaks-and-limits-adjuster/security/advisories/new).
2. Say what happens and how to make it happen, with the files or settings it needs, the TweaksV version and the game version (both are at the top of `TweaksV.log`).

Only you and the maintainer can see the report. The answer comes there, and the report stays private until a version with the fix is out.

## Supported Versions

Security fixes go into the latest version only.

## Scope

- **This repository:** TweaksV's own code and the merged plugins' code.
- **Elsewhere:** a problem in MinHook itself goes to [its authors](https://github.com/TsudaKageyu/minhook); one in the game, the ASI loader or Script Hook V goes to theirs.
- **Known:** MinHook keeps its trampolines in memory that is writable and executable. This is accepted; see T-HARDEN in [NOTES.md](NOTES.md).
