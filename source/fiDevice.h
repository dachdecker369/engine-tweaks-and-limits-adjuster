// TweaksV: carried over from RageOpenV (src/gtav/rage/fiDevice.h) by Chiheb-Bacha, based on
// ClosedIV by martonp96 - https://github.com/Chiheb-Bacha/RageOpenV (GPL-3.0).
// Modified for TweaksV; every change is marked "TweaksV:". The virtual functions
// below are the game's vtable layout: never reorder, add or remove any.
// TweaksV: fiDeviceLocal, Mount, SetPath and ToFullPath - the device of the newmods folder, which
// TweaksV no longer has - are removed; the vtable stays as the base of fiPackfile (fiPackfile.h).
#pragma once
#include <cstddef>	// TweaksV: offsetof

namespace RageOpenV // TweaksV: one namespace per original plugin
{
namespace rage
{
	struct fiFindData
	{
		char fileName[256];
		uint64_t fileSize;
		FILETIME lastWriteTime;
		DWORD fileAttributes;
	};

	class fiResourceInfo;

	class fiDevice
	{
	public:
		virtual ~fiDevice() = default;
		virtual HANDLE Open(const char* fileName, bool readOnly) = 0;
		virtual HANDLE OpenBulk(const char* fileName, uint64_t* ptr) = 0;
		virtual HANDLE OpenBulkWrap(const char* fileName, uint64_t* ptr, void* unk) = 0;
		virtual HANDLE CreateLocal(const char* fileName) = 0;
		virtual HANDLE Create(const char* fileName) = 0;
		virtual uint32_t ReadFile(HANDLE handle, void* buffer, uint32_t toRead) = 0;
		virtual uint32_t ReadBulk(HANDLE handle, uint64_t ptr, char* buffer, uint32_t toRead) = 0;
		virtual uint32_t WriteBulk(HANDLE handle, uint64_t offset, const void* buffer, uint32_t length) = 0;
		virtual uint32_t Write(HANDLE handle, const void* buffer, uint32_t length) = 0;
		virtual uint32_t Seek(HANDLE handle, int32_t distance, uint32_t method) = 0;
		virtual uint64_t Seek64(HANDLE handle, int64_t distance, uint32_t method) = 0;
		virtual uint32_t Close(HANDLE handle) = 0;
		virtual uint32_t CloseBulk(HANDLE handle) = 0;
		virtual uint32_t Size(HANDLE handle) = 0;
		virtual uint64_t Size64(HANDLE handle) = 0;
		virtual uint32_t Flush(HANDLE handle) = 0;
		virtual bool Delete(const char* fileName) = 0;
		virtual bool Rename(const char* from, const char* to) = 0;
		virtual bool MakeDirectory(const char* dir) = 0;
		virtual bool UnmakeDirectory(const char* dir) = 0;
		virtual void Sanitize() = 0;
		virtual uint64_t GetFileSize(const char* fileName) = 0;
		virtual uint64_t GetFileTime(const char* fileName) = 0;
		virtual bool SetFileTime(const char* file, uint64_t fileTime) = 0;
		virtual HANDLE FindFileBegin(const char* path, rage::fiFindData* findData) = 0;
		virtual bool FindFileNext(HANDLE handle, rage::fiFindData* findData) = 0;
		virtual int FindFileEnd(HANDLE handle) = 0;
		virtual rage::fiDevice* GetLowLevelDevice() = 0;
		virtual void* FixRelativeName(char*, int, const char*) = 0;
		virtual bool SetEndOfFile(HANDLE handle) = 0;
		virtual uint32_t GetAttributes(const char* path) = 0;
		virtual uint64_t GetRootDeviceId(const char*) = 0;
		virtual bool SetAttributes(const char* file, uint32_t attributes) = 0;
		virtual uint32_t IsMemoryMappedDevice() = 0;
		virtual bool SafeRead(HANDLE handle, void* buffer, uint32_t length) = 0;
		virtual bool SafeWrite(HANDLE handle, const void* buffer, uint32_t length) = 0;
		virtual uint32_t GetResourceInfo(const char* fileName, fiResourceInfo* flags) = 0;
		virtual int32_t IsValidHandle() = 0;
		virtual uint32_t GetBulkOffset(HANDLE handle) = 0;
		virtual uint32_t GetPhysicalSortKey(const char*) = 0;
		virtual bool IsRpf() = 0;
		virtual uint8_t GetRpfVersion() = 0;
		virtual fiDevice* GetRpfDevice() = 0;
		virtual bool IsCloud() = 0;
		virtual uint64_t GetPackfileIndex() = 0;
		virtual const char* GetDebugName() = 0;

		// TweaksV: Mount, SetPath and FindGameFunctions - the newmods device's - are removed
	};

	// TweaksV: class fiDeviceRelative removed - declared and defined, but never used; class fiDeviceLocal
	// removed with the newmods folder

	// TweaksV: the layout the game relies on (verified with MSVC 19.51)
	static_assert(offsetof(fiFindData, fileSize) == 0x100 && offsetof(fiFindData, lastWriteTime) == 0x108 &&
		offsetof(fiFindData, fileAttributes) == 0x110 && sizeof(fiFindData) == 0x118, "rage::fiFindData layout changed");
}
} // namespace RageOpenV
