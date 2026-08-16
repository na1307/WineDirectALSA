module;

#include "wine_unixlib.h"

module wda;

extern "C" {
bool WDA_GetDevices(devices *devices) {
    return devices != nullptr && linux_call(get_devices, devices) == 0;
}
}
