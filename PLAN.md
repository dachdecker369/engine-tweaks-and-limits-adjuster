# Plan

The order of work for a change to TweaksV, phase by phase, each phase ending with the test suites green, the four builds (Release, Debug, the test build, the analysis build) clean, a local commit and my boot. It carried the decals part of 1.6.0, the whole rewrite of 2.0.0 and the fixes of the 2.0.0 audit (all in the git history). One change is planned, 2.1.1, a patch with one phase: the DirectStorage part reports a factory request that never came as a problem, with a box of its own, instead of a log line (NOTES.md, T-UNLOCK). No feature is planned ([ROADMAP.md](ROADMAP.md)).

**State:** 2.1.1 not started. 2.1.0 is released, the tag on 2c01831, the commit I booted; main carries documents-only commits above it, all pushed; the suites stand at 485 checks; the open issues are T-HARDEN, T-LAYOUT, T-CODEPAGE, T-DIAG, T-SUMMARY, T-FREEZE and T-UNLOCK.
