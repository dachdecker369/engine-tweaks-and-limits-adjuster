// TweaksV: carried over from RageOpenV (src/gtav/rage/fiDevice.cpp) by Chiheb-Bacha, based on
// ClosedIV by martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV: the newmods device fixes; every change is
// marked "TweaksV:".
#include "main.h"
#include <atomic>					// TweaksV
#include <fstream>					// TweaksV
#include <mutex>					// TweaksV
#include <shared_mutex>				// TweaksV
#include <unordered_map>			// TweaksV
#include "TweaksVLog.h"	// TweaksV

namespace RageOpenV // TweaksV: one namespace per original plugin
{

// TweaksV: the game's Mount and SetPath, found once by FindGameFunctions() - upstream searched for
// them lazily, inside the game's own calls, and used the result unchecked
static bool(*g_gameMount)(void*, const char*) = nullptr;
static void(*g_gameSetPath)(void*, const char*, bool, rage::fiDevice*) = nullptr;

bool rage::fiDevice::FindGameFunctions()
{
	// TweaksV: only the GTA V Enhanced patterns are kept
	g_gameMount = memory::scan(TweaksV::Pattern("R-E-D1"))
		.as<bool(*)(void*, const char*)>();
	g_gameSetPath = memory::scan(TweaksV::Pattern("R-E-D2"))
		.as<void(*)(void*, const char*, bool, rage::fiDevice*)>();
	return g_gameMount && g_gameSetPath;
}

bool rage::fiDevice::Mount(const char* mountPoint)
{
	logger::write("device", "[%s] %s", __FUNCTION__, mountPoint);
	if (!g_gameMount) // TweaksV: found by FindGameFunctions() (was a lazy memory::scan here)
		return false;

	return g_gameMount(this, mountPoint);
}

void rage::fiDevice::SetPath(const char* path, bool allowRoot, rage::fiDevice* parent)
{
	logger::write("device", "[%s] %s", __FUNCTION__, path);
	if (!g_gameSetPath) // TweaksV: found by FindGameFunctions() (was a lazy memory::scan here)
		return;

	g_gameSetPath(this, path, allowRoot, parent);
}

// TweaksV: rage::fiDeviceRelative's constructor, Mount and SetPath removed - never used

rage::fiDeviceLocal::fiDeviceLocal() {
	pad[0xF8] = 0;
}
rage::fiDeviceLocal::~fiDeviceLocal(){}

// TweaksV: handle -> file name, only for LOG_DEVICE lines: filled only while that switch is on,
// and locked, as the game calls these functions from several threads
std::unordered_map<HANDLE, std::string> handleNames;
static std::shared_mutex handleNamesMutex;

// TweaksV: the file name for a log line - a lookup that never inserts (operator[] did)
static std::string HandleName(HANDLE handle)
{
	try
	{
		std::shared_lock lock(handleNamesMutex);
		auto it = handleNames.find(handle);
		return it != handleNames.end() ? it->second : std::string("?");
	}
	catch (...)
	{
		return std::string("?");
	}
}

// TweaksV: here and below, the names of parameters a function does not use are commented out
HANDLE rage::fiDeviceLocal::Open(const char* fileName, bool /*readOnly*/)
{
	logger::write("device", "[%s] %s", __FUNCTION__, fileName);
	// TweaksV: a file that cannot be opened gives INVALID_HANDLE_VALUE, as the game expects (the
	// failed stream used to be returned as a handle), and nothing may throw back into the game
	try
	{
		auto file = new std::ifstream(ToFullPath(fileName), std::ios::in | std::ios::binary);
		if (!file->is_open())
		{
			delete file;
			return INVALID_HANDLE_VALUE;
		}
		HANDLE handle = file;
		if (config::get_log("device")) // TweaksV: the name is kept only for LOG_DEVICE lines
		{
			std::unique_lock lock(handleNamesMutex);
			handleNames[handle] = fileName ? fileName : "";
		}
		return handle;
	}
	catch (...)
	{
		return INVALID_HANDLE_VALUE;
	}
}

uint32_t rage::fiDeviceLocal::ReadFile(HANDLE handle, void* buffer, uint32_t length)
{
	if (config::get_log("device")) // TweaksV: the name is only looked up for a line that is written
		logger::write("device", "[%s] %s %u", __FUNCTION__, HandleName(handle).c_str(), length);
	auto file = (std::ifstream*)handle;
	file->clear(); // TweaksV: a short read left failbit set and every later read failed
	file->read((char*)buffer, length);
	return static_cast<uint32_t>(file->gcount()); // TweaksV: cast - at most length
}

uint32_t rage::fiDeviceLocal::ReadBulk(HANDLE handle, uint64_t offset, char* buffer, uint32_t length)
{
	if (config::get_log("device")) // TweaksV: the name is only looked up for a line that is written
		logger::write("device", "[%s] %s %u", __FUNCTION__, HandleName(handle).c_str(), length);
	auto file = (std::ifstream*)handle;
	file->clear(); // TweaksV: after a short read, seekg did nothing
	file->seekg(offset);
	return ReadFile(handle, buffer, length);
}

uint64_t rage::fiDeviceLocal::Seek64(HANDLE handle, int64_t distance, uint32_t method)
{
	if (config::get_log("device")) // TweaksV: only for a line that is written; %lld for the 64-bit distance (was %d)
		logger::write("device", "[%s] %s %lld", __FUNCTION__, HandleName(handle).c_str(), distance);
	auto file = (std::ifstream*)handle;
	file->clear(); // TweaksV: after a short read, seekg did nothing
	if (method == SEEK_CUR)
	{
		file->seekg(distance, std::ios::cur);
	}
	else if (method == SEEK_SET)
	{
		file->seekg(distance, std::ios::beg);
	}
	else if (method == SEEK_END)
	{
		// TweaksV: B-ROV-18 - upstream subtracts the distance, Win32's FILE_END adds it. Kept as it
		// is until it is known how the game calls this; a non-zero distance is logged once.
		if (distance != 0)
		{
			static std::atomic<bool> logged{ false };
			if (!logged.exchange(true))
				TweaksV::WriteLog("RageOpenV", "newmods: the game seeks from the end of a file with distance " +
					std::to_string(distance) + " (upstream's sign is kept - B-ROV-18).");
		}
		file->seekg(-1 * distance, std::ios::end);
	}

	return file->tellg();
}

uint32_t rage::fiDeviceLocal::Close(HANDLE handle)
{
	if (!handle || handle == INVALID_HANDLE_VALUE) // TweaksV: not one of our streams
		return false;
	auto file = (std::ifstream*)handle;
	if (config::get_log("device")) // TweaksV: only for LOG_DEVICE - the line, and the name, which is no longer needed
	{
		logger::write("device", "[%s] %s", __FUNCTION__, HandleName(handle).c_str());
		std::unique_lock lock(handleNamesMutex);
		handleNames.erase(handle);
	}
	delete file; // TweaksV: the stream, and its OS file handle, leaked on every close
	return true;
}

uint64_t rage::fiDeviceLocal::Size64(HANDLE handle)
{
	uint64_t fileCurrentPos = Seek64(handle, 0, SEEK_CUR);
	uint64_t fileEndPos = Seek64(handle, 0, SEEK_END);
	Seek64(handle, fileCurrentPos, SEEK_SET);
	if (config::get_log("device")) // TweaksV: only for a line that is written; %llu for the 64-bit size (was %d)
		logger::write("device", "[%s] %s %llu", __FUNCTION__, HandleName(handle).c_str(), fileEndPos);
	return fileEndPos;
}

uint64_t rage::fiDeviceLocal::GetFileSize(const char* fileName)
{
	uint64_t size = 0;
	WIN32_FILE_ATTRIBUTE_DATA fileData;
	if (GetFileAttributesExA(ToFullPath(fileName).c_str(), GET_FILEEX_INFO_LEVELS::GetFileExInfoStandard, &fileData))
		size = fileData.nFileSizeLow | (static_cast<size_t>(fileData.nFileSizeHigh) << 32);

	logger::write("device", "[%s] %s %llu", __FUNCTION__, fileName, size); // TweaksV: %llu (was %d)
	return size;
}

uint64_t rage::fiDeviceLocal::GetFileTime(const char* fileName)
{
	WIN32_FILE_ATTRIBUTE_DATA fileData;
	uint64_t filetime = 0;
	if (GetFileAttributesExA(ToFullPath(fileName).c_str(), GET_FILEEX_INFO_LEVELS::GetFileExInfoStandard, &fileData))
		filetime = *(uint64_t*)&fileData.ftLastWriteTime;

	logger::write("device", "[%s] %s %llu", __FUNCTION__, fileName, filetime); // TweaksV: %llu (was %d)
	return filetime;
}

uint32_t rage::fiDeviceLocal::GetAttributes(const char* fileName)
{
	uint32_t attributes = ::GetFileAttributesA(ToFullPath(fileName).c_str());
	if (attributes != INVALID_FILE_ATTRIBUTES)
		logger::write("device", "[%s] %s", __FUNCTION__, fileName);
	return attributes;
}

HANDLE rage::fiDeviceLocal::FindFileBegin(const char* path, rage::fiFindData* findData)
{
	logger::write("device", "[%s]", __FUNCTION__);
	WIN32_FIND_DATAW foundData;

	// TweaksV: the path and the search pattern are built at their real length - in the fixed 256- and
	// 260-character buffers they had, a long path stopped the game (swprintf_s ends the process when
	// its buffer is too small); a path too long to search now fails like a missing folder
	const size_t pathLen = path ? strlen(path) : 0;
	const bool slashFound = pathLen && (path[pathLen - 1] == '/' || path[pathLen - 1] == '\\');

	// TweaksV: an empty full path (an error) must not turn into "\*", the root of the drive
	const std::string fullPath = ToFullPath(path);
	if (fullPath.empty())
		return INVALID_HANDLE_VALUE;

	// TweaksV: CP_ACP - path::string() gives the ANSI code page, not UTF-8 (was 0xFDE9)
	const int wideLength = MultiByteToWideChar(CP_ACP, 0, fullPath.c_str(), -1, nullptr, 0);
	if (wideLength <= 1)
		return INVALID_HANDLE_VALUE;
	std::wstring pattern(static_cast<size_t>(wideLength - 1), L'\0');
	MultiByteToWideChar(CP_ACP, 0, fullPath.c_str(), -1, pattern.data(), wideLength);

	// TweaksV: was swprintf_s into a 260-character buffer - before that wprintf_s, which printed to
	// stdout with the buffer as the format, so FindFirstFileW always got an empty path and failed
	pattern += slashFound ? L"*" : L"\\*";
	HANDLE firstFileHandle = FindFirstFileW(pattern.c_str(), &foundData);
	if (firstFileHandle == INVALID_HANDLE_VALUE)
		return INVALID_HANDLE_VALUE;

	ConvertFoundData(&foundData, findData);
	return firstFileHandle;
}

bool rage::fiDeviceLocal::FindFileNext(HANDLE handle, rage::fiFindData* findData)
{
	logger::write("device", "[%s]", __FUNCTION__);
	WIN32_FIND_DATAW foundData;
	bool fileFound = FindNextFileW(handle, &foundData);
	if (fileFound)
		ConvertFoundData(&foundData, findData);
	return fileFound;
}

int rage::fiDeviceLocal::FindFileEnd(HANDLE handle)
{
	// TweaksV: a search handle is never in handleNames - operator[] added an empty entry for it on
	// every call; the unused stream cast is gone
	logger::write("device", "[%s]", __FUNCTION__);
	return (FindClose(handle) != 0) - 1;
}

HANDLE rage::fiDeviceLocal::OpenBulk(const char* fileName, uint64_t* /*ptr*/)
{
	return Open(fileName, true);
}

HANDLE rage::fiDeviceLocal::OpenBulkWrap(const char* fileName, uint64_t* ptr, void* /*unk*/)
{
	return OpenBulk(fileName, ptr);
}

uint32_t rage::fiDeviceLocal::Seek(HANDLE handle, int32_t distance, uint32_t method)
{
	return (uint32_t)Seek64(handle, (uint64_t)distance, method);
}

uint32_t rage::fiDeviceLocal::CloseBulk(HANDLE handle)
{
	return Close(handle);
}

uint32_t rage::fiDeviceLocal::Size(HANDLE handle)
{
	return (uint32_t)Size64(handle);
}

rage::fiDevice* rage::fiDeviceLocal::GetLowLevelDevice()
{
	logger::write("device", "[%s]", __FUNCTION__);
	return this;
}

void* rage::fiDeviceLocal::FixRelativeName(char* dest, int length, const char* source)
{
	logger::write("device", "[%s]", __FUNCTION__);
	// TweaksV: copies the name and no more, always ending it - upstream copied length bytes whatever
	// the name's own length, reading past its end
	if (!dest || length <= 0)
		return dest;
	const size_t size = source ? strnlen(source, static_cast<size_t>(length) - 1) : 0;
	if (size)
		memcpy(dest, source, size);
	dest[size] = '\0';
	return dest;
}

bool rage::fiDeviceLocal::SetEndOfFile(HANDLE /*handle*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return false;
}

bool rage::fiDeviceLocal::SafeRead(HANDLE handle, void* buffer, uint32_t length)
{
	logger::write("device", "[%s]", __FUNCTION__);
	uint32_t offset = 0;
	if (length <= 0)
		return true;
	for (;;)
	{
		uint32_t bytesRead = ReadFile(handle, (char*)buffer + offset, length - offset);
		if (bytesRead == 0) // TweaksV: was "bytesRead < 0" - never true for an unsigned value, so this looped forever at the end of a file
			break;
		offset += bytesRead;
		if (offset >= length)
			return true;
	}
	return false;
}

bool rage::fiDeviceLocal::SafeWrite(HANDLE /*handle*/, const void* /*buffer*/, uint32_t /*length*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return false;
}

HANDLE rage::fiDeviceLocal::CreateLocal(const char* /*fileName*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

HANDLE rage::fiDeviceLocal::Create(const char* /*fileName*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

uint32_t rage::fiDeviceLocal::WriteBulk(HANDLE /*handle*/, uint64_t /*offset*/, const void* /*buffer*/, uint32_t /*length*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

uint32_t rage::fiDeviceLocal::Write(HANDLE /*hande*/, const void* /*buffer*/, uint32_t /*length*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

uint32_t rage::fiDeviceLocal::Flush(HANDLE /*handle*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

bool rage::fiDeviceLocal::Delete(const char* /*fileName*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

bool rage::fiDeviceLocal::Rename(const char* /*from*/, const char* /*to*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

bool rage::fiDeviceLocal::MakeDirectory(const char* /*dir*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

bool rage::fiDeviceLocal::UnmakeDirectory(const char* /*dir*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

void rage::fiDeviceLocal::Sanitize()
{
	logger::write("device", "[%s]", __FUNCTION__);
}

bool rage::fiDeviceLocal::SetFileTime(const char* /*fileName*/, uint64_t /*fileTime*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

uint64_t rage::fiDeviceLocal::GetRootDeviceId(const char*)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 1;
}

bool rage::fiDeviceLocal::SetAttributes(const char* /*fileName*/, uint32_t /*attributes*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return false;
}

uint32_t rage::fiDeviceLocal::IsMemoryMappedDevice()
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 2;
}

uint32_t rage::fiDeviceLocal::GetResourceInfo(const char* /*fileName*/, rage::fiResourceInfo* /*flags*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

int32_t rage::fiDeviceLocal::IsValidHandle()
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

uint32_t rage::fiDeviceLocal::GetBulkOffset(HANDLE /*handle*/)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

uint32_t rage::fiDeviceLocal::GetPhysicalSortKey(const char*)
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0x40000000;
}

bool rage::fiDeviceLocal::IsRpf()
{
	logger::write("device", "[%s]", __FUNCTION__);
	return false;
}

uint8_t rage::fiDeviceLocal::GetRpfVersion()
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

rage::fiDevice* rage::fiDeviceLocal::GetRpfDevice()
{
	logger::write("device", "[%s]", __FUNCTION__);
	return this;
}

bool rage::fiDeviceLocal::IsCloud()
{
	logger::write("device", "[%s]", __FUNCTION__);
	return false;
}

uint64_t rage::fiDeviceLocal::GetPackfileIndex()
{
	logger::write("device", "[%s]", __FUNCTION__);
	return 0;
}

const char* rage::fiDeviceLocal::GetDebugName()
{
	logger::write("device", "[%s]", __FUNCTION__);
	return "rage::fiDeviceLocal";
}

void rage::fiDeviceLocal::ConvertFoundData(LPWIN32_FIND_DATAW foundDataWin, rage::fiFindData* foundDataRage)
{
	logger::write("device", "[%s]", __FUNCTION__);
	// TweaksV: a name that does not fit gives an empty name instead of an unterminated buffer
	if (!WideCharToMultiByte(0xFDE9u, 0, foundDataWin->cFileName, -1, foundDataRage->fileName, 256, 0i64, 0i64))
		foundDataRage->fileName[0] = '\0';
	for (size_t i = 0; i < strlen(foundDataRage->fileName); ++i) // TweaksV: size_t (was uint8_t)
		if (foundDataRage->fileName[i] == '/')
			foundDataRage->fileName[i] = '\\';
	foundDataRage->lastWriteTime = foundDataWin->ftLastWriteTime;
	foundDataRage->fileAttributes = foundDataWin->dwFileAttributes;
	// TweaksV: nFileSizeHigh comes before nFileSizeLow in WIN32_FIND_DATAW, so reading both as one
	// 64-bit value swapped the halves
	foundDataRage->fileSize = (static_cast<uint64_t>(foundDataWin->nFileSizeHigh) << 32) | foundDataWin->nFileSizeLow;
}

} // namespace RageOpenV
