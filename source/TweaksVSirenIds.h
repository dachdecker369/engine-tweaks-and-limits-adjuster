// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the siren setting IDs, measured before the SirenSettings part to come lifts their limit. A siren setting
// of carcols.meta and the vehicle variation that names it carry its ID in one byte, and a table of 256 bytes maps an
// ID to the setting's position in the game's array, a byte too: so the game has 256 IDs and 255 settings a vehicle can
// use, and a pack whose IDs clash with others' loses its settings silently. With DIAGNOSTIC_MODE=1 the module finds and
// checks the sites the part's writing half will change and logs how many settings and IDs the game and the mods use.
#pragma once

namespace TweaksV
{
	// Finds and checks every site of the limit, logs each one and starts the measurement; writes nothing to the game.
	// Started once by StartFeatures, with DIAGNOSTIC_MODE=1 only (kMeasuredFirst, TweaksVFeatures.cpp).
	void MeasureSirenIds();
}
