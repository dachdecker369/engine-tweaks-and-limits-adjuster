// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the sirens per vehicle, a limit of the SirenSettings part to come, measured first. The game lights
// siren1 to siren20 of a vehicle, so a vehicle made for 32 sirens keeps the rest dark. With DIAGNOSTIC_MODE=1
// the module counts the vehicle models whose sirens the game draws and logs which of them carry siren bones
// past siren20 and which lose siren bones past bone 127 (SPECIFICATIONS 11.10.4).
#pragma once

namespace TweaksV
{
	// Finds and checks the siren count's sites and starts its measurement, which redirects the vehicle lights'
	// call of the siren lights' renderer. Called by the table of the parts measured first, with
	// DIAGNOSTIC_MODE=1 only.
	void MeasureSirenCount();
}
