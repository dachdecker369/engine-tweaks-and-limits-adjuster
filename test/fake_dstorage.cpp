// Test harness for TweaksV: a stand-in for DirectStorage's dstorage.dll with its four exports. The
// fake game (option "dstorage") loads it before TweaksV, as the real game imports it, and calls it
// after TweaksV has started. The factory has an 8-entry function table, as IDStorageFactory has:
// [4] is OpenFile, which hands out a dummy file and counts the calls that reach it
// (FakeOpenFileCalls); the other entries are never called.
#include <windows.h>

static volatile long g_openFileCalls = 0;
static int g_file = 0;	// the "file" OpenFile hands out

template <int N> static HRESULT STDMETHODCALLTYPE Method()
{
	return E_NOTIMPL + N;	// a different body each, so the linker keeps 8 functions
}

static HRESULT STDMETHODCALLTYPE FactoryOpenFile(void*, const wchar_t* path, REFIID, void** ppv)
{
	InterlockedIncrement(&g_openFileCalls);
	if (!path || !ppv)
		return E_INVALIDARG;
	*ppv = &g_file;
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

// for the fake game: how many OpenFile calls reached this DLL (TweaksV may refuse some first)
extern "C" __declspec(dllexport) long WINAPI FakeOpenFileCalls()
{
	return g_openFileCalls;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID)
{
	return TRUE;
}
