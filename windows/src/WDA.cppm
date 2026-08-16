module;

#include "linux_functions.h"

export module wda;

extern "C" {
export bool WDA_GetDevices(devices *devices);
}
