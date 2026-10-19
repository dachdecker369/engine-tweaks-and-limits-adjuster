// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the box streamer's traversal list: the map boxes one search of the game's box streamer can
// return. The game's own list holds 1000 entries, too few with heavy map mods, where LODs then go
// missing. TRAVERSAL_LIST_SIZE in [BOXSTREAMER_SETTINGS] moves the list to a larger one.
#pragma once

namespace TweaksV
{
	// Moves the list and raises its capacity, as [BOXSTREAMER_SETTINGS] asks. Called once by DllMain.
	void InitBoxStreamer();
}
