#include "syslib.h"
#include "stdio.h"

void main(void) {
    int c = user_get(0);
    while (c == USER_GET_LOST_FOCUS) {
        c = user_get(1);
    }

    while (!(c >= '0' && c <= '9'))
        c = user_get(1);
    
    int secs = c - '0';
    printf("Enter sleep for %d secs...\n", secs);

    uint64_t curr = user_gettime();
    uint64_t deadline = curr + secs * 1000000000U;

    user_sleep(deadline);

    printf("now i am back!!!!!!!!!!\n");
}