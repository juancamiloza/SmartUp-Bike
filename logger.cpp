#include "logger.h" // Include its own header

// Definition of the timestamped logging function
void ts_log_printf(const char* format, ...) {
    char loc_buf[256]; // Buffer for the final formatted string with timestamp
    char temp_buf[200]; // Buffer for the message part (without timestamp)
    va_list arg;        // For handling variable arguments

    // Initialize va_list and format the message part
    va_start(arg, format);
    vsnprintf(temp_buf, sizeof(temp_buf), format, arg);
    va_end(arg);

    // Prepend timestamp to the message
    // millis() gives unsigned long, format specifier %lu or cast to double for %f.
    // Using %08.3f which expects a double.
    snprintf(loc_buf, sizeof(loc_buf), "[%08.3fs] %s", (double)millis() / 1000.0, temp_buf);

    // Print to Serial and flush to ensure it's sent immediately
    Serial.println(loc_buf);
    Serial.flush(); // Ensures data is sent, useful for debugging before potential crashes
}


#if 0 // Implementation for hexDump if enabled in logger.h
void hexDump(const uint8_t* data, size_t length) {
    char buf[128]; // Max line length for hex dump
    String line;
    for (size_t i = 0; i < length; i += 16) {
        line = "";
        // Offset
        sprintf(buf, "%04X: ", (unsigned int)i);
        line += buf;
        // Hex bytes
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < length) {
                sprintf(buf, "%02X ", data[i + j]);
            } else {
                sprintf(buf, "   "); // Pad if line is short
            }
            line += buf;
        }
        line += " ";
        // ASCII representation
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < length) {
                if (isprint(data[i + j])) {
                    sprintf(buf, "%c", data[i + j]);
                } else {
                    sprintf(buf, ".");
                }
                line += buf;
            }
        }
        ts_log_printf("  %s", line.c_str()); // Use ts_log_printf for consistent output
    }
}
#endif