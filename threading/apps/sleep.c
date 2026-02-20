#include "syslib.h"
#include "stdio.h"

void main(void) {
    uint64_t sleep_s = 10;
    printf("Enter sleep for %u secs...\n", sleep_s);

    uint64_t sleep_ns = sleep_s * 1000000000;

    uint64_t curr = user_gettime();
    uint64_t deadline = curr + sleep_ns;
    printf("Now (ns): %u, Deadline (ns): %u\n", curr, deadline);

    user_sleep(deadline);

    printf("Sleep finished: %u\n", user_gettime());
}