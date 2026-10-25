# Plan

The order of work for a change to TweaksV, phase by phase, each phase ending with the test suites green, the four builds (Release, Debug, the test build, the analysis build) clean, a local commit and my boot. It carried the decals part of 1.6.0, the whole rewrite of 2.0.0, the fixes of the 2.0.0 audit, the 2.1.1 patch for T-UNLOCK, and the two versions from the NaturalVision Enhanced report, 2.2.0 and 2.2.1, built in parallel tracks - one coding session in a worktree of its own per track, each owning its files, rebased onto main in order and released one after the other (all in the git history). No change is planned, and no feature ([ROADMAP.md](ROADMAP.md)).

**State:** nothing planned. 2.2.1 is released, the tag on f965d5f, the commit I booted; main carries documents-only commits above it, all pushed; the suites stand at 521 checks; the open issues are T-HARDEN, T-LAYOUT, T-CODEPAGE, T-DIAG, T-SUMMARY and T-FREEZE.
