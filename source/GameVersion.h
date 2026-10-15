// TweaksV: carried over unchanged from PackfileLimitAdjusterEnhanced (GameVersion.h) by
// Chiheb-Bacha - https://github.com/Chiheb-Bacha/PackfileLimitAdjusterEnhanced (GPL-3.0).
// Modified for TweaksV: only this header comment was added. Shared by
// PackfileLimitAdjusterEnhanced and TweaksV's log header.
#pragma once
#pragma comment(lib, "Version.lib")
#include <Windows.h>
#include <cstdint>
#include <vector>

struct FileVersion
{
    uint16_t Major = 0;
    uint16_t Minor = 0;
    uint16_t Build = 0;
    uint16_t Revision = 0;

    constexpr uint64_t ToComparable() const noexcept
    {
        return (uint64_t(Major) << 48) |
            (uint64_t(Minor) << 32) |
            (uint64_t(Build) << 16) |
            uint64_t(Revision);
    }

    friend constexpr bool operator<(const FileVersion& a, const FileVersion& b) noexcept
    {
        return a.ToComparable() < b.ToComparable();
    }

    friend constexpr bool operator>=(const FileVersion& a, const FileVersion& b) noexcept
    {
        return a.ToComparable() >= b.ToComparable();
    }
};

inline bool ReadCurrentProcessFileVersion(FileVersion& outVersion)
{
    wchar_t path[MAX_PATH]{};

    if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
        return false;

    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(path, &dummy);
    if (size == 0)
        return false;

    std::vector<std::byte> buffer(size);

    if (!GetFileVersionInfoW(path, 0, size, buffer.data()))
        return false;

    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;

    if (!VerQueryValueW(
        buffer.data(),
        L"\\",
        reinterpret_cast<LPVOID*>(&info),
        &infoSize))
    {
        return false;
    }

    outVersion.Major = HIWORD(info->dwFileVersionMS);
    outVersion.Minor = LOWORD(info->dwFileVersionMS);
    outVersion.Build = HIWORD(info->dwFileVersionLS);
    outVersion.Revision = LOWORD(info->dwFileVersionLS);

    return true;
}

inline uint64_t GetCurrentProcessVersionComparable()
{
    static const uint64_t cachedVersion = []() -> uint64_t
        {
            FileVersion v{};
            if (ReadCurrentProcessFileVersion(v))
                return v.ToComparable();
            return 0;
        }();

    return cachedVersion;
}
