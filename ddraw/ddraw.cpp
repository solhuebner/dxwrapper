/**
* Copyright (C) 2025 Elisha Riedlinger
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*
* SetAppCompatData code created based on information from here:
* http://web.archive.org/web/20170418171908/http://www.blitzbasic.com/Community/posts.php?topic=99477
*/

#include "ddraw.h"
#include "Dllmain\Dllmain.h"
#include "d3d9\d3d9External.h"
#include "Utils\Utils.h"
#include "GDI\GDI.h"
#include "External\Hooking\Hook.h"

AddressLookupTableDdraw<void> ProxyAddressLookupTable = AddressLookupTableDdraw<void>();

static UINT GetAdapterIndex(GUID FAR* lpGUID);

namespace {
	bool IsInitialized = false;
	CRITICAL_SECTION ddcs;
	CRITICAL_SECTION pecs;
}

namespace DdrawWrapper
{
	VISIT_PROCS_DDRAW(INITIALIZE_OUT_WRAPPED_PROC);
	VISIT_PROCS_DDRAW_SHARED(INITIALIZE_OUT_WRAPPED_PROC);
	INITIALIZE_OUT_WRAPPED_PROC(Direct3DCreate9, unused);

	struct DDDeviceInfo
	{
		GUID guid;
		std::string name;
		std::string description;
		UINT AdapterIndex;

		bool operator==(const DDDeviceInfo& other) const
		{
			return IsEqualGUID(guid, other.guid);
		}
	};

	std::vector<DDDeviceInfo> g_DeviceCache;

	CRITICAL_SECTION* GetDDCriticalSection()
	{
		return IsInitialized ? &ddcs : nullptr;
	}

	CRITICAL_SECTION* GetPECriticalSection()
	{
		return IsInitialized ? &pecs : nullptr;
	}
}

using namespace DdrawWrapper;

static void SetAllAppCompatData();
static HRESULT DirectDrawEnumerateHandler(LPVOID lpCallback, LPVOID lpContext, DWORD dwFlags, DirectDrawEnumerateTypes DDETType);

// ******************************
// ddraw.dll export functions
// ******************************

HRESULT WINAPI dd_AcquireDDThreadLock()
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		if (IsInitialized)
		{
			EnterCriticalSection(&ddcs);
			return DD_OK;
		}
		return DDERR_UNSUPPORTED;
	}

	DEFINE_STATIC_PROC_ADDRESS(AcquireDDThreadLockProc, AcquireDDThreadLock, AcquireDDThreadLock_out);

	if (!AcquireDDThreadLock)
	{
		return DDERR_UNSUPPORTED;
	}

	return AcquireDDThreadLock();
}

DWORD WINAPI dd_CompleteCreateSysmemSurface(DWORD arg)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Not Implemented");
		return NULL;
	}

	DEFINE_STATIC_PROC_ADDRESS(CompleteCreateSysmemSurfaceProc, CompleteCreateSysmemSurface, CompleteCreateSysmemSurface_out);

	if (!CompleteCreateSysmemSurface)
	{
		return NULL;
	}

	return CompleteCreateSysmemSurface(arg);
}

HRESULT WINAPI dd_D3DParseUnknownCommand(LPVOID lpCmd, LPVOID *lpRetCmd)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		if (!lpCmd || !lpRetCmd)
		{
			return DDERR_INVALIDPARAMS;
		}

		const auto* command = static_cast<const D3DHAL_DP2COMMAND*>(lpCmd);
		const BYTE opcode = command->bCommand;
		const WORD count = command->wStateCount;

		const BYTE* base = reinterpret_cast<const BYTE*>(lpCmd);
		size_t advance = 0;

		switch (opcode)
		{
		case D3DDP2OP_VIEWPORTINFO:
			advance = sizeof(D3DHAL_DP2COMMAND) + count * sizeof(D3DHAL_DP2VIEWPORTINFO);
			break;

		case D3DDP2OP_WINFO:
			advance = sizeof(D3DHAL_DP2COMMAND) + count * sizeof(D3DHAL_DP2WINFO);
			break;

		case 0x0D: // Undocumented command
			advance = sizeof(D3DHAL_DP2COMMAND) + count * command->bReserved;
			break;

		default:
			// Command ranges based on original logic
			if (opcode <= D3DDP2OP_INDEXEDTRIANGLELIST ||
				opcode == D3DDP2OP_RENDERSTATE ||
				opcode >= D3DDP2OP_LINELIST)
			{
				return DDERR_INVALIDPARAMS;
			}
			else
			{
				return D3DERR_COMMAND_UNPARSED;
			}
		}

		*lpRetCmd = const_cast<BYTE*>(base + advance);
		return DD_OK;
	}

	DEFINE_STATIC_PROC_ADDRESS(D3DParseUnknownCommandProc, D3DParseUnknownCommand, D3DParseUnknownCommand_out);

	if (!D3DParseUnknownCommand)
	{
		return D3DERR_COMMAND_UNPARSED;
	}

	return D3DParseUnknownCommand(lpCmd, lpRetCmd);
}

HRESULT WINAPI dd_DDGetAttachedSurfaceLcl(DWORD arg1, DWORD arg2, DWORD arg3)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	DEFINE_STATIC_PROC_ADDRESS(DDGetAttachedSurfaceLclProc, DDGetAttachedSurfaceLcl, DDGetAttachedSurfaceLcl_out);

	if (!DDGetAttachedSurfaceLcl)
	{
		return DDERR_UNSUPPORTED;
	}

	return DDGetAttachedSurfaceLcl(arg1, arg2, arg3);
}

DWORD WINAPI dd_DDInternalLock(DWORD arg1, DWORD arg2)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Not Implemented");
		return 0xFFFFFFFF;
	}

	DEFINE_STATIC_PROC_ADDRESS(DDInternalLockProc, DDInternalLock, DDInternalLock_out);

	if (!DDInternalLock)
	{
		return 0xFFFFFFFF;
	}

	return DDInternalLock(arg1, arg2);
}

DWORD WINAPI dd_DDInternalUnlock(DWORD arg)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Not Implemented");
		return 0xFFFFFFFF;
	}

	DEFINE_STATIC_PROC_ADDRESS(DDInternalUnlockProc, DDInternalUnlock, DDInternalUnlock_out);

	if (!DDInternalUnlock)
	{
		return 0xFFFFFFFF;
	}

	return DDInternalUnlock(arg);
}

HRESULT WINAPI dd_DSoundHelp(DWORD arg1, DWORD arg2, DWORD arg3)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	DEFINE_STATIC_PROC_ADDRESS(DSoundHelpProc, DSoundHelp, DSoundHelp_out);

	if (!DSoundHelp)
	{
		return DDERR_UNSUPPORTED;
	}

	return DSoundHelp(arg1, arg2, arg3);
}

HRESULT WINAPI dd_DirectDrawCreate(GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(3, "Redirecting 'DirectDrawCreate' to --> 'Direct3DCreate9'");

		if (Config.SetSwapEffectShim < 2)
		{
			Direct3D9SetSwapEffectUpgradeShim(Config.SetSwapEffectShim);
		}

		m_IDirectDrawX* p_IDirectDrawX = new m_IDirectDrawX(1, GetAdapterIndex(lpGUID), false);

		*lplpDD = reinterpret_cast<LPDIRECTDRAW>(p_IDirectDrawX->GetWrapperInterfaceX(1));

		// Success
		return DD_OK;
	}

	DEFINE_STATIC_PROC_ADDRESS(DirectDrawCreateProc, DirectDrawCreate, DirectDrawCreate_out);

	if (!DirectDrawCreate)
	{
		return DDERR_UNSUPPORTED;
	}

	// Set AppCompatData
	if (Config.isAppCompatDataSet)
	{
		SetAllAppCompatData();
	}

	LOG_LIMIT(3, "Redirecting 'DirectDrawCreate' ...");

	HRESULT hr = DirectDrawCreate(lpGUID, lplpDD, pUnkOuter);

	if (SUCCEEDED(hr) && lplpDD && *lplpDD)
	{
		m_IDirectDrawX* Interface = new m_IDirectDrawX((IDirectDraw7*)*lplpDD, 1);

		*lplpDD = (LPDIRECTDRAW)Interface->GetWrapperInterfaceX(1);
	}

	return hr;
}

HRESULT WINAPI dd_DirectDrawCreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, LPUNKNOWN pUnkOuter)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		if (!lplpDDClipper || pUnkOuter)
		{
			return DDERR_INVALIDPARAMS;
		}

		m_IDirectDrawClipper* ClipperX = m_IDirectDrawClipper::CreateDirectDrawClipper(nullptr, nullptr, dwFlags);

		m_IDirectDrawX::AddBaseClipper(ClipperX);

		*lplpDDClipper = ClipperX;

		return DD_OK;
	}

	DEFINE_STATIC_PROC_ADDRESS(DirectDrawCreateClipperProc, DirectDrawCreateClipper, DirectDrawCreateClipper_out);

	if (!DirectDrawCreateClipper)
	{
		return DDERR_UNSUPPORTED;
	}

	HRESULT hr = DirectDrawCreateClipper(dwFlags, lplpDDClipper, pUnkOuter);

	if (SUCCEEDED(hr) && lplpDDClipper)
	{
		*lplpDDClipper = m_IDirectDrawClipper::CreateDirectDrawClipper(*lplpDDClipper, nullptr, dwFlags);
	}

	return hr;
}

HRESULT WINAPI dd_DirectDrawCreateEx(GUID FAR *lpGUID, LPVOID *lplpDD, REFIID riid, IUnknown FAR *pUnkOuter)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		if (!lplpDD || pUnkOuter)
		{
			return DDERR_INVALIDPARAMS;
		}

		if (riid != IID_IDirectDraw7)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: invalid IID " << riid);
			return DDERR_INVALIDPARAMS;
		}

		LOG_LIMIT(3, "Redirecting 'DirectDrawCreateEx' to --> 'Direct3DCreate9'");

		if (Config.SetSwapEffectShim < 2)
		{
			Direct3D9SetSwapEffectUpgradeShim(Config.SetSwapEffectShim);
		}

		m_IDirectDrawX *p_IDirectDrawX = new m_IDirectDrawX(7, GetAdapterIndex(lpGUID), true);

		*lplpDD = p_IDirectDrawX->GetWrapperInterfaceX(7);

		// Success
		return DD_OK;
	}

	DEFINE_STATIC_PROC_ADDRESS(DirectDrawCreateExProc, DirectDrawCreateEx, DirectDrawCreateEx_out);

	if (!DirectDrawCreateEx)
	{
		return DDERR_UNSUPPORTED;
	}

	// Set AppCompatData
	if (Config.isAppCompatDataSet)
	{
		SetAllAppCompatData();
	}

	LOG_LIMIT(3, "Redirecting 'DirectDrawCreateEx' ...");

	HRESULT hr = DirectDrawCreateEx(lpGUID, lplpDD, IID_IDirectDraw7, pUnkOuter);

	if (SUCCEEDED(hr))
	{
		DWORD DxVersion = GetGUIDVersion(riid);

		m_IDirectDrawX *p_IDirectDrawX = new m_IDirectDrawX((IDirectDraw7*)*lplpDD, DxVersion);

		*lplpDD = p_IDirectDrawX->GetWrapperInterfaceX(DxVersion);
	}

	return hr;
}

HRESULT WINAPI dd_DirectDrawEnumerateA(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		return DirectDrawEnumerateHandler(lpCallback, lpContext, 0, DDET_ENUMCALLBACKA);
	}

	DEFINE_STATIC_PROC_ADDRESS(DirectDrawEnumerateAProc, DirectDrawEnumerateA, DirectDrawEnumerateA_out);

	if (!DirectDrawEnumerateA)
	{
		return DDERR_UNSUPPORTED;
	}

	return DirectDrawEnumerateA(lpCallback, lpContext);
}

HRESULT WINAPI dd_DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		return DirectDrawEnumerateHandler(lpCallback, lpContext, dwFlags, DDET_ENUMCALLBACKEXA);
	}

	DEFINE_STATIC_PROC_ADDRESS(DirectDrawEnumerateExAProc, DirectDrawEnumerateExA, DirectDrawEnumerateExA_out);

	if (!DirectDrawEnumerateExA)
	{
		return DDERR_UNSUPPORTED;
	}

	return DirectDrawEnumerateExA(lpCallback, lpContext, dwFlags);
}

HRESULT WINAPI dd_DirectDrawEnumerateExW(LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		//return DirectDrawEnumerateHandler(lpCallback, lpContext, dwFlags, DDET_ENUMCALLBACKEXW);

		// Just return unsupported to match native DirectDraw
		return DDERR_UNSUPPORTED;
	}

	DEFINE_STATIC_PROC_ADDRESS(DirectDrawEnumerateExWProc, DirectDrawEnumerateExW, DirectDrawEnumerateExW_out);

	if (!DirectDrawEnumerateExW)
	{
		return DDERR_UNSUPPORTED;
	}

	return DirectDrawEnumerateExW(lpCallback, lpContext, dwFlags);
}

HRESULT WINAPI dd_DirectDrawEnumerateW(LPDDENUMCALLBACKW lpCallback, LPVOID lpContext)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		//return DirectDrawEnumerateHandler(lpCallback, lpContext, 0, DDET_ENUMCALLBACKW);

		// Just return unsupported to match native DirectDraw
		return DDERR_UNSUPPORTED;
	}

	DEFINE_STATIC_PROC_ADDRESS(DirectDrawEnumerateWProc, DirectDrawEnumerateW, DirectDrawEnumerateW_out);

	if (!DirectDrawEnumerateW)
	{
		return DDERR_UNSUPPORTED;
	}

	return DirectDrawEnumerateW(lpCallback, lpContext);
}

HRESULT WINAPI dd_DllCanUnloadNow()
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	DEFINE_STATIC_PROC_ADDRESS(DllCanUnloadNowProc, DllCanUnloadNow, DllCanUnloadNow_out);

	if (!DllCanUnloadNow)
	{
		return DDERR_UNSUPPORTED;
	}

	return DllCanUnloadNow();
}

HRESULT WINAPI dd_DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		if (!ppv)
		{
			return E_POINTER;
		}

		HRESULT hr = ProxyQueryInterface(nullptr, riid, ppv, rclsid);

		if (SUCCEEDED(hr) && ppv && *ppv)
		{
			if (riid == IID_IClassFactory)
			{
				((m_IClassFactory*)(*ppv))->SetCLSID(rclsid);
			}
			((IUnknown*)ppv)->AddRef();
		}

		return hr;
	}

	DEFINE_STATIC_PROC_ADDRESS(DllGetClassObjectProc, DllGetClassObject, DllGetClassObject_out);

	if (!DllGetClassObject)
	{
		return DDERR_UNSUPPORTED;
	}

	HRESULT hr = DllGetClassObject(rclsid, riid, ppv);

	if (SUCCEEDED(hr) && ppv)
	{
		if (riid == IID_IClassFactory)
		{
			*ppv = new m_IClassFactory((IClassFactory*)*ppv, genericQueryInterface);

			((m_IClassFactory*)(*ppv))->SetCLSID(rclsid);

			return DD_OK;
		}

		genericQueryInterface(riid, ppv);
	}

	return hr;
}

HRESULT WINAPI dd_GetDDSurfaceLocal(DWORD arg1, DWORD arg2, DWORD arg3)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	DEFINE_STATIC_PROC_ADDRESS(GetDDSurfaceLocalProc, GetDDSurfaceLocal, GetDDSurfaceLocal_out);

	if (!GetDDSurfaceLocal)
	{
		return DDERR_UNSUPPORTED;
	}

	return GetDDSurfaceLocal(arg1, arg2, arg3);
}

DWORD WINAPI dd_GetOLEThunkData(DWORD index)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		//	switch(index) 
		//	{
		//		case 1: return _dwLastFrameRate;
		//		case 2: return _lpDriverObjectList;
		//		case 3: return _lpAttachedProcesses;
		//		case 4: return 0; // does nothing?
		//		case 5: return _CheckExclusiveMode;
		//		case 6: return 0; // ReleaseExclusiveModeMutex
		//	}

		LOG_LIMIT(100, __FUNCTION__ << " Not Implemented");
		return NULL;
	}

	DEFINE_STATIC_PROC_ADDRESS(GetOLEThunkDataProc, GetOLEThunkData, GetOLEThunkData_out);

	if (!GetOLEThunkData)
	{
		return NULL;
	}

	return GetOLEThunkData(index);
}

HRESULT WINAPI dd_GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE7 *lpDDS, DWORD arg)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	DEFINE_STATIC_PROC_ADDRESS(GetSurfaceFromDCProc, GetSurfaceFromDC, GetSurfaceFromDC_out);

	if (!GetSurfaceFromDC)
	{
		return DDERR_UNSUPPORTED;
	}

	return GetSurfaceFromDC(hdc, lpDDS, arg);
}

HRESULT WINAPI dd_RegisterSpecialCase(DWORD arg1, DWORD arg2, DWORD arg3, DWORD arg4)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	DEFINE_STATIC_PROC_ADDRESS(RegisterSpecialCaseProc, RegisterSpecialCase, RegisterSpecialCase_out);

	if (!RegisterSpecialCase)
	{
		return DDERR_UNSUPPORTED;
	}

	return RegisterSpecialCase(arg1, arg2, arg3, arg4);
}

HRESULT WINAPI dd_ReleaseDDThreadLock()
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		if (IsInitialized)
		{
			LeaveCriticalSection(&ddcs);
			return DD_OK;
		}
		return DDERR_UNSUPPORTED;
	}

	DEFINE_STATIC_PROC_ADDRESS(ReleaseDDThreadLockProc, ReleaseDDThreadLock, ReleaseDDThreadLock_out);

	if (!ReleaseDDThreadLock)
	{
		return DDERR_UNSUPPORTED;
	}

	return ReleaseDDThreadLock();
}

HRESULT WINAPI dd_SetAppCompatData(DWORD Type, DWORD Value)
{
	LOG_LIMIT(1, __FUNCTION__);

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Skipping compatibility flags: " << Type << " " << Value);
		return DD_OK;
	}

	DEFINE_STATIC_PROC_ADDRESS(SetAppCompatDataProc, SetAppCompatData, SetAppCompatData_out);

	if (!SetAppCompatData)
	{
		return DDERR_GENERIC;
	}

	return SetAppCompatData(Type, Value);
}

// ******************************
// Helper functions
// ******************************

void InitDDraw()
{
	if (!IsInitialized)
	{
		if (!InitializeCriticalSectionAndSpinCount(&ddcs, 4000))
		{
			InitializeCriticalSection(&ddcs);
		}
		if (!InitializeCriticalSectionAndSpinCount(&pecs, 4000))
		{
			InitializeCriticalSection(&pecs);
		}
		IsInitialized = true;
	}

	// Hook other gdi32 and user32 APIs
	static bool RunOnce = true;
	if (RunOnce)
	{
		Logging::Log() << "Installing GDI & User32 hooks";
		using namespace GdiWrapper;
		if (!GetModuleHandleA("gdi32.dll")) LoadLibrary("gdi32.dll");
		if (!GetModuleHandleA("user32.dll")) LoadLibrary("user32.dll");
		HMODULE gdi32 = GetModuleHandleA("gdi32.dll");
		HMODULE user32 = GetModuleHandleA("user32.dll");
		HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
		if (gdi32)
		{
			GetDeviceCaps_out = (FARPROC)Hook::HotPatch(GetProcAddress(gdi32, "GetDeviceCaps"), "GetDeviceCaps", gdi_GetDeviceCaps);
		}
		if (user32)
		{
			CreateWindowExA_out = (FARPROC)Hook::HotPatch(GetProcAddress(user32, "CreateWindowExA"), "CreateWindowExA", user_CreateWindowExA);
			CreateWindowExW_out = (FARPROC)Hook::HotPatch(GetProcAddress(user32, "CreateWindowExW"), "CreateWindowExW", user_CreateWindowExW);
			DestroyWindow_out = (FARPROC)Hook::HotPatch(GetProcAddress(user32, "DestroyWindow"), "DestroyWindow", user_DestroyWindow);
			GetSystemMetrics_out = (FARPROC)Hook::HotPatch(GetProcAddress(user32, "GetSystemMetrics"), "GetSystemMetrics", user_GetSystemMetrics);
			//GetWindowLongA_out = (FARPROC)Hook::HotPatch(GetProcAddress(user32, "GetWindowLongA"), "GetWindowLongA", user_GetWindowLongA);
			//GetWindowLongW_out = (FARPROC)Hook::HotPatch(GetProcAddress(user32, "GetWindowLongW"), "GetWindowLongW", user_GetWindowLongW);
			//SetWindowLongA_out = (FARPROC)Hook::HotPatch(GetProcAddress(user32, "SetWindowLongA"), "SetWindowLongA", user_SetWindowLongA);
			//SetWindowLongW_out = (FARPROC)Hook::HotPatch(GetProcAddress(user32, "SetWindowLongW"), "SetWindowLongW", user_SetWindowLongW);
		}
		if (kernel32)
		{
			Logging::Log() << "Installing Kernel32 hooks";
			Utils::GetDiskFreeSpaceA_out = (FARPROC)Hook::HotPatch(GetProcAddress(kernel32, "GetDiskFreeSpaceA"), "GetDiskFreeSpaceA", Utils::kernel_GetDiskFreeSpaceA);
			if (!Utils::CreateThread_out)
			{
				Utils::CreateThread_out = (FARPROC)Hook::HotPatch(GetProcAddress(kernel32, "CreateThread"), "CreateThread", Utils::kernel_CreateThread);
			}
			Utils::CreateFileA_out = (FARPROC)Hook::HotPatch(GetProcAddress(kernel32, "CreateFileA"), "CreateFileA", Utils::kernel_CreateFileA);
			Utils::VirtualAlloc_out = (FARPROC)Hook::HotPatch(GetProcAddress(kernel32, "VirtualAlloc"), "VirtualAlloc", Utils::kernel_VirtualAlloc);
			//Utils::HeapAlloc_out = (FARPROC)Hook::HotPatch(GetProcAddress(kernel32, "HeapAlloc"), "HeapAlloc", Utils::kernel_HeapAlloc);
			Utils::HeapSize_out = (FARPROC)Hook::HotPatch(GetProcAddress(kernel32, "HeapSize"), "HeapSize", Utils::kernel_HeapSize);
		}
		RunOnce = false;
	}
}

void ExitDDraw()
{
	if (IsInitialized)
	{
		{
			ScopedCriticalSection ThreadLockDD(&ddcs);
			ScopedCriticalSection ThreadLockPE(&pecs);

			IsInitialized = false;
		}
		DeleteCriticalSection(&ddcs);
		DeleteCriticalSection(&pecs);
	}
}

// Sets Application Compatibility Toolkit options for DXPrimaryEmulation using SetAppCompatData API
static void SetAllAppCompatData()
{
	DEFINE_STATIC_PROC_ADDRESS(SetAppCompatDataProc, SetAppCompatData, SetAppCompatData_out);

	if (!SetAppCompatData)
	{
		Logging::Log() << __FUNCTION__ << " Error: Failed to get `SetAppCompatData` address!";
		return;
	}

	// Set AppCompatData
	for (DWORD x = 1; x <= 12; x++)
	{
		if (Config.DXPrimaryEmulation[x])
		{
			Logging::Log() << __FUNCTION__ << " SetAppCompatData: " << x << " " << (DWORD)((x == AppCompatDataType.LockColorkey) ? AppCompatDataType.LockColorkey : 0);

			// For LockColorkey, this one uses the second parameter
			if (x == AppCompatDataType.LockColorkey)
			{
				SetAppCompatData(x, Config.LockColorkey);
			}
			// For all the other items
			else
			{
				SetAppCompatData(x, 0);
			}
		}
	}
	return;
}

static UINT GetAdapterIndex(GUID* lpGUID)
{
	if (!lpGUID)
	{
		return D3DADAPTER_DEFAULT;
	}

	DDDeviceInfo info = {};
	info.guid = *lpGUID;

	auto it = std::find(g_DeviceCache.begin(), g_DeviceCache.end(), info);
	if (it != g_DeviceCache.end())
	{
		return it->AdapterIndex;
	}

	return D3DADAPTER_DEFAULT;
}

static bool FindGUIDByDeviceName(const std::string& deviceName, const std::string& deviceDesc, GUID& outGuid)
{
	for (const auto& device : g_DeviceCache)
	{
		if (device.name == deviceName && device.description == deviceDesc)
		{
			outGuid = device.guid;
			return true;
		}
	}
	return false;
}

static void StoreDeviceCache(DDDeviceInfo& info)
{
	auto it = std::find(g_DeviceCache.begin(), g_DeviceCache.end(), info);
	if (it == g_DeviceCache.end())
	{
		g_DeviceCache.push_back(info);
	}
	else
	{
		it->description = info.description;
		it->name = info.name;
		it->AdapterIndex = info.AdapterIndex;
	}
}

static HRESULT DirectDrawEnumerateHandler(LPVOID lpCallback, LPVOID lpContext, DWORD dwFlags, DirectDrawEnumerateTypes DDETType)
{
	if (!lpCallback)
	{
		return DDERR_INVALIDPARAMS;
	}

	// Declare Direct3DCreate9
	DEFINE_STATIC_PROC_ADDRESS(Direct3DCreate9Proc, Direct3DCreate9, Direct3DCreate9_out);

	if (!Direct3DCreate9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: failed to get 'Direct3DCreate9' ProcAddress of d3d9.dll!");
		return DDERR_UNSUPPORTED;
	}

	// Create Direct3D9 object
	ComPtr<IDirect3D9> d3d9Object(Direct3DCreate9(D3D_SDK_VERSION));

	// Error handling
	if (!d3d9Object)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: failed to create Direct3D9 object");
		return DDERR_UNSUPPORTED;
	}
	D3DADAPTER_IDENTIFIER9 Identifier = {};
	int AdapterCount = (dwFlags & DDENUM_ATTACHEDSECONDARYDEVICES) != DDENUM_ATTACHEDSECONDARYDEVICES ? 0 : (int)d3d9Object->GetAdapterCount();

	GUID myGUID = {};
	GUID lastGUID = {};
	GUID* lpGUID;
	LPSTR lpDesc, lpName;
	wchar_t lpwName[32] = { '\0' };
	wchar_t lpwDesc[128] = { '\0' };
	HMONITOR hMonitor;
	HRESULT hr = DD_OK;

	for (int Adapter = -1; Adapter < AdapterCount; Adapter++)
	{
		if (Adapter == -1)
		{
			lpGUID = nullptr;
			lpDesc = "Primary Display Driver";
			lpName = "display";
			hMonitor = nullptr;
		}
		else
		{
			if (FAILED(d3d9Object->GetAdapterIdentifier(Adapter, 0, &Identifier)))
			{
				hr = DDERR_UNSUPPORTED;
				break;
			}

			// Get GUID
			lpGUID = &myGUID;
			{
				if (!FindGUIDByDeviceName(Identifier.DeviceName, Identifier.Description, myGUID))
				{
					// Set unique GUID
					bool ReusingGUID = IsEqualGUID(lastGUID, Identifier.DeviceIdentifier) == TRUE;
					myGUID = Identifier.DeviceIdentifier;
					if (ReusingGUID)
					{
						myGUID.Data1++;
					}
				}

				// Get device info
				DDDeviceInfo info = {};
				info.guid = myGUID;
				info.description = Identifier.Description;
				info.name = Identifier.DeviceName;
				info.AdapterIndex = Adapter;

				// Store device info or update existing cache
				StoreDeviceCache(info);

				lastGUID = Identifier.DeviceIdentifier;
			}

			// Get name and description
			lpDesc = Identifier.Description;
			lpName = Identifier.DeviceName;

			// Get monitor handle
			hMonitor = nullptr;
			if (DDETType == DDET_ENUMCALLBACKEXA || DDETType == DDET_ENUMCALLBACKEXW)
			{
				hMonitor = Utils::GetMonitorFromDeviceName(Identifier.DeviceName);
			}
		}

		if (DDETType == DDET_ENUMCALLBACKEXW || DDETType == DDET_ENUMCALLBACKW)
		{
			size_t nReturn;
			mbstowcs_s(&nReturn, lpwName, lpName, 32);
			mbstowcs_s(&nReturn, lpwDesc, lpDesc, 128);
		}

		BOOL hr_Callback = DDENUMRET_OK;
		switch (DDETType)
		{
		case DDET_ENUMCALLBACKA:
			hr_Callback = LPDDENUMCALLBACKA(lpCallback)(lpGUID, lpDesc, lpName, lpContext);
			break;
		case DDET_ENUMCALLBACKEXA:
			hr_Callback = LPDDENUMCALLBACKEXA(lpCallback)(lpGUID, lpDesc, lpName, lpContext, hMonitor);
			break;
		case DDET_ENUMCALLBACKEXW:
			hr_Callback = LPDDENUMCALLBACKEXW(lpCallback)(lpGUID, lpwDesc, lpwName, lpContext, hMonitor);
			break;
		case DDET_ENUMCALLBACKW:
			hr_Callback = LPDDENUMCALLBACKW(lpCallback)(lpGUID, lpwDesc, lpwName, lpContext);
			break;
		default:
			hr_Callback = DDENUMRET_CANCEL;
			hr = DDERR_UNSUPPORTED;
		}

		if (hr_Callback == DDENUMRET_CANCEL)
		{
			break;
		}
	}

	return hr;
}
