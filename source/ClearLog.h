// TweaksV: ClearLog(), carried over from ModkitLimitAdjusterEnhanced (Logging.h/.cpp) by
// Chiheb-Bacha - the same function exists in PackfileLimitAdjusterEnhanced and
// StraightToStoryMode with only the log file name changed -
// https://github.com/Chiheb-Bacha/ModkitLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV: one shared copy that clears TweaksV.log. TweaksV's
// DllMain calls it once per start (the per-plugin calls are removed).
#pragma once

void ClearLog();
