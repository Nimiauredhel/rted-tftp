#include "common.h"

static bool random_was_seeded = false;

bool should_terminate = false;

int random_range(int min, int max)
{
    if (!random_was_seeded)
    {
        srand(time(NULL) + getpid());
        random_was_seeded = true;
    }

    int random_number = rand();
    random_number = (random_number % (max - min + 1)) + min;
    return random_number;
}

float seconds_since_clock(struct timespec start_clock)
{
    struct timespec now_clock;
    clock_gettime(CLOCK_MONOTONIC, &now_clock);
    float elapsed_float = (now_clock.tv_nsec - start_clock.tv_nsec) / 1000000000.0;
    elapsed_float += (now_clock.tv_sec - start_clock.tv_sec);
    return elapsed_float;
}
