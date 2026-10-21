// SPDX-License-Identifier: GPL-3.0-only
// TweaksV - holds a slim reader/writer lock (SRWLOCK) for a scope, released by an exception too.
#pragma once

#include <windows.h>

namespace TweaksV
{
	// Holds the lock exclusively until the end of the scope. An exception releases it too, so a
	// failure under the lock can never leave it taken - everything that takes it later (every log
	// line, every reported problem) would otherwise wait on it forever.
	struct ExclusiveLock
	{
		explicit ExclusiveLock(SRWLOCK& target) : lock(&target) { AcquireSRWLockExclusive(lock); }
		~ExclusiveLock() { ReleaseSRWLockExclusive(lock); }
		ExclusiveLock(const ExclusiveLock&) = delete;
		ExclusiveLock& operator=(const ExclusiveLock&) = delete;
		SRWLOCK* lock;
	};

	// The same for readers: held shared until the end of the scope.
	struct SharedLock
	{
		explicit SharedLock(SRWLOCK& target) : lock(&target) { AcquireSRWLockShared(lock); }
		~SharedLock() { ReleaseSRWLockShared(lock); }
		SharedLock(const SharedLock&) = delete;
		SharedLock& operator=(const SharedLock&) = delete;
		SRWLOCK* lock;
	};
}
