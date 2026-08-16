#include <array>
#include <string>
#include <vector>

#include <windows.h>

#include "resource.h"
#include "linux_functions.h"

import wda;

namespace {
    struct DialogState {
        std::vector<device> devices;
    };
}

static bool RefreshDevices(DialogState &state) {
    devices query{
        .devices = nullptr,
        .capacity = 0,
        .length = 0
    };

    if (!WDA_GetDevices(&query)) {
        return false;
    }

    std::vector<device> new_devices(query.length);

    devices result{
        .devices = new_devices.data(),
        .capacity = static_cast<uint32_t>(new_devices.size()),
        .length = 0
    };

    if (!WDA_GetDevices(&result)) {
        return false;
    }

    new_devices.resize(result.length);

    state.devices = std::move(new_devices);

    return true;
}

static std::string GetSelectedDeviceId(HWND combo, const DialogState &state) {
    const auto selected = SendMessageA(combo, CB_GETCURSEL, 0, 0);

    if (selected == CB_ERR) {
        return {};
    }

    const auto data = SendMessageA(combo, CB_GETITEMDATA, selected, 0);

    if (data == CB_ERR) {
        return {};
    }

    if (data == -2) {
        return "(None)";
    }

    if (data < 0) {
        return {};
    }

    const auto index = static_cast<size_t>(data);

    if (index >= state.devices.size()) {
        return {};
    }

    return state.devices[index].id;
}

static void PopulateDeviceLists(HWND output, HWND input, const DialogState &state, const std::string &output_id, const std::string &input_id) {
    SendMessageA(output, CB_RESETCONTENT, 0, 0);
    SendMessageA(input, CB_RESETCONTENT, 0, 0);

    const auto none_index = SendMessageA(input, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("(None)"));

    SendMessageA(input, CB_SETITEMDATA, none_index, -2);

    // 기본값
    SendMessageA(input, CB_SETCURSEL, none_index, 0);

    for (size_t i = 0; i < state.devices.size(); ++i) {
        const auto &item = state.devices[i];
        const auto item_data = static_cast<LPARAM>(i);

        if (item.type == device_type_input || item.type == device_type_inout) {
            const auto index = SendMessageA(input, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.display_name));

            SendMessageA(input, CB_SETITEMDATA, index, item_data);

            if (item.id == input_id) {
                SendMessageA(input, CB_SETCURSEL, index, 0);
            }
        }

        if (item.type == device_type_output || item.type == device_type_inout) {
            const auto index = SendMessageA(output, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.display_name));

            SendMessageA(output, CB_SETITEMDATA, index, item_data);

            if (item.id == output_id) {
                SendMessageA(output, CB_SETCURSEL, index, 0);
            }
        }
    }
}

static void LoadSettings(std::string &output_id, std::string &input_id) {
    HKEY wdaKey;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\WineDirectALSA", 0, KEY_READ, &wdaKey) != ERROR_SUCCESS) {
        return;
    }

    std::array<char, device_id_size> output_id_arr{};
    std::array<char, device_id_size> input_id_arr{};

    DWORD output_length = output_id_arr.size();
    DWORD input_length = input_id_arr.size();

    if (RegQueryValueExA(wdaKey, "Output", nullptr, nullptr, reinterpret_cast<LPBYTE>(output_id_arr.data()),
        &output_length) != ERROR_SUCCESS) {
        RegCloseKey(wdaKey);

        return;
    }

    if (RegQueryValueExA(wdaKey, "Input", nullptr, nullptr, reinterpret_cast<LPBYTE>(input_id_arr.data()),
        &input_length) != ERROR_SUCCESS) {
        RegCloseKey(wdaKey);

        return;
    }

    output_id.assign(output_id_arr.data());
    input_id.assign(input_id_arr.data());

    RegCloseKey(wdaKey);
}

static INT_PTR CALLBACK ControlDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    const auto output = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE);
    const auto input = GetDlgItem(hWnd, IDC_INPUT_DEVICE);

    switch (uMsg) {
    case WM_INITDIALOG:
        {
            auto &state = *reinterpret_cast<DialogState*>(lParam);

            SetWindowLongPtrA(hWnd, GWLP_USERDATA, lParam);

            std::string output_id;
            std::string input_id = "(None)";

            LoadSettings(output_id, input_id);

            if (!RefreshDevices(state)) {
                MessageBoxA(hWnd, "Failed to enumerate ALSA devices.", "WineDirectALSA", MB_OK | MB_ICONERROR);
            }

            PopulateDeviceLists(output, input, state, output_id, input_id);

            return TRUE;
        }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            {
                const auto outcbi = SendMessageA(output, CB_GETCURSEL, 0, 0);
                const auto incbi = SendMessageA(input, CB_GETCURSEL, 0, 0);

                if (outcbi == CB_ERR || incbi == CB_ERR) {
                    MessageBoxA(hWnd, "Please select the input/output device first.", nullptr, MB_OK | MB_ICONERROR);

                    return TRUE;
                }

                const auto outi = SendMessageA(output, CB_GETITEMDATA, outcbi, 0);
                const auto ini = SendMessageA(input, CB_GETITEMDATA, incbi, 0);

                auto &state = *reinterpret_cast<DialogState*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
                auto input_value = std::string(ini == -2 ? "(None)" : state.devices[ini].id);
                auto output_value = std::string(state.devices[outi].id);

                HKEY wdaKey;
                bool error = false;

                auto code = RegCreateKeyExA(
                    HKEY_CURRENT_USER,
                    "SOFTWARE\\WineDirectALSA",
                    0,
                    nullptr,
                    0,
                    KEY_WRITE,
                    nullptr,
                    &wdaKey,
                    nullptr);

                if (code != ERROR_SUCCESS) {
                    error = true;

                    goto ok_msgbox;
                }

                code = RegSetValueExA(wdaKey, "Input", 0, REG_SZ, reinterpret_cast<const BYTE*>(input_value.c_str()),
                    static_cast<DWORD>(input_value.length() + 1));

                if (code != ERROR_SUCCESS) {
                    error = true;

                    RegCloseKey(wdaKey);

                    goto ok_msgbox;
                }

                code = RegSetValueExA(wdaKey, "Output", 0, REG_SZ, reinterpret_cast<const BYTE*>(output_value.c_str()),
                    static_cast<DWORD>(output_value.length() + 1));

                if (code != ERROR_SUCCESS) {
                    error = true;

                    RegCloseKey(wdaKey);

                    goto ok_msgbox;
                }

                RegCloseKey(wdaKey);

            ok_msgbox:
                if (!error) {
                    MessageBoxA(hWnd, "You must restart the program using the driver for the changes to take effect.", "Info",
                        MB_OK | MB_ICONINFORMATION);
                } else {
                    std::array<char, 1000> message{};

                    FormatMessageA(
                        FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS,
                        nullptr,
                        code,
                        0,
                        message.data(),
                        static_cast<DWORD>(message.size()),
                        nullptr);

                    MessageBoxA(hWnd, message.data(), nullptr, MB_OK | MB_ICONERROR);
                }

                SetWindowLongPtrA(hWnd, GWLP_USERDATA, 0);
                EndDialog(hWnd, IDOK);
                return TRUE;
            }
        case IDCANCEL:
            SetWindowLongPtrA(hWnd, GWLP_USERDATA, 0);
            EndDialog(hWnd, IDCANCEL);
            return TRUE;

        case IDC_REFRESH:
            {
                auto &state = *reinterpret_cast<DialogState*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));

                // state.devices를 갱신하기 전에 현재 ID를 복사
                const auto output_id = GetSelectedDeviceId(output, state);
                const auto input_id = GetSelectedDeviceId(input, state);

                if (!RefreshDevices(state)) {
                    MessageBoxA(hWnd, "Failed to refresh ALSA devices.", "WineDirectALSA", MB_OK | MB_ICONERROR);

                    return TRUE;
                }

                PopulateDeviceLists(output, input, state, output_id, input_id);

                return TRUE;
            }
        }

        break;
    }

    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, int) {
    DialogState state;

    DialogBoxParamA(hInstance, MAKEINTRESOURCEA(IDD_CONTROL_PANEL), nullptr, ControlDialogProc, reinterpret_cast<LPARAM>(&state));

    return 0;
}
