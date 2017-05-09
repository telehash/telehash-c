#ifndef util_sys_h
#define util_sys_h

typedef uint32_t at_t;

// returns a number that increments in seconds for comparison (epoch or just since boot)
at_t util_sys_seconds();

// number of milliseconds since given epoch seconds value
unsigned long long util_sys_ms(long epoch);

unsigned short util_sys_short(unsigned short x);
unsigned long util_sys_long(unsigned long x);

// use the platform's best RNG
void util_sys_random_init(void);
long util_sys_random(void);

// -1 toggles debug, 0 disable, 1 enable
void util_sys_logging(int enabled);

// returns NULL for convenient return logging
void *util_sys_log(uint8_t level, const char *file, int line, const char *function, const char * format, ...);

// use syslog levels https://en.wikipedia.org/wiki/Syslog#Severity_level
#define LOG_LEVEL(level, fmt, ...) util_sys_log(level, __FILE__, __LINE__, __func__, fmt, ## __VA_ARGS__)

// default LOG is DEBUG level and compile-time optional
#ifdef NOLOG
#define LOG(...) NULL
#define LOG_DEBUG LOG
#define LOG_INFO LOG
#define LOG_WARN LOG
#define LOG_ERROR LOG
#define LOG_CRAZY LOG
#else
#define LOG(fmt, ...) util_sys_log(7, __FILE__, __LINE__, __func__, fmt, ## __VA_ARGS__)
#define LOG_DEBUG LOG
#define LOG_INFO(fmt, ...) util_sys_log(6, __FILE__, __LINE__, __func__, fmt, ## __VA_ARGS__)
#define LOG_WARN(fmt, ...) util_sys_log(4, __FILE__, __LINE__, __func__, fmt, ## __VA_ARGS__)
#define LOG_ERROR(fmt, ...) util_sys_log(3, __FILE__, __LINE__, __func__, fmt, ## __VA_ARGS__)
#define LOG_CRAZY(fmt, ...) util_sys_log(8, __FILE__, __LINE__, __func__, fmt, ## __VA_ARGS__)
#endif

// most things just need these


#endif
