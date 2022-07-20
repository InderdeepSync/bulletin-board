#include <iostream>
#include <unistd.h>
#include <stdarg.h>

#include "logger.h"

bool shouldDelayOperations;

void setDebuggingPreference(bool isDelayEnabled) {
    shouldDelayOperations = isDelayEnabled;
}

bool getDebuggingPreference() {
    return shouldDelayOperations;
}

void debug_usleep(useconds_t usec) {
    if (shouldDelayOperations) {
        usleep(usec);
    }
}

void debug_printf(const char *format, ...) {
    va_list argptr;
    va_start(argptr, format);

    if (shouldDelayOperations)
        vprintf(format, argptr);

    va_end(argptr);
}
