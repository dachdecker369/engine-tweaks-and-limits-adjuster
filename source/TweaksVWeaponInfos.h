// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the weapon infos, a limit of the WeaponLimits part to come, measured first. The game parses each
// weapons.meta file into one weapon info blob, and its array of blobs has room for 160 of them on 1.0.1158.16: a
// 161st file is written past the array's end, into the game's heap, and the game crashes while loading or later.
// With DIAGNOSTIC_MODE=1 the module finds and checks every site the part will raise and logs how many weapons.meta
// files the game has loaded against that room; it writes nothing to the game.
#pragma once

namespace TweaksV
{
	// Finds and checks the sites of the weapon info blob array and starts its measurement. Started by the table of
	// the parts measured first (TweaksVFeatures.cpp) with DIAGNOSTIC_MODE=1 only, once, from DllMain.
	void MeasureWeaponInfos();
}
