//
// Created by jianp on 2025/11/7.
//

#ifndef MUDUOSELF_LOGGER_H
#define MUDUOSELF_LOGGER_H

#include <string.h>
#include <string>
#include "noncopyable.h"

#define  LOG_INFO(logmsgFormat, ...) \
    do                               \
    {                                \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(INFO);    \
        char buf[1024] = {0};        \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);\
    } while(0)

#define  LOG_ERROR(logmsgFormat, ...) \
    do                               \
    {                                \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(ERROR);    \
        char buf[1024] = {0};        \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);\
    } while(0)

#define  LOG_FATAL(logmsgFormat, ...) \
    do                               \
    {                                \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(FATAL);    \
        char buf[1024] = {0};        \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);\
    } while(0)


#ifdef MUDEBUG
#define  LOG_DEBUG(logmsgFormat, ...) \
    do                               \
    {                                \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(DEBUG);    \
        char buf[1024] = {0};        \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);\
    } while(0)
#else
    #define LOG_DEBUG(logmsgFormat, ...)
#endif

enum LogLevel {
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

class Logger : noncopyable {
public:
    static Logger& instance();

    void setLogLevel(int level);

    void log(std::string msg);
private:
    int logLevel_;
    Logger() {}
};


#endif //MUDUOSELF_LOGGER_H
