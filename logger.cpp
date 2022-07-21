#include <iostream>
#include <unistd.h>
#include <stdarg.h>

#include "logger.h"

bool shouldDelayOperations;

pthread_mutex_t loggerMutex = PTHREAD_MUTEX_INITIALIZER;

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

    if (shouldDelayOperations) {
        pthread_mutex_lock(&loggerMutex);
        vprintf(format, argptr);
        pthread_mutex_unlock(&loggerMutex);
    }

    va_end(argptr);
}
