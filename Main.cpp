/*
 * ===================================================================
 *  Dragon Ball Z: Kakarot (HD) - FPS Unlocker & ASI Mod
 *  Game       : AT-Win64-Shipping.exe (HD 1.40) (Unreal Engine 4.27)
 *  Version    : 1.1.0.0
 *  Authors    : Talha2003 & Gantz79
 *  Compiler   : MinGW-w64 (GCC) COMPATIBLE
 * ===================================================================
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif

#include <windows.h>
#include <psapi.h>
#include <vector>
#include <cstdint>

constexpr DWORD MAX_WAIT_TIME_MS  = 180000;
constexpr DWORD POLL_INTERVAL_MS  = 500;

static void PatchMemory(uintptr_t address, const std::vector<uint8_t>& patch) {
    DWORD oldProtect;
    if (VirtualProtect(reinterpret_cast<LPVOID>(address), patch.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(reinterpret_cast<LPVOID>(address), patch.data(), patch.size());
        VirtualProtect(reinterpret_cast<LPVOID>(address), patch.size(), oldProtect, &oldProtect);
    }
}

static uintptr_t FindPattern(uintptr_t baseAddress, size_t size, const std::vector<int>& pattern) {
    size_t patternSize = pattern.size();
    uintptr_t endAddress = baseAddress + size - patternSize;

    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t i = baseAddress;

    while (i < endAddress) {
        if (VirtualQuery(reinterpret_cast<LPCVOID>(i), &mbi, sizeof(mbi)) == 0) {
            i += 0x1000;
            continue;
        }

        if (mbi.State != MEM_COMMIT ||
            (mbi.Protect & PAGE_NOACCESS) ||
            (mbi.Protect & PAGE_GUARD)) {
            i = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            continue;
        }

        uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (regionEnd > endAddress) regionEnd = endAddress;

        uint8_t* scanBytes = reinterpret_cast<uint8_t*>(i);
        size_t regionSize = regionEnd - i;

        for (size_t j = 0; j + patternSize <= regionSize; ++j) {
            bool found = true;
            for (size_t k = 0; k < patternSize; ++k) {
                if (pattern[k] != -1 && scanBytes[j + k] != static_cast<uint8_t>(pattern[k])) {
                    found = false;
                    break;
                }
            }
            if (found) return i + j;
        }

        i = regionEnd;
    }
    return 0;
}

static float GetMonitorRefreshRate() {
    DEVMODEW devMode = {};
    devMode.dmSize = sizeof(devMode);
    if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &devMode)) {
        return static_cast<float>(devMode.dmDisplayFrequency);
    }
    return 60.0f;
}

static bool IsGameReady(uintptr_t baseAddress, size_t moduleSize, const std::vector<int>& pattern) {
    HWND hWnd = FindWindowW(L"UnrealWindow", NULL);
    if (!hWnd) return false;
    return FindPattern(baseAddress, moduleSize, pattern) != 0;
}

static DWORD WINAPI MainThread(LPVOID hModule) {
    (void)hModule;

    uintptr_t baseAddress = 0;
    for (int i = 0; i < 60 && !baseAddress; ++i) {
        baseAddress = reinterpret_cast<uintptr_t>(GetModuleHandleA("AT-Win64-Shipping.exe"));
        if (!baseAddress) Sleep(500);
    }
    if (!baseAddress) return 0;

    MODULEINFO modInfo = {};
    if (!GetModuleInformation(GetCurrentProcess(), reinterpret_cast<HMODULE>(baseAddress), &modInfo, sizeof(modInfo))) {
        return 0;
    }
    size_t moduleSize = modInfo.SizeOfImage;

    std::vector<int> fpsPattern = { 0xF3, 0x0F, 0x11, 0x4F, -1 };

    DWORD startTime = GetTickCount();
    while (GetTickCount() - startTime < MAX_WAIT_TIME_MS) {
        if (IsGameReady(baseAddress, moduleSize, fpsPattern)) break;
        Sleep(POLL_INTERVAL_MS);
    }

    uintptr_t patchAddress = FindPattern(baseAddress, moduleSize, fpsPattern);
    if (!patchAddress) {
        patchAddress = baseAddress + 0x1A9E8F5;
    }

    float targetFPS = GetMonitorRefreshRate();

    std::vector<uint8_t> nopPatch = { 0x90, 0x90, 0x90, 0x90, 0x90 };
    PatchMemory(patchAddress, nopPatch);

    uintptr_t fpsPointerAddress = baseAddress + 0x0700C228;
    float* fpsValue = reinterpret_cast<float*>(fpsPointerAddress);

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(fpsValue, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT) {
        DWORD oldProtect;
        if (VirtualProtect(fpsValue, sizeof(float), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            *fpsValue = targetFPS;
            VirtualProtect(fpsValue, sizeof(float), oldProtect, &oldProtect);
        }
    }

    return 0;
}

extern "C" BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;

    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
