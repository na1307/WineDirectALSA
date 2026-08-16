#include <string>
#include <array>

#include <Windows.h>
#include <olectl.h>

#include "wine_unixlib.h"

import globals;
import casioclassfactory;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        this_module = hModule;

        if (init_linux_call() != 0) {
            return FALSE;
        }
    }

    return TRUE;
}

HRESULT DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
    if (!IsEqualCLSID(rclsid, CLSID_CASIO)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    if (ppv == nullptr) {
        return E_POINTER;
    }

    *ppv = nullptr;
    const auto factory = new CASIOClassFactory;

    factory->AddRef();

    const auto hr = factory->QueryInterface(riid, ppv);

    factory->Release();

    return hr;
}

HRESULT DllCanUnloadNow() {
    return global_ref_count <= 0 ? S_OK : S_FALSE;
}

HRESULT DllRegisterServer() {
    HKEY clsidKey;

    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Classes\CLSID\{4d054273-0cb8-4390-94aa-c03e94854f6c})", 0, nullptr, 0, KEY_WRITE, nullptr,
        &clsidKey, nullptr) != ERROR_SUCCESS) {
        return E_UNEXPECTED;
    }

    const std::string desc("WineDirectALSA Driver");

    if (RegSetValueExA(clsidKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(desc.c_str()), desc.length() + 1) != ERROR_SUCCESS) {
        return E_UNEXPECTED;
    }

    HKEY inprocKey;

    if (RegCreateKeyExA(clsidKey, "InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &inprocKey, nullptr) != ERROR_SUCCESS) {
        return E_UNEXPECTED;
    }

    std::array<char, MAX_PATH> path{};

    GetModuleFileNameA(this_module, path.data(), MAX_PATH);

    if (RegSetValueExA(inprocKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(path.data()), std::strlen(path.data()) + 1) != ERROR_SUCCESS) {
        return E_UNEXPECTED;
    }

    constexpr std::string apartment("Apartment");

    if (RegSetValueExA(inprocKey, "ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(apartment.c_str()), apartment.length() + 1) !=
        ERROR_SUCCESS) {
        return E_UNEXPECTED;
    }

    RegCloseKey(inprocKey);
    RegCloseKey(clsidKey);

    HKEY wdaKey;

    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\ASIO\\WineDirectALSA", 0, nullptr, 0, KEY_WRITE, nullptr, &wdaKey, nullptr) != ERROR_SUCCESS) {
        return E_UNEXPECTED;
    }

    const std::string clsid("{4d054273-0cb8-4390-94aa-c03e94854f6c}");

    if (RegSetValueExA(wdaKey, "CLSID", 0, REG_SZ, reinterpret_cast<const BYTE*>(clsid.c_str()), clsid.length() + 1) != ERROR_SUCCESS) {
        return E_UNEXPECTED;
    }

    if (RegSetValueExA(wdaKey, "Description", 0, REG_SZ, reinterpret_cast<const BYTE*>(desc.c_str()), desc.length() + 1) != ERROR_SUCCESS) {
        return E_UNEXPECTED;
    }

    RegCloseKey(wdaKey);

    return S_OK;
}

HRESULT DllUnregisterServer() {
    auto result = RegDeleteTreeA(HKEY_LOCAL_MACHINE, "SOFTWARE\\ASIO\\WineDirectALSA");

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        return E_UNEXPECTED;
    }

    result = RegDeleteTreeA(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Classes\CLSID\{4d054273-0cb8-4390-94aa-c03e94854f6c})");

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        return E_UNEXPECTED;
    }

    return S_OK;
}
