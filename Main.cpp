/*
 * ===================================================================
 *  Dragon Ball Z: Kakarot (HD) - FPS Unlocker & ASI Mod
 *  Game       : AT-Win64-Shipping.exe (HD 1.40) (Unreal Engine 4.27)
 *  Version    : 1.1.0.0
 *  Authors    : Talha2003 & Gantz79
 *  Compiler   : MinGW-w64 (GCC) COMPATIBLE
 * ===================================================================
 */

#include <windows.h>
#include <psapi.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

static float g_ManualFPSOverride = 0.0f;

static const uintptr_t FPS_DATA_OFFSET = 0x0700C228;

static const BYTE   AOB_PATTERN[] = { 0xF3, 0x0F, 0x11, 0x4F, 0x48 };
static const size_t AOB_LEN       = sizeof(AOB_PATTERN);

static const uintptr_t EXPECTED_TEXT_OFFSET = 0x2660EA8;

static void LogDebug(const char* msg)
{
#ifdef _DEBUG
    OutputDebugStringA(msg);
#else
    (void)msg;
#endif
}

static bool IsRangeCommitted(const void* address, size_t size)
{
    const BYTE* cur = reinterpret_cast<const BYTE*>(address);
    const BYTE* end = cur + size;

    while (cur < end)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(cur, &mbi, sizeof(mbi)) == 0)
            return false;

        if (mbi.State != MEM_COMMIT)
            return false;
        if (mbi.Protect == PAGE_NOACCESS || (mbi.Protect & PAGE_GUARD))
            return false;

        const BYTE* regionEnd = reinterpret_cast<const BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
        cur = regionEnd;
    }
    return true;
}

struct FindWindowCtx
{
    DWORD pid;
    HWND  result;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    FindWindowCtx* ctx = reinterpret_cast<FindWindowCtx*>(lParam);

    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid != ctx->pid)
        return TRUE;

    if (!IsWindowVisible(hwnd))
        return TRUE;

    if (GetWindow(hwnd, GW_OWNER) != nullptr)
        return TRUE;

    if (GetWindowTextLengthW(hwnd) == 0)
        return TRUE;

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc))
        return TRUE;

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 200 || h < 150)
        return TRUE;

    ctx->result = hwnd;
    return FALSE;
}

static HWND FindProcessMainWindow(DWORD pid)
{
    FindWindowCtx ctx{ pid, nullptr };
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.result;
}

static HWND WaitForGameReallyReady(DWORD pid, DWORD timeoutMs)
{
    DWORD start = GetTickCount();
    HWND candidate = nullptr;

    while (GetTickCount() - start < timeoutMs)
    {
        candidate = FindProcessMainWindow(pid);
        if (candidate)
        {
            Sleep(1000);
            if (IsWindow(candidate) && IsWindowVisible(candidate))
            {
                HWND recheck = FindProcessMainWindow(pid);
                if (recheck == candidate)
                    return candidate;
            }
            candidate = nullptr;
        }
        Sleep(250);
    }
    return nullptr;
}

static float DetectTargetFPS(HWND hwnd)
{
    if (g_ManualFPSOverride > 0.0f)
        return g_ManualFPSOverride;

    const float kFallback = 60.0f;

    POINT origin{ 0, 0 };
    HMONITOR hMon = hwnd ? MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY)
                          : MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFOEXW monInfo{};
    monInfo.cbSize = sizeof(monInfo);

    if (hMon && GetMonitorInfoW(hMon, &monInfo))
    {
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettingsW(monInfo.szDevice, ENUM_CURRENT_SETTINGS, &dm))
        {
            if (dm.dmDisplayFrequency > 1)
                return static_cast<float>(dm.dmDisplayFrequency);
        }
    }

    DEVMODEW dmPrimary{};
    dmPrimary.dmSize = sizeof(dmPrimary);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dmPrimary))
    {
        if (dmPrimary.dmDisplayFrequency > 1)
            return static_cast<float>(dmPrimary.dmDisplayFrequency);
    }

    LogDebug("Could not detect monitor refresh rate, falling back to 60.\n");
    return kFallback;
}

static bool CheckBytesAtAddress(const void* address, const BYTE* expected, size_t len)
{
    if (!IsRangeCommitted(address, len))
        return false;
    return memcmp(address, expected, len) == 0;
}

static bool SafePatchBytes(void* address, const BYTE* newBytes, size_t len)
{
    if (!IsRangeCommitted(address, len))
        return false;

    DWORD oldProtect;
    if (!VirtualProtect(address, len, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    memcpy(address, newBytes, len);

    DWORD dummy;
    VirtualProtect(address, len, oldProtect, &dummy);
    return true;
}

static bool SafeReadFloat(uintptr_t address, float* outValue)
{
    if (!IsRangeCommitted(reinterpret_cast<void*>(address), sizeof(float)))
        return false;

    *outValue = *reinterpret_cast<float*>(address);
    return true;
}

static bool SafeWriteTargetFPS(uintptr_t moduleBase, size_t moduleSize, float targetFPS)
{
    if (FPS_DATA_OFFSET + sizeof(float) * 2 > moduleSize)
    {
        LogDebug("FPS data offset falls outside the module image - aborting write.\n");
        return false;
    }

    void* primary   = reinterpret_cast<void*>(moduleBase + FPS_DATA_OFFSET);
    void* secondary = reinterpret_cast<void*>(moduleBase + FPS_DATA_OFFSET + 0x4);

    if (!IsRangeCommitted(primary, sizeof(float) * 2))
        return false;

    DWORD oldProtect;
    if (!VirtualProtect(primary, sizeof(float) * 2, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    *reinterpret_cast<float*>(primary)   = targetFPS;
    *reinterpret_cast<float*>(secondary) = targetFPS;

    DWORD dummy;
    VirtualProtect(primary, sizeof(float) * 2, oldProtect, &dummy);
    return true;
}

static DWORD WINAPI MainThread(LPVOID)
{
    HMODULE hModule = nullptr;
    for (int attempt = 0; attempt < 200 && !hModule; ++attempt)
    {
        hModule = GetModuleHandleA("AT-Win64-Shipping.exe");
        if (!hModule)
            Sleep(250);
    }
    if (!hModule)
    {
        LogDebug("AT-Win64-Shipping.exe never appeared - giving up.\n");
        return 1;
    }

    DWORD pid = GetCurrentProcessId();
    HWND gameWindow = WaitForGameReallyReady(pid, 90000);
    if (!gameWindow)
    {
        LogDebug("Timed out waiting for a real game window - aborting to be safe.\n");
        return 1;
    }
    LogDebug("Game window detected and stable - proceeding.\n");

    MODULEINFO modInfo{};
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo)))
        return 1;

    uintptr_t moduleBase = reinterpret_cast<uintptr_t>(hModule);
    size_t moduleSize    = modInfo.SizeOfImage;

    void* patchAddress = reinterpret_cast<void*>(moduleBase + EXPECTED_TEXT_OFFSET);

    if (EXPECTED_TEXT_OFFSET + AOB_LEN > moduleSize)
    {
        LogDebug("Target code offset falls outside the module image - aborting.\n");
        return 1;
    }

    if (!CheckBytesAtAddress(patchAddress, AOB_PATTERN, AOB_LEN))
    {
        LogDebug("Byte assert failed at the expected offset - this game build doesn't "
                 "match the CT file. Aborting without touching memory.\n");
        return 1;
    }

    static const BYTE NOP5[AOB_LEN] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
    if (!SafePatchBytes(patchAddress, NOP5, AOB_LEN))
    {
        LogDebug("Failed to patch write-back instruction - aborting.\n");
        return 1;
    }

    float targetFPS = DetectTargetFPS(gameWindow);
    char logBuf[160];
    snprintf(logBuf, sizeof(logBuf), "Auto-detected target FPS: %.2f\n", targetFPS);
    LogDebug(logBuf);

    bool wrote = false;
    for (int attempt = 0; attempt < 5 && !wrote; ++attempt)
    {
        wrote = SafeWriteTargetFPS(moduleBase, moduleSize, targetFPS);
        if (!wrote)
            Sleep(200);
    }
    if (!wrote)
    {
        LogDebug("Could not write target FPS value - verify FPS_DATA_OFFSET for this game build.\n");
        return 1;
    }

    Sleep(1500);
    float readBack = 0.0f;
    if (SafeReadFloat(moduleBase + FPS_DATA_OFFSET, &readBack))
    {
        if (readBack != targetFPS)
        {
            LogDebug("Value drifted after late init, re-applying once.\n");
            SafeWriteTargetFPS(moduleBase, moduleSize, targetFPS);
        }
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
