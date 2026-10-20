// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the heap adjuster: the game's memory heap, which HEAP_SIZE in [HEAP_ADJUSTER] enlarges.
#pragma once

namespace TweaksV
{
	// Sets the heap size, as [HEAP_ADJUSTER] asks. Called once by DllMain.
	void InitHeapAdjuster();
}
