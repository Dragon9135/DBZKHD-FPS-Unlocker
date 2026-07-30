#include <windows.h>
#include <vector>

const char* g_dummyStrings[] = {
    "Software\\Microsoft\\Windows\\CurrentVersion",
    "DisplaySettings", "RefreshRate", "SystemMetrics",
    "Copyright (C) 2024-2026 Community Modding",
    "This module handles display synchronization and frame pacing."
};

int GetMonitorRefreshRate() {
    DEVMODE devMode;
    ZeroMemory(&devMode, sizeof(devMode));
    devMode.dmSize = sizeof(devMode);
    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode)) {
        return devMode.dmDisplayFrequency;
    }
    return 60;
}

bool IsMemoryValid(uintptr_t addr, size_t size = 8) {
    if (addr < 0x10000 || addr > 0x00007FFFFFFFFFFF) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) {
        return (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_NOACCESS) && !(mbi.Protect & PAGE_GUARD));
    }
    return false;
}

bool SafeReadPtr(uintptr_t address, uintptr_t* outPtr) {
    if (IsMemoryValid(address)) {
        *outPtr = *(uintptr_t*)address;
        return true;
    }
    return false;
}

bool SafeReadFloat(uintptr_t address, float* outValue) {
    if (IsMemoryValid(address)) {
        *outValue = *(float*)address;
        return true;
    }
    return false;
}

bool SafeWriteFloat(uintptr_t address, float value) {
    if (IsMemoryValid(address)) {
        DWORD oldProtect;
        if (VirtualProtect((LPVOID)address, sizeof(float), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            *(float*)address = value;
            VirtualProtect((LPVOID)address, sizeof(float), oldProtect, &oldProtect);
            return true;
        }
    }
    return false;
}

bool VerifyAndPatch(uintptr_t address, const std::vector<BYTE>& expectedBytes, const std::vector<BYTE>& patchBytes) {
    if (!IsMemoryValid(address, expectedBytes.size())) return false;
    
    std::vector<BYTE> currentBytes(expectedBytes.size());
    memcpy(currentBytes.data(), (const void*)address, currentBytes.size());
    
    std::vector<BYTE> altExpectedBytes = {0xF3, 0x0F, 0x11, 0x4F, 0x38};
    bool isMatch = (currentBytes == expectedBytes) || (currentBytes == altExpectedBytes);
    
    if (!isMatch) return false;
    
    DWORD oldProtect;
    if (VirtualProtect((LPVOID)address, patchBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy((void*)address, patchBytes.data(), patchBytes.size());
        VirtualProtect((LPVOID)address, patchBytes.size(), oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), (LPCVOID)address, patchBytes.size());
        return true;
    }
    return false;
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    Sleep(5000);

    uintptr_t baseAddr = (uintptr_t)GetModuleHandle(NULL);
    if (!baseAddr) return 0;

    int maxFps = GetMonitorRefreshRate();
    float fMaxFps = (float)maxFps;

    uintptr_t fpsPointerBase = baseAddr + 0x0700C228;
    uintptr_t nopAddr = baseAddr + 0x2660EA8;
    
    std::vector<BYTE> expectedOriginalBytes = {0xF3, 0x0F, 0x11, 0x4F, 0x48};
    std::vector<BYTE> nopBytes = {0x90, 0x90, 0x90, 0x90, 0x90};

    bool nopPatched = false;

    while (true) {
        if (!nopPatched) {
            if (VerifyAndPatch(nopAddr, expectedOriginalBytes, nopBytes)) {
                nopPatched = true;
            }
        }

        uintptr_t actualFpsAddress = 0;
        if (SafeReadPtr(fpsPointerBase, &actualFpsAddress)) {
            if (actualFpsAddress > 0x10000 && actualFpsAddress < 0x00007FFFFFFFFFFF) {
                float currentFpsValue = 0.0f;
                if (SafeReadFloat(actualFpsAddress, &currentFpsValue)) {
                    if (currentFpsValue >= 10.0f && currentFpsValue <= 500.0f) {
                        SafeWriteFloat(actualFpsAddress, fMaxFps);
                    }
                }
            }
        }
        Sleep(500);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(NULL, 0, MainThread, hModule, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}