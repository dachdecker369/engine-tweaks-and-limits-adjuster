// SPDX-License-Identifier: GPL-3.0-only
// Test harness for TweaksV: a stand-in for DirectStorage's dstorage.dll with its four exports. The
// fake game (option "dstorage") loads it before TweaksV, as the real game imports it, and calls it
// after TweaksV has started. The factory has an 8-entry function table, as IDStorageFactory has:
// [4] is OpenFile, which opens the file for real, as DirectStorage does - CreateFileW for reading,
// sharing reading only - and keeps it open in the file object it hands out, until the fake game closes
// it (FakeCloseFile); it counts the calls that reach it (FakeOpenFileCalls), and the fake game reads a
// file through its handle (FakeFileHandle). A network path is not opened: the stand-in answers
// ERROR_BAD_NETPATH, so that no test reaches for a share. The other entries are never called.
#include <windows.h>

#include <new>

static volatile long g_openFileCalls = 0;

template <int N> static HRESULT STDMETHODCALLTYPE Method()
{
	return E_NOTIMPL + N;	// a different body each, so the linker keeps 8 functions
}

// The file object OpenFile hands out: the open handle, held until FakeCloseFile.
struct FakeFile
{
	HANDLE handle;
};

static HRESULT STDMETHODCALLTYPE FactoryOpenFile(void*, const wchar_t* path, REFIID, void** ppv)
{
	InterlockedIncrement(&g_openFileCalls);
	if (!path || !ppv)
		return E_INVALIDARG;
	*ppv = nullptr;
	if (path[0] == L'\\' && path[1] == L'\\' && path[2] != L'.' &&
		!(path[2] == L'?' && path[3] == L'\\' && path[4] != L'U' && path[4] != L'u'))
		return HRESULT_FROM_WIN32(ERROR_BAD_NETPATH);	// \\server\share or \\?\UNC\...: not opened
	const HANDLE handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(GetLastError());
	FakeFile* const file = new (std::nothrow) FakeFile{ handle };
	if (!file)
	{
		CloseHandle(handle);
		return E_OUTOFMEMORY;
	}
	*ppv = file;
	return S_OK;
}

static void* const g_table[8] = { reinterpret_cast<void*>(&Method<0>), reinterpret_cast<void*>(&Method<1>),
	reinterpret_cast<void*>(&Method<2>), reinterpret_cast<void*>(&Method<3>), reinterpret_cast<void*>(&FactoryOpenFile),
	reinterpret_cast<void*>(&Method<5>), reinterpret_cast<void*>(&Method<6>), reinterpret_cast<void*>(&Method<7>) };
static struct { void* const* table; } g_factory = { g_table };

extern "C" __declspec(dllexport) HRESULT WINAPI DStorageGetFactory(REFIID, void** ppv)
{
	if (!ppv)
		return E_POINTER;
	*ppv = &g_factory;
	return S_OK;
}

extern "C" __declspec(dllexport) HRESULT WINAPI DStorageSetConfiguration(const void* configuration)
{
	return configuration ? S_OK : E_INVALIDARG;
}

extern "C" __declspec(dllexport) HRESULT WINAPI DStorageSetConfiguration1(const void* configuration)
{
	return configuration ? S_OK : E_INVALIDARG;
}

extern "C" __declspec(dllexport) HRESULT WINAPI DStorageCreateCompressionCodec(int, unsigned, REFIID, void** ppv)
{
	if (ppv)
		*ppv = nullptr;
	return E_NOTIMPL;
}

// for the fake game: how many OpenFile calls reached this DLL
extern "C" __declspec(dllexport) long WINAPI FakeOpenFileCalls()
{
	return g_openFileCalls;
}

// for the fake game: the handle a file object OpenFile handed out holds open
extern "C" __declspec(dllexport) HANDLE WINAPI FakeFileHandle(void* file)
{
	return file ? static_cast<FakeFile*>(file)->handle : INVALID_HANDLE_VALUE;
}

// for the fake game: lets go of a file object OpenFile handed out, closing its handle
extern "C" __declspec(dllexport) void WINAPI FakeCloseFile(void* file)
{
	if (!file)
		return;
	CloseHandle(static_cast<FakeFile*>(file)->handle);
	delete static_cast<FakeFile*>(file);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID)
{
	return TRUE;
}
