// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the props per anchor of a ped model, the limit of the PedProps part to come, measured first. A ped
// model's props - hats, glasses, watches - hang on 13 anchors and come in variation files, its own and one per
// clothing pack; the game counts an anchor's props over all of them, but its checks take that total as 8 bits,
// so from 256 props on one anchor it refuses the props past (total mod 256). With DIAGNOSTIC_MODE=1 the module
// finds and checks the eleven places the part will widen and the calls around them, and logs how many props each
// ped model has on each anchor - writing nothing to the game.
#pragma once

namespace TweaksV
{
	// With DIAGNOSTIC_MODE=1 only: finds and checks the ped props' sites and starts the measurement of the props per
	// anchor. Called once by StartFeatures, through the table of the parts measured first.
	void MeasurePedProps();
}
