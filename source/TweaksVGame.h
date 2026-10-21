// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - the game it runs in: GTA5_Enhanced.exe only, and the game's file version.
#pragma once

#include <cstdint>

namespace TweaksV
{
	// True if this process is GTA5_Enhanced.exe. TweaksV does nothing in any other.
	bool IsGtaVEnhanced();

	// The game's file version, "1.0.1158.16": its four numbers, and one number that orders builds.
	struct GameVersion
	{
		uint16_t major = 0;
		uint16_t minor = 0;
		uint16_t build = 0;
		uint16_t revision = 0;

		constexpr uint64_t Number() const noexcept
		{
			return (static_cast<uint64_t>(major) << 48) | (static_cast<uint64_t>(minor) << 32) | (static_cast<uint64_t>(build) << 16) | revision;
		}
	};

	// The version from the game's version resource; false, and 0.0.0.0, if it has none.
	bool ReadGameVersion(GameVersion& version);

	// The same as one number, read once; 0 if it could not be read.
	uint64_t GameVersionNumber();
}
