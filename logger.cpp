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

void logDebugMessage(char* message, int sleepDurationInSeconds) {
    if (shouldDelayOperations) {
        std::cout << message << std::endl;
        if (sleepDurationInSeconds != 0) {
            sleep(sleepDurationInSeconds);
        }
    }
}

void debug_printf(const char *format, ...) {
    va_list argptr;
    va_start(argptr, format);

    if (shouldDelayOperations)
        vprintf(format, argptr);

    va_end(argptr);
}
