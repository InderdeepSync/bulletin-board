
#ifndef BULLETIN_BOARD_LOGGER_H
#define BULLETIN_BOARD_LOGGER_H

void setDebuggingPreference(bool shouldDebug);

bool getDebuggingPreference();

void debug_printf(const char *format, ...);

void debug_sleep(int sleepDurationInSeconds);

#endif //BULLETIN_BOARD_LOGGER_H
