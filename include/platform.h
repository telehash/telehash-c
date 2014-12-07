#ifndef platform_h
#define platform_h

// returns a number that increments in seconds for comparison (epoch or just since boot)
unsigned long platform_seconds();

// number of milliseconds since given epoch seconds value
unsigned long long platform_ms(unsigned long epoch);

unsigned short platform_short(unsigned short x);

// use the platform's best RNG
void platform_random_init(void);
long platform_random(void);

// -1 toggles debug, 0 disable, 1 enable
void platform_logging(int enabled);

// returns NULL for convenient return logging
void *platform_log(const char *file, int line, const char *function, const char * format, ...);

#ifdef NOLOG
#define LOG(...) NULL
#else
#define LOG(fmt, ...) platform_log(__FILE__, __LINE__, __func__, fmt, ## __VA_ARGS__)
#endif


#endif