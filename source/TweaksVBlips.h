// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the game's own blips next to a mod's. The game's script blip_controller makes the vanilla blips -
// the collectibles, the shops, the activities - only while fewer active blips than a threshold of its own,
// 150, are on the map, so the blips a map mod adds push the game's own off it. ACTIVE_BLIP_LIMIT in [BLIPS]
// raises that threshold in the script's code each time the game places the script; with DIAGNOSTIC_MODE=1
// the part logs the natives the script's check calls and how full the game's blip pool gets.
#pragma once

namespace TweaksV
{
	// Joins the script programs' hook (TweaksVScripts.h), as [BLIPS] asks, and with DIAGNOSTIC_MODE starts the
	// measurement of the blip pool. Called once by DllMain.
	void InitBlips();
}
