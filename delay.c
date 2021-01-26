#define _POSIX_C_SOURCE 200809L
#include <stdnoreturn.h>
#include <time.h>

extern noreturn void print_error_end_exit(char * error);

void delay(int ms) {
    struct timespec req = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000,
    };
    if (nanosleep(&req, NULL)) {
        print_error_end_exit("sleep error");
    }
}
