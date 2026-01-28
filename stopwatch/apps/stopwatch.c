#include "syslib.h"
#include "stdio.h"

#include "blockpixel.h"

#define WIDTH           39
#define HEIGHT          20

#define START_POS       2
#define BAR_LEN_LONG    2
#define BAR_LEN_SHORT   1

#define NUM_CELLS       6
#define CELL_WIDTH      (BAR_LEN_LONG + BAR_LEN_SHORT * 2 + 1)

#define NUM_TOP         ((HEIGHT - (BAR_LEN_LONG + BAR_LEN_SHORT) * 2) / 2)
#define NUM_BOTTOM      (HEIGHT - NUM_TOP)
#define FIRST_NUM_LEFT  ((WIDTH - CELL_WIDTH * NUM_CELLS) / 2)

struct stopwatch {
    uint64_t start_time;
    uint64_t offset_cs;

    uint8_t is_running;
    uint8_t mins;
    uint8_t secs;
    uint8_t centisecs;

    struct bp bp;
    uint8_t bp_buffer[WIDTH * HEIGHT];
};

struct digit_spec {
    uint8_t top_left;
    uint8_t top_center;
    uint8_t top_right;
    uint8_t center;
    uint8_t bottom_left;
    uint8_t bottom_center;
    uint8_t bottom_right;
};

static struct digit_spec specs[10] = {
    {1, 1, 1, 0, 1, 1, 1}, // 0
    {1, 0, 0, 0, 1, 0, 0}, // 1
    {0, 1, 1, 1, 1, 1, 0}, // 2
    {0, 1, 1, 1, 0, 1, 1}, // 3
    {1, 0, 1, 1, 0, 0, 1}, // 4
    {1, 1, 0, 1, 0, 1, 1}, // 5
    {1, 1, 0, 1, 1, 1, 1}, // 6
    {0, 1, 1, 0, 0, 0, 1}, // 7
    {1, 1, 1, 1, 1, 1, 1}, // 8
    {1, 1, 1, 1, 0, 1, 1}  // 9
};

void digit_draw(struct stopwatch *stopwatch, uint8_t digit, int cell_idx, uint8_t color) {
    struct digit_spec spec = specs[digit];

    int start_x = FIRST_NUM_LEFT + cell_idx * CELL_WIDTH + 1;

    if (spec.top_left) {
        for (int y = NUM_TOP; y <= NUM_TOP + BAR_LEN_LONG; y++) {
            bp_put(&stopwatch->bp, start_x, y, color, BP_LAZY);
        }
    }

    if (spec.top_center) {
        for (int x = start_x; x <= start_x + BAR_LEN_LONG; x++) {
            bp_put(&stopwatch->bp, x, NUM_TOP, color, BP_LAZY);
        }
    }

    if (spec.top_right) {
        for (int y = NUM_TOP; y <= NUM_TOP + BAR_LEN_LONG; y++) {
            bp_put(&stopwatch->bp, start_x + BAR_LEN_LONG, y, color, BP_LAZY);
        }
    }

    if (spec.center) {
        for (int x = start_x; x <= start_x + BAR_LEN_LONG; x++) {
            bp_put(&stopwatch->bp, x, NUM_TOP + BAR_LEN_LONG, color, BP_LAZY);
        }
    }

    if (spec.bottom_left) {
        for (int y = NUM_TOP + BAR_LEN_LONG; y <= NUM_TOP + 2 * BAR_LEN_LONG; y++) {
            bp_put(&stopwatch->bp, start_x, y, color, BP_LAZY);
        }
    }

    if (spec.bottom_center) {
        for (int x = start_x; x <= start_x + BAR_LEN_LONG; x++) {
            bp_put(&stopwatch->bp, x, NUM_TOP + 2 * BAR_LEN_LONG, color, BP_LAZY);
        }
    }

    if (spec.bottom_right) {
        for (int y = NUM_TOP + BAR_LEN_LONG; y <= NUM_TOP + 2 * BAR_LEN_LONG; y++) {
            bp_put(&stopwatch->bp, start_x + BAR_LEN_LONG, y, color, BP_LAZY);
        }
    }
}

void stopwatch_redraw(struct stopwatch *stopwatch) {
    // color background black
    for (int x = 0; x < WIDTH; x++)
        for (int y = 0; y < HEIGHT; y++)
            bp_put(&stopwatch->bp, x, y, ANSI_BLACK, BP_LAZY);
    
    uint8_t color = stopwatch->is_running ? ANSI_GREEN : ANSI_WHITE;
    
    digit_draw(stopwatch, stopwatch->mins / 10, 0, color);
    digit_draw(stopwatch, stopwatch->mins % 10, 1, color);
    digit_draw(stopwatch, stopwatch->secs / 10, 2, color);
    digit_draw(stopwatch, stopwatch->secs % 10, 3, color);
    digit_draw(stopwatch, stopwatch->centisecs / 10, 4, color);
    digit_draw(stopwatch, stopwatch->centisecs % 10, 5, color);

    bp_flush(&stopwatch->bp);
}

void main(void) {
    struct stopwatch stopwatch;
    stopwatch.is_running = 0;
    stopwatch.mins = 0;
    stopwatch.secs = 0;
    stopwatch.centisecs = 0;
    stopwatch.start_time = 0;
    stopwatch.offset_cs = 0;
    
    bp_init(&stopwatch.bp, 0, 1, WIDTH, HEIGHT, stopwatch.bp_buffer);
    // stopwatch_redraw(&stopwatch);

    while (1) {
        int c = user_get(0);

        while (c == USER_GET_LOST_FOCUS) {
            c = user_get(1);
        }
        
        if (c == 'q') break;
        else if (c == 's') {
            stopwatch.is_running = !stopwatch.is_running;
            stopwatch.start_time = user_gettime();

            if (!stopwatch.is_running) {
                stopwatch.offset_cs = stopwatch.mins * 60 * 100 + \
                    stopwatch.secs * 100 + stopwatch.centisecs;
            }

            stopwatch.mins = 0;
            stopwatch.secs = 0;
            stopwatch.centisecs = 0;
        }
        else if (c == 'r') {
            stopwatch.offset_cs = 0;

            stopwatch.mins = 0;
            stopwatch.secs = 0;
            stopwatch.centisecs = 0;

            stopwatch.start_time = user_gettime();

            stopwatch_redraw(&stopwatch);
        }

        if (stopwatch.is_running) {
            uint64_t curr_time = user_gettime();
            uint64_t elapsed_cs = (curr_time - stopwatch.start_time) / 10000000;

            uint64_t total_cs = stopwatch.offset_cs + elapsed_cs;
            stopwatch.centisecs = total_cs % 100;

            uint64_t total_secs = total_cs / 100;
            stopwatch.secs = total_secs % 60;

            uint64_t total_mins = total_secs / 60;
            stopwatch.mins = total_mins;
            
            stopwatch_redraw(&stopwatch);
        }
    }
}