#pragma once

#include "linux_functions.h"

#if __cplusplus
extern "C" {
#endif

long init_linux_call(void);

long linux_call(linux_functions, void *);

#if __cplusplus
}
#endif
