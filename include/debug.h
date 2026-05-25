#pragma once
#include <Arduino.h>
#include <time.h>

// Set DEBUG_LEVEL via build flag: -DDEBUG_LEVEL=3
// 1=ERROR  2=WARN  3=INFO  4=VERBOSE
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 3
#endif

inline void debugPrintPrefix(const char *level)
{
    const time_t now = time(nullptr);
    if (now > 1000000000L)
    {
        struct tm local = {};
        char timestamp[20];
        localtime_r(&now, &local);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local);
        Serial.printf("[%s] [%s] ", timestamp, level);
        return;
    }

    Serial.printf("[boot +%lus] [%s] ", millis() / 1000UL, level);
}

#if DEBUG_LEVEL >= 1
#define DBG_ERROR(fmt, ...) do { debugPrintPrefix("ERR"); Serial.printf(fmt "\n", ##__VA_ARGS__); } while (0)
#else
#define DBG_ERROR(fmt, ...) do {} while (0)
#endif

#if DEBUG_LEVEL >= 2
#define DBG_WARN(fmt, ...)  do { debugPrintPrefix("WARN"); Serial.printf(fmt "\n", ##__VA_ARGS__); } while (0)
#else
#define DBG_WARN(fmt, ...)  do {} while (0)
#endif

#if DEBUG_LEVEL >= 3
#define DBG_INFO(fmt, ...)  do { debugPrintPrefix("INFO"); Serial.printf(fmt "\n", ##__VA_ARGS__); } while (0)
#else
#define DBG_INFO(fmt, ...)  do {} while (0)
#endif

#if DEBUG_LEVEL >= 4
#define DBG_VERBOSE(fmt, ...) do { debugPrintPrefix("VERB"); Serial.printf(fmt "\n", ##__VA_ARGS__); } while (0)
#else
#define DBG_VERBOSE(fmt, ...) do {} while (0)
#endif
