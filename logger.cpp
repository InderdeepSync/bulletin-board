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

void debug_sleep(int sleepDurationInSeconds) {
    struct timespec tim, tim2;
    tim.tv_sec = sleepDurationInSeconds;
    tim.tv_nsec = 0L;

    if (shouldDelayOperations) {
        nanosleep(&tim , &tim2);
    }
}

void debug_printf(const char *format, ...) {
    va_list argptr;
    va_start(argptr, format);

    if (shouldDelayOperations)
        vprintf(format, argptr);

    va_end(argptr);
}
