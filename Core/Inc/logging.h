#pragma once
#include "console.h"





/*
* Default logging level
*/
#define LOG_LEVEL_PANIC 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_NOTICE 3
#define LOG_LEVEL_INFO 4
#define LOG_LEVEL_DEBUG 5

#define LOG_LEVEL LOG_LEVEL_DEBUG





namespace LOGGING {

const uint8_t LOG_QUEUE_DEPTH = 8;
const uint16_t MAX_LOG_SIZE = 80;
const uint8_t MAX_TAG_SIZE = 16;
const uint8_t MAX_LOG_LEVEL = 6;

enum {LOGGING_PANIC=0, LOGGING_ERROR, LOGGING_WARN, LOGGING_NOTICE, LOGGING_INFO, LOGGING_DEBUG};


typedef struct logItem {
    uint8_t level;
    uint32_t timestamp;
    uint32_t line;
    char tag[MAX_TAG_SIZE];
    char log_message[MAX_LOG_SIZE];
} logItem;



class Logging {
    public:
    void log(const char *tag, uint8_t level, uint32_t line, const char *format, ...);
    void setup(void);
    void init(void);
    void loop(void);
    void panic(const char *tag, uint32_t line, const char *format, ...);



    protected:
    void _xmit_logitem(const char *tag, uint8_t level, uint32_t timestamp, const char *str, uint32_t line);

    osMessageQueueId_t _queue_logging_handle;
    bool _queue_overflow;
};

} /* End Namespace LOGGING */

extern LOGGING::Logging Logger;

#define LOG_PANIC(tag, format, ...) Logger.panic(tag, __LINE__, format __VA_OPT__(,) __VA_ARGS__)

#define LOG_ERROR(tag, format, ...) Logger.log(tag, LOGGING::LOGGING_ERROR, __LINE__, format __VA_OPT__(,) __VA_ARGS__)
#if LOG_LEVEL_WARN <= LOG_LEVEL
#define LOG_WARN(tag, format, ...) Logger.log(tag, LOGGING::LOGGING_WARN, __LINE__, format __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_WARN(tag, format, ...)
#endif
#if LOG_LEVEL_NOTICE <= LOG_LEVEL
#define LOG_NOTICE(tag, format, ...) Logger.log(tag, LOGGING::LOGGING_NOTICE, __LINE__, format __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_NOTICE(tag, format, ...)
#endif
#if LOG_LEVEL_INFO <= LOG_LEVEL
#define LOG_INFO(tag, format, ...) Logger.log(tag, LOGGING::LOGGING_INFO, __LINE__, format __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_INFO(tag, format, ...)
#endif
#if LOG_LEVEL_DEBUG <= LOG_LEVEL
#define LOG_DEBUG(tag, format, ...) Logger.log(tag, LOGGING::LOGGING_DEBUG, __LINE__, format __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_DEBUG(tag, format, ...)
#endif







