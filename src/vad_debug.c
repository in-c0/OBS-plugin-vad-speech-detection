#include <stdio.h>
#include <stdarg.h>
#include <obs-module.h>

void vad_write_debug_text_throttled(const char *format, ...)
{
    static uint64_t last_log_time = 0;
    uint64_t now = os_gettime_ns();

    if ((now - last_log_time) < 200000000ULL)  // 200ms throttle
        return;

    last_log_time = now;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    blog(LOG_INFO, "[VAD] %s", buffer);
}
