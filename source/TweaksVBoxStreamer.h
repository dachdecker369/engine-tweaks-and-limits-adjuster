// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the box streamer's traversal list: the map boxes one search of the game's box streamer can
// return. The game's own list holds 1000 entries, too few with heavy map mods, where LODs then go
// missing. TRAVERSAL_LIST_SIZE in [BOXSTREAMER] moves the list to a larger one; with DIAGNOSTIC_MODE=1
// the part counts its searches and logs how full the list gets, and whether the game's own list would
// have overflowed.
#pragma once

namespace TweaksV
{
	// Moves the list and raises its capacity, as [BOXSTREAMER] asks. Called once by DllMain.
	void InitBoxStreamer();
}
