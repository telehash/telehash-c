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
void *util_sys_log(const char *file, int line, const char *function, const char * format, ...);

#ifdef NOLOG
#define LOG(...) NULL
#else
#define LOG(fmt, ...) util_sys_log(__FILE__, __LINE__, __func__, fmt, ## __VA_ARGS__)
#endif


#endif
