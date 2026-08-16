module;

#include <windows.h>

export module globals;

export constexpr GUID CLSID_CASIO = { .Data1 = 0x4d054273, .Data2 = 0x0cb8, .Data3 = 0x4390, .Data4 = { 0x94, 0xaa, 0xc0, 0x3e, 0x94, 0x85, 0x4f, 0x6c }};

export HMODULE this_module = nullptr;

export int global_ref_count = 0;
