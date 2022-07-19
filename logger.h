
#ifndef BULLETIN_BOARD_LOGGER_H
#define BULLETIN_BOARD_LOGGER_H

void setDebuggingPreference(bool isDelayEnabled);

bool getDebuggingPreference();

void logDebugMessage(char* message, int sleepDurationInSeconds = 0);

void debug_printf(const char *format, ...);

#endif //BULLETIN_BOARD_LOGGER_H
