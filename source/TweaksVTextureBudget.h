// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the video memory budget for streamed resources, measured first for the Resources part to come. The
// game places a streamed texture or buffer only while their video memory stays below one of four budgets from
// gameconfig.xml, chosen by the texture quality - 3000 MB at the highest - and evicts what it cannot place, so a
// heavy texture load goes missing however much memory the card has. With DIAGNOSTIC_MODE=1 TweaksV finds and
// checks the budget's sites in the game's code and logs how close the streamed textures and buffers come to it.
#pragma once

namespace TweaksV
{
	// Finds and checks the texture budget's sites and starts its measurement; writes nothing to the game. Called
	// once by DllMain, through the table of the parts measured first, with DIAGNOSTIC_MODE=1 only.
	void MeasureTextureBudget();
}
