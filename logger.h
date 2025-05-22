#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h> // For Serial, millis, etc.
#include <stdarg.h>  // For va_list, vsnprintf

// Function declaration for timestamped logging
void ts_log_printf(const char* format, ...);

#if 0 // Example of a hex dump utility, can be useful for debugging BLE data
void hexDump(const uint8_t* data, size_t length);
#endif

#endif // LOGGER_H