#include <stdlib.h>
#include <stdnoreturn.h>
#include <errno.h>
#include <stdio.h>

noreturn void print_error_end_exit(char * error) {
    if (errno != 0)
        perror(error);
    else
        puts(error);
    exit(-1);
}
