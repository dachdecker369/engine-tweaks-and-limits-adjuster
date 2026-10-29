// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the streaming memory, measured first for the Resources part to come (SPECIFICATIONS 11.10.7): the
// heap the game streams the map, the props and every other streamed resource into - at most 32 regions of
// 256 MB on 1.0.1158.16 - and the streaming allocator's budgets beside it. With DIAGNOSTIC_MODE=1 the module
// finds and checks their sites and logs how full the heap gets and whether streamed resources are refused for
// memory; it writes nothing to the game.
#pragma once

namespace TweaksV
{
	// Finds and checks the streaming memory's four sites - the measurement's three and the region size cap the
	// part's writing half would raise - logs each with what it holds, and starts the measurement; writes
	// nothing. Called once by DllMain, through the table of the parts measured first, with DIAGNOSTIC_MODE=1 only.
	void MeasureStreamingMemory();
}
