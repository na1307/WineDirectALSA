#include <Windows.h>
#include <unixlib.h>

#include "wine_unixlib.h"

long init_linux_call(void) {
    return __wine_init_unix_call();
}

long linux_call(const linux_functions code, void *args) {
    return WINE_UNIX_CALL(code, args);
}
